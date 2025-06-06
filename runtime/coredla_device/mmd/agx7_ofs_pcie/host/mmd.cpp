// (c) 1992-2024 Intel Corporation.
// Intel, the Intel logo, Intel, MegaCore, NIOS II, Quartus and TalkBack words
// and logos are trademarks of Intel Corporation or its subsidiaries in the U.S.
// and/or other countries. Other marks and brands may be claimed as the property
// of others. See Trademarks on intel.com for full list of Intel trademarks or
// the Trademarks & Brands Names Database (if Intel) or See www.Intel.com/legal (if Altera)
// Your use of Intel Corporation's design tools, logic functions and other
// software and tools, and its AMPP partner logic functions, and any output
// files any of the foregoing (including device programming or simulation
// files), and any associated documentation or information are expressly subject
// to the terms and conditions of the Altera Program License Subscription
// Agreement, Intel MegaCore Function License Agreement, or other applicable
// license agreement, including, without limitation, that your use is for the
// sole purpose of programming logic devices manufactured by Intel and sold by
// Intel or its authorized distributors.  Please refer to the applicable
// agreement for further details.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include <linux/mman.h>
#include <sys/mman.h>

// On some systems MAP_HUGE_2MB is not defined. It should be defined for all
// platforms that DCP supports, but we also want ability to compile MMD on
// CentOS 6 systems.
#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif

#ifndef MAP_HUGE_1GB
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#endif

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <unordered_map>
#include <vector>
#ifdef DLA_MMD
#include <chrono>
#include <thread>
#endif

#include "aocl_mmd.h"
#include "mmd_device.h"

bool diagnose = 0;

/** If the MMD is loaded dynamically, destructors in the MMD will execute before
 *  the destructors in the runtime upon program termination. The DeviceMapManager
 *  guards accesses to the device/handle maps to make sure the runtime doesn't
 *  get to reference them after MMD destructors have been called. Destructor
 *  makes sure that all devices are closed at program termination regardless of
 *  what the runtime does. Implemented as a singleton.
 */
class DeviceMapManager final {
 public:
  /** C++ std map data structure to keep track of
   *  object id -> handle and handle -> device
   */
  typedef std::map<int, Device *> t_handle_to_dev_map;
  typedef std::map<uint64_t, int> t_id_to_handle_map;

  static const int SUCCESS = 0;
  static const int FAILURE = -1;

  /** Returns handle and device pointer to the device with the specified name
   *  Creates a new entry for this device if it doesn't already exist
   *  Return 0 on success, -1 on failure
   */
  int get_or_create_device(const char *board_name, int *handle, Device **device);

  /** Return obj id based on ASP name.*/
  uint64_t id_from_name(const char *board_name);

  /** Return MMD handle based on obj id. Returned value is negative if board
   *   doesn't exist
   */
  inline int handle_from_id(uint64_t obj_id);

  /** Return pointer to device based on MMD handle. Returned value is null
   *   if board doesn't exist
   */
  Device *device_from_handle(int handle);

  /** Closes specified device if it exists */
  void close_device_if_exists(int handle);

  /* Returns a reference to the class singleton */
  static DeviceMapManager &get_instance() {
    static DeviceMapManager instance;
    return instance;
  }

  DeviceMapManager(DeviceMapManager const &) = delete;
  void operator=(DeviceMapManager const &) = delete;
  ~DeviceMapManager() {
    // delete all allocated Device* entries
    while (handle_to_dev_map->size() > 0) {
      int handle = handle_to_dev_map->begin()->first;
      aocl_mmd_close(handle);
#ifdef SIM
      std::cout << "# mmd.cpp: When destroying DeviceMapManager in ASE, assume it worked.\n";
      break;
#endif
      MMD_DEBUG("DEBUG LOG : In DeviceMapManager destructor, closing device with handle %d \n", handle);
    }
    delete handle_to_dev_map;
    delete id_to_handle_map;
    handle_to_dev_map = nullptr;
    id_to_handle_map = nullptr;
  }

 private:
  DeviceMapManager() {
    handle_to_dev_map = new t_handle_to_dev_map();
    id_to_handle_map = new t_id_to_handle_map();

    MMD_DEBUG("DEBUG LOG : Constructing DeviceMapManager object\n");
  }
  t_handle_to_dev_map *handle_to_dev_map = nullptr;
  t_id_to_handle_map *id_to_handle_map = nullptr;
};
static DeviceMapManager &device_manager = DeviceMapManager::get_instance();

/** Returns handle and device pointer to the device with the specified name
 *  Creates a new entry for this device if it doesn't already exist
 *  Return 0 on success, -1 on failure
 */
int DeviceMapManager::get_or_create_device(const char *board_name, int *handle, Device **device) {
  int _handle = MMD_INVALID_PARAM;
  Device *_device = nullptr;

  if (id_to_handle_map == nullptr || handle_to_dev_map == nullptr) {
    MMD_DEBUG(
        "DEBUG LOG : Failure in DeviceMapManager::get_or_create_device,id_to_handle_map or handle_to_dev_map is "
        "NULL\n");
    return DeviceMapManager::FAILURE;
  }

  uint64_t obj_id = id_from_name(board_name);
  if (!obj_id) {
    MMD_DEBUG("DEBUG LOG : Failure in DeviceMapManager::get_or_create_device. obj_id : %ld \n", obj_id);
    return false;
  }
  if (id_to_handle_map->count(obj_id) == 0) {
    try {
      _device = new Device(obj_id);
      _handle = _device->get_mmd_handle();
      id_to_handle_map->insert({obj_id, _handle});
      handle_to_dev_map->insert({_handle, _device});
    } catch (std::runtime_error &e) {
      MMD_DEBUG("DEBUG LOG : Failure in DeviceMapManager::get_or_create_device %s\n", e.what());
      delete _device;
      return DeviceMapManager::FAILURE;
    }
    MMD_DEBUG("DEBUG LOG : Success in creating new device object handle : %d \n", _handle);
  } else {
    _handle = id_to_handle_map->at(obj_id);
    _device = handle_to_dev_map->at(_handle);
    MMD_DEBUG("DEBUG LOG : Success in retrieving device metadata(handle , object) , handle : %d\n", _handle);
  }

  (*handle) = _handle;
  (*device) = _device;

  MMD_DEBUG("DEBUG LOG : Success in creating new device object , handle : %d\n", _handle);
  return DeviceMapManager::SUCCESS;
}

/** Return obj id based on ASP name.*/
uint64_t DeviceMapManager::id_from_name(const char *board_name) {
  uint64_t obj_id = 0;
  if (Device::parse_board_name(board_name, obj_id)) {
    MMD_DEBUG("DEBUG LOG : Success in retrieving object id from board name\n");
    return obj_id;
  } else {
    MMD_DEBUG("DEBUG LOG : Failed to retrieve object id from board name\n");
    return 0;
  }
}

/** Return MMD handle based on obj id. Returned value is negative if board
 *  doesn't exist
 */
inline int DeviceMapManager::handle_from_id(uint64_t obj_id) {
  int handle = MMD_INVALID_PARAM;
  if (id_to_handle_map) {
    auto it = id_to_handle_map->find(obj_id);
    if (it != id_to_handle_map->end()) {
      handle = it->second;
    }
    MMD_DEBUG("DEBUG LOG : Success in retrieving handle from object id. handle : %d \n", handle);
  } else {
    MMD_DEBUG("DEBUG LOG : Failed to retrieve handle from object id \n");
  }
  return handle;
}

/** Return pointer to device based on MMD handle. Returned value is null
 *  if board doesn't exist
 */
Device *DeviceMapManager::device_from_handle(int handle) {
  Device *dev = nullptr;
  if (handle_to_dev_map) {
    auto it = handle_to_dev_map->find(handle);
    if (it != handle_to_dev_map->end()) {
      return it->second;
    }
    MMD_DEBUG("DEBUG LOG : Success in retrieving device from handle. handle : %d \n", handle);
  } else {
    MMD_DEBUG("DEBUG LOG : Failed to retrieve device from handle\n");
  }
  return dev;
}

/** Closes specified device if it exists */
void DeviceMapManager::close_device_if_exists(int handle) {
  if (handle_to_dev_map) {
    if (handle_to_dev_map->count(handle) > 0) {
      Device *dev = handle_to_dev_map->at(handle);
      uint64_t obj_id = dev->get_fpga_obj_id();
      delete dev;

      handle_to_dev_map->erase(handle);
      id_to_handle_map->erase(obj_id);
      MMD_DEBUG("DEBUG LOG : Closing device with handle : %d\n", handle);
    } else {
      MMD_DEBUG("DEBUG LOG : Nothing to close. Device with handle : %d already closed\n", handle);
    }
  } else {
    MMD_DEBUG("DEBUG LOG : Error, no handle to device map entry found for handle : %d \n", handle);
  }
}

/** Interface for checking if AFU has ASP loaded */
bool mmd_asp_loaded(const char *name) {
  uint64_t obj_id = device_manager.id_from_name(name);
  if (!obj_id) {
    MMD_DEBUG("DEBUG LOG : Error, no object id found for board : %s \n", name);
    return false;
  }

  int handle = device_manager.handle_from_id(obj_id);
  if (handle > 0) {
    Device *dev = device_manager.device_from_handle(handle);
    if (dev) {
      MMD_DEBUG("DEBUG LOG : ASP loaded for handle : %d \n", handle);
      return dev->asp_loaded();
    } else {
      MMD_DEBUG("DEBUG LOG : ASP not loaded for handle : %d \n", handle);
      return false;
    }
  } else {
    bool asp_loaded = false;
    try {
      Device dev(obj_id);
      asp_loaded = dev.asp_loaded();
    } catch (std::runtime_error &e) {
      MMD_DEBUG("DEBUG LOG : ASP not loaded for handle : %d , %s\n", handle, e.what());
      return false;
    }

    MMD_DEBUG("DEBUG LOG : ASP loaded : %d (0 - not loaded , 1 - loaded) for handle : %d \n", asp_loaded, handle);
    return asp_loaded;
  }
}

/** Function called as part of aocl_mmd_get_offline_info()
 *  to determine number of baords in system
 */
static unsigned int get_offline_num_acl_boards(const char *asp_uuid) {
  bool asp_only = true;
  fpga_guid guid;
  fpga_result res = FPGA_OK;
  uint32_t num_matches = 0;
  bool ret_err = false;
  fpga_properties filter = NULL;

  if (uuid_parse(asp_uuid, guid) < 0) {
    MMD_DEBUG("Error parsing guid '%s'\n", asp_uuid);
    ret_err = true;
    goto out;
  }

  res = fpgaGetProperties(NULL, &filter);
  if (res != FPGA_OK) {
    MMD_DEBUG("Error creating properties object: %s\n", fpgaErrStr(res));
    ret_err = true;
    goto out;
  }

  if (asp_only) {
    res = fpgaPropertiesSetGUID(filter, guid);
    if (res != FPGA_OK) {
      MMD_DEBUG("Error setting GUID: %s\n", fpgaErrStr(res));
      ret_err = true;
      goto out;
    }
  }

  res = fpgaPropertiesSetObjectType(filter, FPGA_ACCELERATOR);
  if (res != FPGA_OK) {
    MMD_DEBUG("Error setting object type: %s\n", fpgaErrStr(res));
    ret_err = true;
    goto out;
  }

  res = fpgaEnumerate(&filter, 1, NULL, 0, &num_matches);
  if (res != FPGA_OK) {
    MMD_DEBUG("Error enumerating AFCs: %s\n", fpgaErrStr(res));
    ret_err = true;
    goto out;
  }

out:
  if (filter) fpgaDestroyProperties(&filter);

  if (ret_err) {
    return MMD_AOCL_ERR;
  } else {
    return num_matches;
  }
}

/** Function called as part of aocl_mmd_get_offline_info()
 *  to determine names of boards in the system
 */
static bool get_offline_board_names(std::string &boards, bool asp_only = true) {
  boards = "dla_agx7_ofs_board";
  return true;
}

// Macros used for acol_mmd_get_offline_info and aocl_mmd_get_info
#define RESULT_INT(X)                                  \
  {                                                    \
    *((int *)param_value) = X;                         \
    if (param_size_ret) *param_size_ret = sizeof(int); \
  }
#define RESULT_SIZE_T(X)                                  \
  {                                                       \
    *((size_t *)param_value) = X;                         \
    if (param_size_ret) *param_size_ret = sizeof(size_t); \
  }

#define RESULT_STR(X)                                                        \
  do {                                                                       \
    unsigned Xlen = strnlen(X, 4096) + 1;                                    \
    unsigned Xcpylen = (param_value_size <= Xlen) ? param_value_size : Xlen; \
    memcpy((void *)param_value, X, Xcpylen);                                 \
    if (param_size_ret) *param_size_ret = Xcpylen;                           \
  } while (0)

/** Get information about the board using the enum aocl_mmd_offline_info_t for
 *  offline info (called without a handle), and the enum aocl_mmd_info_t for
 *  info specific to a certain board.
 *  Arguments:
 *
 *    requested_info_id - a value from the aocl_mmd_offline_info_t enum
 *
 *    param_value_size - size of the param_value field in bytes. This should
 *      match the size of the return type expected as indicated in the enum
 *      definition.
 *
 *    param_value - pointer to the variable that will receive the returned info
 *
 *    param_size_ret - receives the number of bytes of data actually returned
 *
 *  Returns: a negative value to indicate error.
 */

// From DLA perspective, only AOCL_MMD_BOARD_NAMES info we care
int aocl_mmd_get_offline_info(aocl_mmd_offline_info_t requested_info_id,
                              size_t param_value_size,
                              void *param_value,
                              size_t *param_size_ret) {
  /** aocl_mmd_get_offline_info can be called many times by the runtime
   *  and it is expensive to query the system.  Only compute values first
   *  time aocl_mmd_get_offline_info called future iterations use saved results
   */
  static bool initialized = false;
  static int mem_type_info;
  static unsigned int num_acl_boards;
  static std::string boards;
  static bool success;

  if (!initialized) {
    mem_type_info = (int)AOCL_MMD_PHYSICAL_MEMORY;
    num_acl_boards = get_offline_num_acl_boards(I_DK_AFU_ID);
    success = get_offline_board_names(boards, true);
    initialized = true;
  }

  switch (requested_info_id) {
    case AOCL_MMD_VERSION:
      RESULT_STR(AOCL_MMD_VERSION_STRING);
      break;
    case AOCL_MMD_NUM_BOARDS: {
      RESULT_INT(num_acl_boards);
      break;
    }
    case AOCL_MMD_VENDOR_NAME:
      RESULT_STR("Intel Corp");
      break;
    case AOCL_MMD_BOARD_NAMES: {
      if (success) {
        RESULT_STR(boards.c_str());
      } else {
        return MMD_AOCL_ERR;
      }
      break;
    }
    case AOCL_MMD_VENDOR_ID:
      RESULT_INT(0);
      break;
    case AOCL_MMD_USES_YIELD:
      RESULT_INT(0);
      break;
    case AOCL_MMD_MEM_TYPES_SUPPORTED:
      RESULT_INT(mem_type_info);
      break;
  }

  return 0;
}

/** Get information about the board using the enum aocl_mmd_info_t for
 *  info specific to a certain board.
 *  Arguments:
 *
 *  requested_info_id - a value from the aocl_mmd_info_t enum
 *
 *  param_value_size - size of the param_value field in bytes. This should
 *    match the size of the return type expected as indicated in the enum
 *    definition. For example, the AOCL_MMD_TEMPERATURE returns a float, so
 *    the param_value_size should be set to sizeof(float) and you should
 *    expect the same number of bytes returned in param_size_ret.
 *
 *  param_value - pointer to the variable that will receive the returned info
 *
 *  param_size_ret - receives the number of bytes of data actually returned
 *
 *  Returns: a negative value to indicate error.
 */
int aocl_mmd_get_info(
    int handle, aocl_mmd_info_t requested_info_id, size_t param_value_size, void *param_value, size_t *param_size_ret) {
  MMD_DEBUG("DEBUG LOG : called aocl_mmd_get_info\n");
  Device *dev = device_manager.device_from_handle(handle);
  if (dev == NULL) return 0;

  assert(param_value);
  switch (requested_info_id) {
    case AOCL_MMD_BOARD_NAME: {
      std::ostringstream board_name;
      board_name << "Intel OFS Platform"
                 << " (" << dev->get_dev_name() << ")";
      RESULT_STR(board_name.str().c_str());
      break;
    }
    case AOCL_MMD_NUM_KERNEL_INTERFACES:
      RESULT_INT(1);
      break;
    case AOCL_MMD_KERNEL_INTERFACES:
      RESULT_INT(AOCL_MMD_KERNEL);
      break;
#ifdef SIM
    case AOCL_MMD_PLL_INTERFACES:
      RESULT_INT(-1);
      break;
#else
    case AOCL_MMD_PLL_INTERFACES:
      RESULT_INT(-1);
      break;
#endif
    case AOCL_MMD_MEMORY_INTERFACE:
      RESULT_INT(AOCL_MMD_MEMORY);
      break;
    case AOCL_MMD_PCIE_INFO: {
      RESULT_STR(dev->get_bdf().c_str());
      break;
    }
    case AOCL_MMD_BOARD_UNIQUE_ID:
      RESULT_INT(0);
      break;
    case AOCL_MMD_TEMPERATURE: {
      if (param_value_size == sizeof(float)) {
        float *ptr = static_cast<float *>(param_value);
        *ptr = dev->get_temperature();
        if (param_size_ret) *param_size_ret = sizeof(float);
      }
      break;
    }
    case AOCL_MMD_CONCURRENT_READS:
      RESULT_INT(1);
      break;
    case AOCL_MMD_CONCURRENT_WRITES:
      RESULT_INT(1);
      break;
    case AOCL_MMD_CONCURRENT_READS_OR_WRITES:
      RESULT_INT(2);
      break;

    case AOCL_MMD_MIN_HOST_MEMORY_ALIGNMENT:
      RESULT_SIZE_T(64);
      break;

    case AOCL_MMD_HOST_MEM_CAPABILITIES: {
      RESULT_INT(0);
      break;
    }
    case AOCL_MMD_SHARED_MEM_CAPABILITIES: {
      RESULT_INT(0);
      break;
    }

    case AOCL_MMD_DEVICE_MEM_CAPABILITIES:
      RESULT_INT(0);
      break;
    case AOCL_MMD_HOST_MEM_CONCURRENT_GRANULARITY:
      RESULT_SIZE_T(0);
      break;
    case AOCL_MMD_SHARED_MEM_CONCURRENT_GRANULARITY:
      RESULT_SIZE_T(0);
      break;
    case AOCL_MMD_DEVICE_MEM_CONCURRENT_GRANULARITY:
      RESULT_SIZE_T(0);
      break;
  }
  return 0;
}

#undef RESULT_INT
#undef RESULT_STR

/** Set the interrupt handler for the opened device.
 *  The interrupt handler is called whenever the client needs to be notified
 *  of an asynchronous event signaled by the device internals.
 *  For example, the kernel has completed or is stalled.
 *
 *  Important: Interrupts from the kernel must be ignored until this handler is
 *  set
 *
 *  Arguments:
 *    fn - the callback function to invoke when a kernel interrupt occurs
 *    user_data - the data that should be passed to fn when it is called.
 *
 *  Returns: 0 if successful, negative on error
 */
int AOCL_MMD_CALL aocl_mmd_set_interrupt_handler(int handle, aocl_mmd_interrupt_handler_fn fn, void *user_data) {
  Device *dev = device_manager.device_from_handle(handle);
  if (dev) {
    dev->set_kernel_interrupt(fn, user_data);
    MMD_DEBUG("DEBUG LOG : Set kernel interrupt handler for device handle : %d\n", handle);
  } else {
    MMD_DEBUG("DEBUG LOG : Error setting kernel interrupt handler for device handle : %d\n", handle);
    return MMD_AOCL_ERR;
  }
  return 0;
}

/** Set the operation status handler for the opened device.
 *  The operation status handler is called with
 *     status 0 when the operation has completed successfully.
 *     status negative when the operation completed with errors.
 *
 *  Arguments:
 *    fn - the callback function to invoke when a status update is to be
 *    performed.
 *    user_data - the data that should be passed to fn when it is called.
 *
 *  Returns: 0 if successful, negative on error
 */

int AOCL_MMD_CALL aocl_mmd_set_status_handler(int handle, aocl_mmd_status_handler_fn fn, void *user_data) {
  Device *dev = device_manager.device_from_handle(handle);
  if (dev) {
    dev->set_status_handler(fn, user_data);
    MMD_DEBUG("DEBUG LOG : Set status handler for device handle : %d\n", handle);
  }
  return 0;
}

/** Host to device-global-memory write (HOST DDR -> FPGA DDR)
 *  If op is NULL
 *     - Then these calls must block until the operation is complete.
 *     - The status handler is not called for this operation.
 *
 *  If op is non-NULL, then:
 *     - These may be non-blocking calls
 *     - The status handler must be called upon completion, with status 0
 *     for success, and a negative value for failure.
 *
 *  Arguments:
 *    op - the operation object used to track this operations progress
 *
 *    len - the size in bytes to transfer
 *
 *    src - the host buffer being read from
 *
 *    dst - the host buffer being written to
 *
 *    mmd_interface - the handle to the interface being accessed. E.g. To
 *    access global memory this handle will be whatever is returned by
 *    aocl_mmd_get_info when called with AOCL_MMD_MEMORY_INTERFACE.
 *
 *    offset/src_offset/dst_offset - the byte offset within the interface that
 *    the transfer will begin at.
 *
 *  The return value is 0 if the operation launch was successful, and
 *  negative otherwise.
 */
int AOCL_MMD_CALL
aocl_mmd_write(int handle, aocl_mmd_op_t op, size_t len, const void *src, int mmd_interface, size_t offset) {
  MMD_DEBUG(
      "DEBUG LOG : aocl_mmd_write: handle : %d\t operation : %p\t len : 0x%zx\t src : %p\t mmd_interface : %d\t offset "
      ": 0x%zx\n",
      handle,
      op,
      len,
      src,
      mmd_interface,
      offset);
  Device *dev = device_manager.device_from_handle(handle);
  if (dev){
    return dev->write_block(op, mmd_interface, src, offset, len);
  }
  else {
    MMD_DEBUG("DEBUG LOG : Error in aocl_mmd_write , device not found for handle : %d\n", handle);
    return -1;
  }
}

/** Host reading from device-global-memory (FPGA DDR -> HOST DDR)
 *  If op is NULL
 *     - Then these calls must block until the operation is complete.
 *     - The status handler is not called for this operation.
 *
 *  If op is non-NULL, then:
 *     - These may be non-blocking calls
 *     - The status handler must be called upon completion, with status 0
 *     for success, and a negative value for failure.
 *
 *  Arguments:
 *    op - the operation object used to track this operations progress
 *
 *    len - the size in bytes to transfer
 *
 *    src - the host buffer being read from
 *
 *    dst - the host buffer being written to
 *
 *    mmd_interface - the handle to the interface being accessed. E.g. To
 *    access global memory this handle will be whatever is returned by
 *    aocl_mmd_get_info when called with AOCL_MMD_MEMORY_INTERFACE.
 *
 *    offset/src_offset/dst_offset - the byte offset within the interface that
 *    the transfer will begin at.
 *
 *  The return value is 0 if the operation launch was successful, and
 *  negative otherwise.
 */

int AOCL_MMD_CALL aocl_mmd_read(int handle, aocl_mmd_op_t op, size_t len, void *dst, int mmd_interface, size_t offset) {
  MMD_DEBUG(
      "DEBUG LOG : aocl_mmd_read: handle : %d\t operation : %p\t len : 0x%zx\t dst : %p\t mmd_interface : %d\t offset "
      ": 0x%zx\n",
      handle,
      op,
      len,
      dst,
      mmd_interface,
      offset);
  Device *dev = device_manager.device_from_handle(handle);
  if (dev){
    return dev->read_block(op, mmd_interface, dst, offset, len);
  }
  else {
    MMD_DEBUG("DEBUG LOG : Error in aocl_mmd_read , device not found for handle : %d\n", handle);
    return -1;
  }
}

/** Open and initialize the named device.
 *
 *  The name is typically one specified by the AOCL_MMD_BOARD_NAMES offline
 *  info.
 *
 *  Arguments:
 *     name - open the board with this name (provided as a C-style string,
 *            i.e. NUL terminated ASCII.)
 *
 *  Returns: the non-negative integer handle for the board, otherwise a
 *  negative value to indicate error. Upon receiving the error, the OpenCL
 *  runtime will proceed to open other known devices, hence the MMD mustn't
 *  exit the application if an open call fails.
 */

int AOCL_MMD_CALL aocl_mmd_open(const char *name) {

  MMD_DEBUG("DEBUG LOG : aocl_mmd_open, Opening device: %s\n", name);

  uint64_t obj_id = device_manager.id_from_name(name);
  if (!obj_id) {
    MMD_DEBUG("DEBUG LOG : Error while aocl_mmd_open, object id not found for board : %s\n", name);
    return MMD_INVALID_PARAM;
  }

  int handle;
  Device *dev = nullptr;
  if (device_manager.get_or_create_device(name, &handle, &dev) != DeviceMapManager::SUCCESS) {
    if (std::getenv("MMD_PROGRAM_DEBUG") || std::getenv("MMD_DMA_DEBUG") || std::getenv("MMD_ENABLE_DEBUG")) {
      MMD_DEBUG("DEBUG LOG : Error while aocl_mmd_open, device not found for board : %s\n", name);
    }
    return MMD_AOCL_ERR;
  }

  assert(dev);
  if (dev->asp_loaded()) {
    if (!dev->initialize_asp()) {
      MMD_DEBUG("DEBUG LOG : Error while aocl_mmd_open, Error initializing asp for board : %s\n", name);
      return MMD_ASP_INIT_FAILED;
    }
  } else {
    MMD_DEBUG("DEBUG LOG : Error while aocl_mmd_open, asp not loaded for board : %s\n", name);
    return MMD_ASP_NOT_LOADED;
  }
  MMD_DEBUG("end of aocl_mmd_open \n");
  MMD_DEBUG("DEBUG LOG : Success aocl_mmd_open for board : %s, handle : %d \n", name, handle);
  return handle;
}

/** Close an opened device, by its handle.
 *  Returns: 0 on success, negative values on error.
 */
int AOCL_MMD_CALL aocl_mmd_close(int handle) {
#ifndef SIM
  device_manager.close_device_if_exists(handle);
#else
  std::cout << "# mmd.cpp: During simulation (ASE) we are not closing the device.\n";
#endif
  return 0;
}

// CoreDLA modifications
// To support multiple different FPGA boards, anything board specific must be implemented in a
// board-specific MMD instead of the CoreDLA runtime layer.
#ifdef DLA_MMD
// Query functions to get board-specific values
AOCL_MMD_CALL int dla_mmd_get_max_num_instances() { return 4; }

// DLA can only uses 4GB DDR as of 2024.2
AOCL_MMD_CALL uint64_t dla_mmd_get_ddr_size_per_instance() { return 1ULL << 32; }
AOCL_MMD_CALL double dla_mmd_get_ddr_clock_freq() {
  #ifdef USE_N6001_BOARD
  return 300.0; // MHz
  #else
  return 333.333333; // MHz
  #endif
}

// Helper functions for the wrapper functions around CSR and DDR
uint64_t dla_get_raw_csr_address(int instance, uint64_t addr) { return 0x10000 + (0x800 * instance) + addr; }
uint64_t dla_get_raw_ddr_address(int instance, uint64_t addr) {
  #ifdef USE_N6001_BOARD
  return (1ULL << 32) * instance + addr;
  #else
  return (1ULL << 33) * instance + addr;
  #endif
}

// Wrappers around CSR and DDR reads and writes to abstract away board-specific offsets
AOCL_MMD_CALL int dla_mmd_csr_write(int handle, int instance, uint64_t addr, const uint32_t *data) {
  return aocl_mmd_write(
      handle, NULL, sizeof(uint32_t), data, AOCL_MMD_DLA_CSR, dla_get_raw_csr_address(instance, addr));
}

AOCL_MMD_CALL int dla_mmd_csr_read(int handle, int instance, uint64_t addr, uint32_t *data) {
  return aocl_mmd_read(handle, NULL, sizeof(uint32_t), data, AOCL_MMD_DLA_CSR, dla_get_raw_csr_address(instance, addr));
}

AOCL_MMD_CALL int dla_mmd_ddr_write(int handle, int instance, uint64_t addr, uint64_t length, const void *data) {
  return aocl_mmd_write(handle, NULL, length, data, AOCL_MMD_MEMORY, dla_get_raw_ddr_address(instance, addr));
}

AOCL_MMD_CALL int dla_mmd_ddr_read(int handle, int instance, uint64_t addr, uint64_t length, void *data) {
  return aocl_mmd_read(handle, NULL, length, data, AOCL_MMD_MEMORY, dla_get_raw_ddr_address(instance, addr));
}

AOCL_MMD_CALL double dla_mmd_get_coredla_clock_freq(int handle) {
  constexpr uint64_t hw_timer_address = 0x37000;
  const uint32_t start_bit = 1;
  const uint32_t stop_bit = 2;

  // Send the start command to the hardware counter
  std::chrono::high_resolution_clock::time_point time_before = std::chrono::high_resolution_clock::now();
  int status = aocl_mmd_write(handle, NULL, sizeof(uint32_t), &start_bit, AOCL_MMD_DLA_CSR, hw_timer_address);
  assert(status == 0);

  // Unlikely to sleep for exactly 10 milliseconds, but it doesn't matter since we use a high resolution clock to
  // determine the amount of time between the start and stop commands for the hardware counter
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Send the stop command to the hardware counter
  std::chrono::high_resolution_clock::time_point time_after = std::chrono::high_resolution_clock::now();
  status = aocl_mmd_write(handle, NULL, sizeof(uint32_t), &stop_bit, AOCL_MMD_DLA_CSR, hw_timer_address);
  assert(status == 0);

  // Read back the value of the counter
  uint32_t counter = 0;
  status = aocl_mmd_read(handle, NULL, sizeof(uint32_t), &counter, AOCL_MMD_DLA_CSR, hw_timer_address);
  assert(status == 0);

  // Calculate the clock frequency of the counter, which is running on clk_dla
  double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(time_after - time_before).count();
  return 1.0e-6 * counter / elapsed_seconds;  // 1.0e-6 is to convert to MHz
}
#endif
