// Copyright 2018-2020 Intel Corporation.
//
// This software and the related documents are Intel copyrighted materials,
// and your use of them is governed by the express license under which they
// were provided to you ("License"). Unless the License provides otherwise,
// you may not use, modify, copy, publish, distribute, disclose or transmit
// this software or the related documents without Intel's prior written
// permission.
//
// This software and the related documents are provided as is, with no express
// or implied warranties, other than those that are expressly stated in the
// License.

// This is derived from OPAE + OpenCL PAC BSP

/**
 * \fpga_dma.c
 * \brief FPGA DMA User-mode driver
 */

#include "fpga_dma.h"
#include <assert.h>
#include <errno.h>
#include <opae/fpga.h>
#include <poll.h>
#include <safe_string/safe_string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "fpga_dma_internal.h"
#include "memcpy_s_fast.h"

#ifdef SIM
#define USE_ASE
#else
// TODO:  Need this until we can adequately sync MMIO R/W with pointer accesses.
// Causes module to use fpgaMMIORead32() instead of foo = *ptr;
#define USE_ASE
#endif

#ifdef FPGA_DMA_DEBUG
static int err_cnt = 0;
#endif

#ifdef CHECK_DELAYS
double poll_wait_count = 0;
double buf_full_count = 0;
#endif

/*
 * macro for checking return codes
 */
#define ON_ERR_GOTO(res, label, desc)                         \
  do {                                                        \
    if ((res) != FPGA_OK) {                                   \
      error_print("Error %s: %s\n", (desc), fpgaErrStr(res)); \
      goto label;                                             \
    }                                                         \
  } while (0)

#define ON_ERR_RETURN(res, desc)                              \
  do {                                                        \
    if ((res) != FPGA_OK) {                                   \
      error_print("Error %s: %s\n", (desc), fpgaErrStr(res)); \
      return (res);                                           \
    }                                                         \
  } while (0)

// Internal Functions

/**
 * MMIOWrite64Blk
 *
 * @brief                Writes a block of 64-bit values to FPGA MMIO space
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] device     FPGA address
 * @param[in] host       Host buffer address
 * @param[in] count      Size in bytes
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result MMIOWrite64Blk(fpga_dma_handle dma_h, uint64_t device, uint64_t host, uint64_t bytes) {
  assert(IS_ALIGNED_QWORD(device));
  assert(IS_ALIGNED_QWORD(bytes));

  uint64_t *haddr = (uint64_t *)host;
  uint64_t i;
  fpga_result res = FPGA_OK;

#ifndef USE_ASE
  volatile uint64_t *dev_addr = HOST_MMIO_64_ADDR(dma_h, device);
#endif

  debug_print("copying %lld bytes from 0x%p to 0x%p\n", (long long int)bytes, haddr, (void *)device);
  for (i = 0; i < bytes / sizeof(uint64_t); i++) {
#ifdef USE_ASE
    res = fpgaWriteMMIO64(dma_h->fpga_h, dma_h->mmio_num, device, *haddr);
    ON_ERR_RETURN(res, "fpgaWriteMMIO64");
    haddr++;
    device += sizeof(uint64_t);
#else
    *dev_addr++ = *haddr++;
#endif
  }
  return res;
}

/**
 * MMIOWrite32Blk
 *
 * @brief                Writes a block of 32-bit values to FPGA MMIO space
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] device     FPGA address
 * @param[in] host       Host buffer address
 * @param[in] count      Size in bytes
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result MMIOWrite32Blk(fpga_dma_handle dma_h, uint64_t device, uint64_t host, uint64_t bytes) {
  assert(IS_ALIGNED_DWORD(device));
  assert(IS_ALIGNED_DWORD(bytes));

  uint32_t *haddr = (uint32_t *)host;
  uint64_t i;
  fpga_result res = FPGA_OK;

#ifndef USE_ASE
  volatile uint32_t *dev_addr = HOST_MMIO_32_ADDR(dma_h, device);
#endif

  debug_print("copying %lld bytes from 0x%p to 0x%p\n", (long long int)bytes, haddr, (void *)device);
  for (i = 0; i < bytes / sizeof(uint32_t); i++) {
#ifdef USE_ASE
    res = fpgaWriteMMIO32(dma_h->fpga_h, dma_h->mmio_num, device, *haddr);
    ON_ERR_RETURN(res, "fpgaWriteMMIO32");
    haddr++;
    device += sizeof(uint32_t);
#else
    *dev_addr++ = *haddr++;
#endif
  }
  return res;
}

/**
 * MMIORead64Blk
 *
 * @brief                Reads a block of 64-bit values from FPGA MMIO space
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] device     FPGA address
 * @param[in] host       Host buffer address
 * @param[in] count      Size in bytes
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result MMIORead64Blk(fpga_dma_handle dma_h, uint64_t device, uint64_t host, uint64_t bytes) {
  assert(IS_ALIGNED_QWORD(device));
  assert(IS_ALIGNED_QWORD(bytes));

  uint64_t *haddr = (uint64_t *)host;
  uint64_t i;
  fpga_result res = FPGA_OK;

#ifndef USE_ASE
  volatile uint64_t *dev_addr = HOST_MMIO_64_ADDR(dma_h, device);
#endif

  debug_print("copying %lld bytes from 0x%p to 0x%p\n", (long long int)bytes, (void *)device, haddr);
  for (i = 0; i < bytes / sizeof(uint64_t); i++) {
#ifdef USE_ASE
    res = fpgaReadMMIO64(dma_h->fpga_h, dma_h->mmio_num, device, haddr);
    ON_ERR_RETURN(res, "fpgaReadMMIO64");
    haddr++;
    device += sizeof(uint64_t);
#else
    *haddr++ = *dev_addr++;
#endif
  }
  return res;
}

/**
 * MMIORead32Blk
 *
 * @brief                Reads a block of 32-bit values from FPGA MMIO space
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] device     FPGA address
 * @param[in] host       Host buffer address
 * @param[in] count      Size in bytes
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result MMIORead32Blk(fpga_dma_handle dma_h, uint64_t device, uint64_t host, uint64_t bytes) {
  assert(IS_ALIGNED_DWORD(device));
  assert(IS_ALIGNED_DWORD(bytes));

  uint32_t *haddr = (uint32_t *)host;
  uint64_t i;
  fpga_result res = FPGA_OK;

#ifndef USE_ASE
  volatile uint32_t *dev_addr = HOST_MMIO_32_ADDR(dma_h, device);
#endif

  debug_print("copying %lld bytes from 0x%p to 0x%p\n", (long long int)bytes, (void *)device, haddr);
  for (i = 0; i < bytes / sizeof(uint32_t); i++) {
#ifdef USE_ASE
    res = fpgaReadMMIO32(dma_h->fpga_h, dma_h->mmio_num, device, haddr);
    ON_ERR_RETURN(res, "fpgaReadMMIO32");
    haddr++;
    device += sizeof(uint32_t);
#else
    *haddr++ = *dev_addr++;
#endif
  }
  return res;
}

// Feature type is BBB
static inline bool fpga_dma_feature_is_bbb(uint64_t dfh) {
  // BBB is type 2
  return ((dfh >> AFU_DFH_TYPE_OFFSET) & 0xf) == FPGA_DMA_BBB;
}

/**
 * _switch_to_ase_page
 *
 * @brief                Updates the current page of ASE to the address given
 * @param[in] dma_h      Handle to the FPGA DMA object
 * @param[in] addr       Address to which the ASE page should be switched
 * @return Nothing.  Side-effect is to update the current page in the DMA handle.
 *
 */
static inline void _switch_to_ase_page(fpga_dma_handle dma_h, uint64_t addr) {
  uint64_t requested_page = addr & ~DMA_ADDR_SPAN_EXT_WINDOW_MASK;

  if (requested_page != dma_h->cur_ase_page) {
    MMIOWrite64Blk(dma_h, ASE_CNTL_BASE(dma_h), (uint64_t)&requested_page, sizeof(requested_page));
    dma_h->cur_ase_page = requested_page;
  }
}

/**
 * _send_descriptor
 *
 * @brief                Queues a DMA descriptor to the FPGA
 * @param[in] dma_h      Handle to the FPGA DMA object
 * @param[in] desc       Pointer to a descriptor structure to send
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result _send_descriptor(fpga_dma_handle dma_h, msgdma_ext_desc_t *desc) {
  fpga_result res = FPGA_OK;
  msgdma_status_t status = {0};

  debug_print("desc.rd_address = %x\n", desc->rd_address);
  debug_print("desc.wr_address = %x\n", desc->wr_address);
  debug_print("desc.len = %x\n", desc->len);
  debug_print("desc.wr_burst_count = %x\n", desc->wr_burst_count);
  debug_print("desc.rd_burst_count = %x\n", desc->rd_burst_count);
  debug_print("desc.wr_stride %x\n", desc->wr_stride);
  debug_print("desc.rd_stride %x\n", desc->rd_stride);
  debug_print("desc.rd_address_ext %x\n", desc->rd_address_ext);
  debug_print("desc.wr_address_ext %x\n", desc->wr_address_ext);

  debug_print("SGDMA_CSR_BASE = %lx SGDMA_DESC_BASE=%lx\n", dma_h->dma_csr_base, dma_h->dma_desc_base);

#ifdef CHECK_DELAYS
  bool first = true;
#endif
  do {
    res = MMIORead32Blk(dma_h, CSR_STATUS(dma_h), (uint64_t)&status.reg, sizeof(status.reg));
    ON_ERR_GOTO(res, out, "MMIORead32Blk");
#ifdef CHECK_DELAYS
    if (first && status.st.desc_buf_full) {
      buf_full_count++;
      first = false;
    }
#endif
  } while (status.st.desc_buf_full);

  res = MMIOWrite64Blk(dma_h, dma_h->dma_desc_base, (uint64_t)desc, sizeof(*desc));
  ON_ERR_GOTO(res, out, "MMIOWrite64Blk");

out:
  return res;
}

/**
 * _do_dma
 *
 * @brief                    Performs a DMA transaction with the FPGA
 * @param[in] dma_h          Handle to the FPGA DMA object
 * @param[in] dst            Pointer to a host or FPGA buffer to send or retrieve
 * @param[in] src            Pointer to a host or FPGA buffer to send or retrieve
 * @param[in] count          Number of bytes
 * @param[in] is_last_desc   True if this is the last buffer of a batch
 * @param[in] type           Direction of transfer
 * @param[in] intr_en        True means to ask for an interrupt from the FPGA
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result _do_dma(fpga_dma_handle dma_h,
                           uint64_t dst,
                           uint64_t src,
                           int count,
                           int is_last_desc,
                           fpga_dma_transfer_t type,
                           bool intr_en) {
  msgdma_ext_desc_t desc = {0};
  fpga_result res = FPGA_OK;
  int alignment_offset = 0;
  int segment_size = 0;

  // src, dst and count must be 64-byte aligned
  if (dst % FPGA_DMA_ALIGN_BYTES != 0 || src % FPGA_DMA_ALIGN_BYTES != 0 || count % FPGA_DMA_ALIGN_BYTES != 0) {
    return FPGA_INVALID_PARAM;
  }
  // these fields are fixed for all DMA transfers
  desc.seq_num = 0;
  desc.wr_stride = 1;
  desc.rd_stride = 1;

  desc.control.go = 1;
  if (intr_en)
    desc.control.transfer_irq_en = 1;
  else
    desc.control.transfer_irq_en = 0;

  // Enable "earlyreaddone" in the control field of the descriptor except the last.
  // Setting early done causes the read logic to move to the next descriptor
  // before the previous descriptor completes.
  // This elminates a few hundred clock cycles of waiting between transfers.
  if (!is_last_desc)
    desc.control.early_done_en = 1;
  else
    desc.control.early_done_en = 0;

  if (type == FPGA_TO_FPGA_MM) {
    desc.rd_address = src & FPGA_DMA_MASK_32_BIT;
    desc.wr_address = dst & FPGA_DMA_MASK_32_BIT;
    desc.len = count;
    desc.wr_burst_count = 4;
    desc.rd_burst_count = 4;
    desc.rd_address_ext = (src >> 32) & FPGA_DMA_MASK_32_BIT;
    desc.wr_address_ext = (dst >> 32) & FPGA_DMA_MASK_32_BIT;

    res = _send_descriptor(dma_h, &desc);
    ON_ERR_GOTO(res, out, "_send_descriptor");
  }
  // either FPGA to Host or Host to FPGA transfer so we need to make sure the DMA transaction is aligned to the burst
  // size (CCIP restriction)
  else {
    // need to determine if the CCIP (host) address is aligned to 4CL (256B).  When 0 the CCIP address is aligned.
    alignment_offset =
        (type == HOST_TO_FPGA_MM) ? (src % (4 * FPGA_DMA_ALIGN_BYTES)) : (dst % (4 * FPGA_DMA_ALIGN_BYTES));

    // not aligned to 4CL so performing a short transfer to get aligned
    if (alignment_offset != 0) {
      desc.rd_address = src & FPGA_DMA_MASK_32_BIT;
      desc.wr_address = dst & FPGA_DMA_MASK_32_BIT;
      desc.wr_burst_count = 1;
      desc.rd_burst_count = 1;
      desc.rd_address_ext = (src >> 32) & FPGA_DMA_MASK_32_BIT;
      desc.wr_address_ext = (dst >> 32) & FPGA_DMA_MASK_32_BIT;

      // count isn't large enough to hit next 4CL boundary
      if (((4 * FPGA_DMA_ALIGN_BYTES) - alignment_offset) >= count) {
        segment_size = count;
        count = 0;  // only had to transfer count amount of data to reach the end of the provided buffer
      } else {
        segment_size = (4 * FPGA_DMA_ALIGN_BYTES) - alignment_offset;
        src += segment_size;
        dst += segment_size;
        count -= segment_size;  // subtract the segment size from count since the transfer below will bring us into 4CL
                                // alignment
        desc.control.transfer_irq_en = 0;
      }

      // will post short transfer to align to a 4CL (256 byte) boundary
      desc.len = segment_size;

      res = _send_descriptor(dma_h, &desc);
      ON_ERR_GOTO(res, out, "_send_descriptor");
    }
    // at this point we are 4CL (256 byte) aligned
    // if there is at least 4CL (256 bytes) of data to transfer, post bursts of 4
    if (count >= (4 * FPGA_DMA_ALIGN_BYTES)) {
      desc.rd_address = src & FPGA_DMA_MASK_32_BIT;
      desc.wr_address = dst & FPGA_DMA_MASK_32_BIT;
      desc.wr_burst_count = 4;
      desc.rd_burst_count = 4;
      desc.rd_address_ext = (src >> 32) & FPGA_DMA_MASK_32_BIT;
      desc.wr_address_ext = (dst >> 32) & FPGA_DMA_MASK_32_BIT;

      // buffer ends on 4CL boundary
      if ((count % (4 * FPGA_DMA_ALIGN_BYTES)) == 0) {
        segment_size = count;
        count = 0;  // transfer below will move the remainder of the buffer
      }
      // buffers do not end on 4CL boundary so transfer only up to the last 4CL boundary leaving a segment at the end to
      // finish later
      else {
        segment_size = count - (count % (4 * FPGA_DMA_ALIGN_BYTES));  // round count down to the nearest multiple of 4CL
        src += segment_size;
        dst += segment_size;
        count -= segment_size;
        desc.control.transfer_irq_en = 0;
      }

      desc.len = segment_size;

      res = _send_descriptor(dma_h, &desc);
      ON_ERR_GOTO(res, out, "_send_descriptor");
    }
    // at this point we have posted all the bursts of length 4 we can but there might be 64, 128, or 192 bytes of data
    // to transfer still if buffer did not end on 4CL (256 byte) boundary post short transfer to handle the remainder
    if (count > 0) {
      desc.rd_address = src & FPGA_DMA_MASK_32_BIT;
      desc.wr_address = dst & FPGA_DMA_MASK_32_BIT;
      desc.len = count;
      desc.wr_burst_count = 1;
      desc.rd_burst_count = 1;
      desc.rd_address_ext = (src >> 32) & FPGA_DMA_MASK_32_BIT;
      desc.wr_address_ext = (dst >> 32) & FPGA_DMA_MASK_32_BIT;
      if (intr_en) desc.control.transfer_irq_en = 1;
      // will post short transfer to move the remainder of the buffer
      res = _send_descriptor(dma_h, &desc);
      ON_ERR_GOTO(res, out, "_send_descriptor");
    }

  }  // end of FPGA --> Host or Host --> FPGA transfer

out:
  return res;
}

fpga_result fpgaDmaChannelOpen(fpga_handle fpga, uint64_t dfh_offset, int interrupt_num, fpga_dma_handle *dma_p) {
  fpga_result res = FPGA_OK;
  fpga_dma_handle dma_h = NULL;
  int i = 0;
  if (!fpga) {
    return FPGA_INVALID_PARAM;
  }
  if (!dma_p) {
    return FPGA_INVALID_PARAM;
  }
  // init the dma handle
  dma_h = (fpga_dma_handle)malloc(sizeof(struct _dma_handle_t));
  if (!dma_h) {
    return FPGA_NO_MEMORY;
  }
  dma_h->fpga_h = fpga;
  for (i = 0; i < FPGA_DMA_MAX_BUF; i++) dma_h->dma_buf_ptr[i] = NULL;
  dma_h->mmio_num = 0;
  dma_h->cur_ase_page = 0xffffffffffffffffUll;

  // Discover DMA BBB by traversing the device feature list
  bool dma_found = false;

#ifndef USE_ASE
  res = fpgaMapMMIO(dma_h->fpga_h, 0, (uint64_t **)&dma_h->mmio_va);
  ON_ERR_GOTO(res, out, "fpgaMapMMIO");
#endif

  dfh_feature_t dfh = {0};
  res = MMIORead64Blk(dma_h, dfh_offset, (uint64_t)&dfh, sizeof(dfh));
  ON_ERR_GOTO(res, out, "MMIORead64Blk");

  if (fpga_dma_feature_is_bbb(dfh.dfh) && (dfh.feature_uuid_lo == FPGA_DMA_UUID_L) &&
      (dfh.feature_uuid_hi == FPGA_DMA_UUID_H)) {
    dma_h->dma_base = dfh_offset;
    dma_h->dma_csr_base = dma_h->dma_base + FPGA_DMA_CSR;
    dma_h->dma_desc_base = dma_h->dma_base + FPGA_DMA_DESC;
    dma_h->dma_ase_cntl_base = dma_h->dma_base + FPGA_DMA_ADDR_SPAN_EXT_CNTL;
    dma_h->dma_ase_data_base = dma_h->dma_base + FPGA_DMA_ADDR_SPAN_EXT_DATA;
    dma_found = true;
    *dma_p = dma_h;
    res = FPGA_OK;
  } else {
    *dma_p = NULL;
    res = FPGA_NOT_FOUND;
    goto out;
  }

  // Buffer size must be page aligned for prepareBuffer
  for (i = 0; i < FPGA_DMA_MAX_BUF; i++) {
    res = fpgaPrepareBuffer(
        dma_h->fpga_h, FPGA_DMA_BUF_SIZE, (void **)&(dma_h->dma_buf_ptr[i]), &dma_h->dma_buf_wsid[i], 0);
    ON_ERR_GOTO(res, out, "fpgaPrepareBuffer");

    // Make sure it's actually allocated
    dma_h->dma_buf_ptr[i][0] = 0xff;
    madvise((void *)dma_h->dma_buf_ptr[i], FPGA_DMA_BUF_SIZE, MADV_SEQUENTIAL);

    res = fpgaGetIOAddress(dma_h->fpga_h, dma_h->dma_buf_wsid[i], &dma_h->dma_buf_iova[i]);
    ON_ERR_GOTO(res, rel_buf, "fpgaGetIOAddress");
  }

  // Allocate magic number buffer
  res = fpgaPrepareBuffer(dma_h->fpga_h, FPGA_DMA_ALIGN_BYTES, (void **)&(dma_h->magic_buf), &dma_h->magic_wsid, 0);
  ON_ERR_GOTO(res, out, "fpgaPrepareBuffer");

  dma_h->magic_buf[0] = 0xff;

  res = fpgaGetIOAddress(dma_h->fpga_h, dma_h->magic_wsid, &dma_h->magic_iova);
  ON_ERR_GOTO(res, rel_buf, "fpgaGetIOAddress");
  memset((void *)dma_h->magic_buf, 0, FPGA_DMA_ALIGN_BYTES);

  // turn on global interrupts
  msgdma_ctrl_t ctrl = {0};
  ctrl.ct.global_intr_en_mask = 1;
  res = MMIOWrite32Blk(dma_h, CSR_CONTROL(dma_h), (uint64_t)&ctrl.reg, sizeof(ctrl.reg));
  ON_ERR_GOTO(res, rel_buf, "MMIOWrite32Blk");

  // register interrupt event handle
  res = fpgaCreateEventHandle(&dma_h->eh);
  ON_ERR_GOTO(res, rel_buf, "fpgaCreateEventHandle");

  res = fpgaRegisterEvent(dma_h->fpga_h, FPGA_EVENT_INTERRUPT, dma_h->eh, interrupt_num /*vector id */);
  ON_ERR_GOTO(res, destroy_eh, "fpgaRegisterEvent");

  return FPGA_OK;

destroy_eh:
  res = fpgaDestroyEventHandle(&dma_h->eh);
  ON_ERR_GOTO(res, rel_buf, "fpgaDestroyEventHandle");

rel_buf:
  for (i = 0; i < FPGA_DMA_MAX_BUF; i++) {
    res = fpgaReleaseBuffer(dma_h->fpga_h, dma_h->dma_buf_wsid[i]);
    ON_ERR_GOTO(res, out, "fpgaReleaseBuffer");
  }
out:
  if (!dma_found) {
    free(dma_h);
  }
  return res;
}

/**
 * _read_memory_mmio_unaligned
 *
 * @brief                Performs a unaligned read(address not 4/8/64 byte aligned) from FPGA address(device address).
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] dev_addr   FPGA address
 * @param[in] host_addr  Host buffer address
 * @param[in] count      Size in bytes, always less than 8bytes.
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result _read_memory_mmio_unaligned(fpga_dma_handle dma_h,
                                               uint64_t dev_addr,
                                               uint64_t host_addr,
                                               uint64_t count) {
  fpga_result res = FPGA_OK;

  assert(count < QWORD_BYTES);

  if (0 == count) return res;

  uint64_t shift = dev_addr % QWORD_BYTES;
  debug_print("shift = %08lx , count = %08lx \n", shift, count);

  _switch_to_ase_page(dma_h, dev_addr);
  uint64_t dev_aligned_addr = (dev_addr - shift) & DMA_ADDR_SPAN_EXT_WINDOW_MASK;

  // read data from device memory
  uint64_t read_tmp = 0;
  res = MMIORead64Blk(dma_h, ASE_DATA_BASE(dma_h) + dev_aligned_addr, (uint64_t)&read_tmp, sizeof(read_tmp));
  if (res != FPGA_OK) return res;

  // overlay our data
  memcpy_s_fast((void *)host_addr, count, ((char *)(&read_tmp)) + shift, count);

  return res;
}

/**
 * _write_memory_mmio_unaligned
 *
 * @brief                Performs an unaligned write(address not 4/8/64 byte aligned) to FPGA address(device address).
 * @param[in] dma        Handle to the FPGA DMA object
 * @param[in] dev_addr   FPGA address
 * @param[in] host_addr  Host buffer address
 * @param[in] count      Size in bytes, always less than 8bytes.
 * @return fpga_result FPGA_OK on success, return code otherwise
 *
 */
static fpga_result _write_memory_mmio_unaligned(fpga_dma_handle dma_h,
                                                uint64_t dev_addr,
                                                uint64_t host_addr,
                                                uint64_t count) {
  fpga_result res = FPGA_OK;

  assert(count < QWORD_BYTES);

  if (0 == count) return res;

  uint64_t shift = dev_addr % QWORD_BYTES;
  debug_print("shift = %08lx , count = %08lx \n", shift, count);

  _switch_to_ase_page(dma_h, dev_addr);
  uint64_t dev_aligned_addr = (dev_addr - (dev_addr % QWORD_BYTES)) & DMA_ADDR_SPAN_EXT_WINDOW_MASK;

  // read data from device memory
  uint64_t read_tmp = 0;
  res = MMIORead64Blk(dma_h, ASE_DATA_BASE(dma_h) + dev_aligned_addr, (uint64_t)&read_tmp, sizeof(read_tmp));
  if (res != FPGA_OK) return res;

  // overlay our data
  memcpy_s_fast(((char *)(&read_tmp)) + shift, count, (void *)host_addr, count);

  // write back to device
  res = MMIOWrite64Blk(dma_h, ASE_DATA_BASE(dma_h) + dev_aligned_addr, (uint64_t)&read_tmp, sizeof(read_tmp));
  if (res != FPGA_OK) return res;

  return res;
}

/**
 * _write_memory_mmio
 *
 * @brief                   Writes to a DWORD/QWORD aligned memory address(FPGA address).
 * @param[in] dma           Handle to the FPGA DMA object
 * @param[in/out] dst_ptr   Pointer to the FPGA address
 * @param[in/out] src_ptr   Pointer to the Host buffer address
 * @param[in/out] count     Pointer to the Size in bytes
 * @return fpga_result      FPGA_OK on success, return code otherwise.  Updates src, dst, and count
 *
 */
static fpga_result _write_memory_mmio(fpga_dma_handle dma_h, uint64_t *dst_ptr, uint64_t *src_ptr, uint64_t *count) {
  fpga_result res = FPGA_OK;

  if (*count < DWORD_BYTES) return res;

  assert(*count >= DWORD_BYTES);
  assert(IS_ALIGNED_DWORD(*dst_ptr));
  if (!IS_ALIGNED_DWORD(*dst_ptr))  // If QWORD aligned, this will be true
    return FPGA_EXCEPTION;

  uint64_t src = *src_ptr;
  uint64_t dst = *dst_ptr;
  uint64_t align_bytes = *count;
  uint64_t offset = 0;

  if (!IS_ALIGNED_QWORD(dst)) {
    // Write out a single DWORD to get QWORD aligned
    _switch_to_ase_page(dma_h, dst);
    offset = dst & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIOWrite32Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)src, DWORD_BYTES);
    ON_ERR_RETURN(res, "MMIOWrite32Blk");
    src += DWORD_BYTES;
    dst += DWORD_BYTES;
    align_bytes -= DWORD_BYTES;
  }

  if (0 == align_bytes) return res;

  assert(IS_ALIGNED_QWORD(dst));

  // Write out blocks of 64-bit values
  while (align_bytes >= QWORD_BYTES) {
    uint64_t left_in_page = DMA_ADDR_SPAN_EXT_WINDOW;
    left_in_page -= dst & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    uint64_t size_to_copy = min(left_in_page, (align_bytes & ~(QWORD_BYTES - 1)));
    if (size_to_copy < QWORD_BYTES) break;
    _switch_to_ase_page(dma_h, dst);
    offset = dst & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIOWrite64Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)src, size_to_copy);
    ON_ERR_RETURN(res, "MMIOWrite64Blk");
    src += size_to_copy;
    dst += size_to_copy;
    align_bytes -= size_to_copy;
  }

  if (align_bytes >= DWORD_BYTES) {
    // Write out remaining DWORD
    _switch_to_ase_page(dma_h, dst);
    offset = dst & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIOWrite32Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)src, DWORD_BYTES);
    ON_ERR_RETURN(res, "MMIOWrite32Blk");
    src += DWORD_BYTES;
    dst += DWORD_BYTES;
    align_bytes -= DWORD_BYTES;
  }

  assert(align_bytes < DWORD_BYTES);

  *src_ptr = src;
  *dst_ptr = dst;
  *count = align_bytes;
  return res;
}

/**
 * _ase_host_to_fpga
 *
 * @brief                   Tx "count" bytes from HOST to FPGA using Address span expander(ASE)- will internally make
 * calls to handle unaligned and aligned MMIO writes.
 * @param[in] dma           Handle to the FPGA DMA object
 * @param[in/out] dst_ptr   Pointer to the FPGA address
 * @param[in/out] src_ptr   Pointer to the Host buffer address
 * @param[in] count         Size in bytes
 * @return fpga_result      FPGA_OK on success, return code otherwise.  Updates src and dst
 *
 */
static fpga_result _ase_host_to_fpga(fpga_dma_handle dma_h, uint64_t *dst_ptr, uint64_t *src_ptr, uint64_t count) {
  fpga_result res = FPGA_OK;
  uint64_t dst = *dst_ptr;
  uint64_t src = *src_ptr;
  uint64_t count_left = count;
  uint64_t unaligned_size = 0;

  debug_print("dst_ptr = %08lx , count = %08lx, src = %08lx \n", *dst_ptr, count, *src_ptr);

  // Aligns address to 8 byte using dst masking method
  if (!IS_ALIGNED_DWORD(dst) && !IS_ALIGNED_QWORD(dst)) {
    unaligned_size = QWORD_BYTES - (dst % QWORD_BYTES);
    if (unaligned_size > count_left) unaligned_size = count_left;
    res = _write_memory_mmio_unaligned(dma_h, dst, src, unaligned_size);
    if (res != FPGA_OK) return res;
    count_left -= unaligned_size;
    src += unaligned_size;
    dst += unaligned_size;
  }
  // Handles 8/4 byte MMIO transfer
  res = _write_memory_mmio(dma_h, &dst, &src, &count_left);
  if (res != FPGA_OK) return res;

  // Left over unaligned count bytes are transfered using dst masking method
  unaligned_size = QWORD_BYTES - (dst % QWORD_BYTES);
  if (unaligned_size > count_left) unaligned_size = count_left;

  res = _write_memory_mmio_unaligned(dma_h, dst, src, unaligned_size);
  if (res != FPGA_OK) return res;

  count_left -= unaligned_size;

  *dst_ptr = dst + unaligned_size;
  *src_ptr = src + unaligned_size;

  return FPGA_OK;
}

/**
 * _read_memory_mmio
 *
 * @brief                   Reads a DWORD/QWORD aligned memory address(FPGA address).
 * @param[in] dma           Handle to the FPGA DMA object
 * @param[in/out] dst_ptr   Pointer to the Host Buffer Address
 * @param[in/out] src_ptr   Pointer to the FPGA address
 * @param[in/out] count     Pointer to the size in bytes
 * @return fpga_result      FPGA_OK on success, return code otherwise.  Updates src, dst, and count
 *
 */
static fpga_result _read_memory_mmio(fpga_dma_handle dma_h, uint64_t *src_ptr, uint64_t *dst_ptr, uint64_t *count) {
  fpga_result res = FPGA_OK;

  if (*count < DWORD_BYTES) return res;

  assert(*count >= DWORD_BYTES);
  assert(IS_ALIGNED_DWORD(*src_ptr));
  if (!IS_ALIGNED_DWORD(*src_ptr))  // If QWORD aligned, this will be true
    return FPGA_EXCEPTION;

  uint64_t src = *src_ptr;
  uint64_t dst = *dst_ptr;
  uint64_t align_bytes = *count;
  uint64_t offset = 0;

  if (!IS_ALIGNED_QWORD(src)) {
    // Read a single DWORD to get QWORD aligned
    _switch_to_ase_page(dma_h, src);
    offset = src & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIORead32Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)dst, DWORD_BYTES);
    ON_ERR_RETURN(res, "MMIORead32Blk");
    src += DWORD_BYTES;
    dst += DWORD_BYTES;
    align_bytes -= DWORD_BYTES;
  }

  if (0 == align_bytes) return res;

  assert(IS_ALIGNED_QWORD(src));

  // Read blocks of 64-bit values
  while (align_bytes >= QWORD_BYTES) {
    uint64_t left_in_page = DMA_ADDR_SPAN_EXT_WINDOW;
    left_in_page -= src & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    uint64_t size_to_copy = min(left_in_page, (align_bytes & ~(QWORD_BYTES - 1)));
    if (size_to_copy < QWORD_BYTES) break;
    _switch_to_ase_page(dma_h, src);
    offset = src & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIORead64Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)dst, size_to_copy);
    ON_ERR_RETURN(res, "MMIORead64Blk");
    src += size_to_copy;
    dst += size_to_copy;
    align_bytes -= size_to_copy;
  }

  if (align_bytes >= DWORD_BYTES) {
    // Read remaining DWORD
    _switch_to_ase_page(dma_h, src);
    offset = src & DMA_ADDR_SPAN_EXT_WINDOW_MASK;
    res = MMIORead32Blk(dma_h, ASE_DATA_BASE(dma_h) + offset, (uint64_t)dst, DWORD_BYTES);
    ON_ERR_RETURN(res, "MMIORead32Blk");
    src += DWORD_BYTES;
    dst += DWORD_BYTES;
    align_bytes -= DWORD_BYTES;
  }

  assert(align_bytes < DWORD_BYTES);

  *src_ptr = src;
  *dst_ptr = dst;
  *count = align_bytes;
  return res;
}

/**
 * _ase_fpga_to_host
 *
 * @brief                   Tx "count" bytes from FPGA to HOST using Address span expander(ASE)- will internally make
 * calls to handle unaligned and aligned MMIO writes.
 * @param[in] dma           Handle to the FPGA DMA object
 * @param[in/out] dst_ptr   Pointer to the Host Buffer Address
 * @param[in/out] src_ptr   Pointer to the FPGA address
 * @param[in/out] count     Size in bytes
 * @return fpga_result      FPGA_OK on success, return code otherwise.  Updates src and dst
 *
 */
static fpga_result _ase_fpga_to_host(fpga_dma_handle dma_h, uint64_t *src_ptr, uint64_t *dst_ptr, uint64_t count) {
  fpga_result res = FPGA_OK;
  uint64_t src = *src_ptr;
  uint64_t dst = *dst_ptr;
  uint64_t count_left = count;
  uint64_t unaligned_size = 0;

  debug_print("dst_ptr = %08lx , count = %08lx, src = %08lx \n", *dst_ptr, count, *src_ptr);

  // Aligns address to 8 byte using src masking method
  if (!IS_ALIGNED_DWORD(src) && !IS_ALIGNED_QWORD(src)) {
    unaligned_size = QWORD_BYTES - (src % QWORD_BYTES);
    if (unaligned_size > count_left) unaligned_size = count_left;
    res = _read_memory_mmio_unaligned(dma_h, src, dst, unaligned_size);
    if (res != FPGA_OK) return res;
    count_left -= unaligned_size;
    dst += unaligned_size;
    src += unaligned_size;
  }
  // Handles 8/4 byte MMIO transfer
  res = _read_memory_mmio(dma_h, &src, &dst, &count_left);
  if (res != FPGA_OK) return res;

  // Left over unaligned count bytes are transfered using src masking method
  unaligned_size = QWORD_BYTES - (src % QWORD_BYTES);
  if (unaligned_size > count_left) unaligned_size = count_left;

  res = _read_memory_mmio_unaligned(dma_h, src, dst, unaligned_size);
  if (res != FPGA_OK) return res;

  count_left -= unaligned_size;

  *dst_ptr = dst + unaligned_size;
  *src_ptr = src + unaligned_size;

  return FPGA_OK;
}

static fpga_result clear_interrupt(fpga_dma_handle dma_h) {
  // clear interrupt by writing 1 to IRQ bit in status register
  msgdma_status_t status = {0};
  status.st.irq = 1;

  return MMIOWrite32Blk(dma_h, CSR_STATUS(dma_h), (uint64_t)&status.reg, sizeof(status.reg));
}

static fpga_result poll_interrupt(fpga_dma_handle dma_h) {
  struct pollfd pfd = {0};
  msgdma_status_t status = { 0 };
  fpga_result res = FPGA_OK;
  int poll_res;

  res = fpgaGetOSObjectFromEventHandle(dma_h->eh, &pfd.fd);
  ON_ERR_GOTO(res, out, "fpgaGetOSObjectFromEventHandle failed\n");

  pfd.events = POLLIN;

#ifdef CHECK_DELAYS
  if (0 == poll(&pfd, 1, 0)) poll_wait_count++;
#endif
  poll_res = poll(&pfd, 1, FPGA_DMA_TIMEOUT_MSEC);
  MMIORead32Blk(dma_h, CSR_STATUS(dma_h), (uint64_t)& status.reg, sizeof(status.reg));
  if (poll_res < 0) {
    fprintf(stderr, "Poll error errno = %s DMA status reg: 0x%x\n", strerror(errno), status.reg);
    res = FPGA_EXCEPTION;
    goto out;
  } else if (poll_res == 0) {
    fprintf(stderr, "Poll(interrupt) timeout DMA status reg: 0x%x\n", status.reg);
    res = FPGA_EXCEPTION;
  } else {
    uint64_t count = 0;
    ssize_t bytes_read = read(pfd.fd, &count, sizeof(count));
    if (bytes_read > 0) {
      debug_print("Poll success. Return = %d, count = %d\n", poll_res, (int)count);
      res = FPGA_OK;
    } else {
      fprintf(stderr, "Error: poll failed read: zero bytes read");
      res = FPGA_EXCEPTION;
    }
  }

out:
  clear_interrupt(dma_h);
  return res;
}

static fpga_result _issue_magic(fpga_dma_handle dma_h) {
  fpga_result res = FPGA_OK;
  *(dma_h->magic_buf) = 0x0ULL;

  res = _do_dma(dma_h,
                dma_h->magic_iova | FPGA_DMA_WF_HOST_MASK,
                FPGA_DMA_WF_ROM_MAGIC_NO_MASK,
                64,
                1,
                FPGA_TO_HOST_MM,
                FPGA2HOST_IRQ_REQ /*intr_en */);
  return res;
}

static void _wait_magic(fpga_dma_handle dma_h) {
#ifndef SKIP_FPGA2HOST_IRQ
  poll_interrupt(dma_h);
#endif
  while (*(dma_h->magic_buf) != FPGA_DMA_WF_MAGIC_NO)
    ;
  *(dma_h->magic_buf) = 0x0ULL;
}

fpga_result transferHostToFpga(
    fpga_dma_handle dma_h, uint64_t dst, uint64_t src, size_t count, fpga_dma_transfer_t type) {
  fpga_result res = FPGA_OK;
  uint64_t i = 0;
  uint64_t count_left = count;
  uint64_t aligned_addr = 0;
  uint64_t align_bytes = 0;
  int issued_intr = 0;
  debug_print("Host To Fpga ----------- src = %08lx, dst = %08lx \n", src, dst);
  if (!IS_DMA_ALIGNED(dst)) {
    if (count_left < FPGA_DMA_ALIGN_BYTES) {
      res = _ase_host_to_fpga(dma_h, &dst, &src, count_left);
      ON_ERR_GOTO(res, out, "HOST_TO_FPGA_MM Transfer failed\n");
      return res;
    } else {
      aligned_addr = ((dst / FPGA_DMA_ALIGN_BYTES) + 1) * FPGA_DMA_ALIGN_BYTES;
      align_bytes = aligned_addr - dst;
      res = _ase_host_to_fpga(dma_h, &dst, &src, align_bytes);
      ON_ERR_GOTO(res, out, "HOST_TO_FPGA_MM Transfer failed\n");
      count_left = count_left - align_bytes;
    }
  }
  if (count_left) {
    uint32_t dma_chunks = count_left / FPGA_DMA_BUF_SIZE;
    count_left -= (dma_chunks * FPGA_DMA_BUF_SIZE);
    debug_print(
        "DMA TX : dma chuncks = %d, count_left = %08lx, dst = %08lx, src = %08lx \n", dma_chunks, count_left, dst, src);

    for (i = 0; i < dma_chunks; i++) {
      // constant size transfer, no length check required for memcpy
      memcpy_s_fast(dma_h->dma_buf_ptr[i % FPGA_DMA_MAX_BUF],
                    FPGA_DMA_BUF_SIZE,
                    (void *)(src + i * FPGA_DMA_BUF_SIZE),
                    FPGA_DMA_BUF_SIZE);
      // The value of FPGA_DMA_MAX_BUF is 2. Thus FPGA_DMA_MAX_BUF/2 -- 1, so the comparison
      // is always i % 1 == 0, which will always be true. This means that the i == (dma_chunks -1)
      // portion of the conditional will never be reached. However, for clarity and in case
      // FPGA_DMA_MAX_BUF changes, I will leave the conditional as is and apply a coverity supression
      // coverity[deadcode:FALSE]
      if ((i % (FPGA_DMA_MAX_BUF / 2) == (FPGA_DMA_MAX_BUF / 2) - 1) || i == (dma_chunks - 1) /*last descriptor */) {
        if (i == (FPGA_DMA_MAX_BUF / 2) - 1) {
          res = _do_dma(dma_h,
                        (dst + i * FPGA_DMA_BUF_SIZE),
                        dma_h->dma_buf_iova[i % FPGA_DMA_MAX_BUF] | FPGA_DMA_HOST_MASK,
                        FPGA_DMA_BUF_SIZE,
                        0,
                        type,
                        true);
        } else {
          if (issued_intr) poll_interrupt(dma_h);
          res = _do_dma(dma_h,
                        (dst + i * FPGA_DMA_BUF_SIZE),
                        dma_h->dma_buf_iova[i % FPGA_DMA_MAX_BUF] | FPGA_DMA_HOST_MASK,
                        FPGA_DMA_BUF_SIZE,
                        0,
                        type,
                        true /*intr_en */);
        }
        issued_intr = 1;
      } else {
        res = _do_dma(dma_h,
                      (dst + i * FPGA_DMA_BUF_SIZE),
                      dma_h->dma_buf_iova[i % FPGA_DMA_MAX_BUF] | FPGA_DMA_HOST_MASK,
                      FPGA_DMA_BUF_SIZE,
                      0,
                      type,
                      false /*intr_en */);
      }
    }
    if (issued_intr) {
      poll_interrupt(dma_h);
      issued_intr = 0;
    }
    if (count_left) {
      uint64_t dma_tx_bytes = (count_left / FPGA_DMA_ALIGN_BYTES) * FPGA_DMA_ALIGN_BYTES;
      if (dma_tx_bytes != 0) {
        debug_print("dma_tx_bytes = %08lx  was transfered using DMA\n", dma_tx_bytes);
        if (dma_tx_bytes > FPGA_DMA_BUF_SIZE) {
          res = FPGA_NO_MEMORY;
          ON_ERR_GOTO(res, out, "Illegal transfer size\n");
        }

        memcpy_s_fast(
            dma_h->dma_buf_ptr[0], dma_tx_bytes, (void *)(src + dma_chunks * FPGA_DMA_BUF_SIZE), dma_tx_bytes);
        res = _do_dma(dma_h,
                      (dst + dma_chunks * FPGA_DMA_BUF_SIZE),
                      dma_h->dma_buf_iova[0] | FPGA_DMA_HOST_MASK,
                      dma_tx_bytes,
                      1,
                      type,
                      true /*intr_en */);
        ON_ERR_GOTO(res, out, "HOST_TO_FPGA_MM Transfer failed\n");
        poll_interrupt(dma_h);
      }
      count_left -= dma_tx_bytes;
      if (count_left) {
        dst = dst + dma_chunks * FPGA_DMA_BUF_SIZE + dma_tx_bytes;
        src = src + dma_chunks * FPGA_DMA_BUF_SIZE + dma_tx_bytes;
        res = _ase_host_to_fpga(dma_h, &dst, &src, count_left);
        ON_ERR_GOTO(res, out, "HOST_TO_FPGA_MM Transfer failed\n");
      }
    }
  }
out:
  return res;
}

fpga_result transferFpgaToHost(
    fpga_dma_handle dma_h, uint64_t dst, uint64_t src, size_t count, fpga_dma_transfer_t type) {
  fpga_result res = FPGA_OK;
  uint64_t i = 0;
  uint64_t j = 0;
  uint64_t count_left = count;
  uint64_t aligned_addr = 0;
  uint64_t align_bytes = 0;
  int wf_issued = 0;

  debug_print("FPGA To Host ----------- src = %08lx, dst = %08lx \n", src, dst);
  if (!IS_DMA_ALIGNED(src)) {
    if (count_left < FPGA_DMA_ALIGN_BYTES) {
      res = _ase_fpga_to_host(dma_h, &src, &dst, count_left);
      ON_ERR_GOTO(res, out, "FPGA_TO_HOST_MM Transfer failed");
      return res;
    } else {
      aligned_addr = ((src / FPGA_DMA_ALIGN_BYTES) + 1) * FPGA_DMA_ALIGN_BYTES;
      align_bytes = aligned_addr - src;
      res = _ase_fpga_to_host(dma_h, &src, &dst, align_bytes);
      ON_ERR_GOTO(res, out, "FPGA_TO_HOST_MM Transfer failed");
      count_left = count_left - align_bytes;
    }
  }
  if (count_left) {
    uint32_t dma_chunks = count_left / FPGA_DMA_BUF_SIZE;
    count_left -= (dma_chunks * FPGA_DMA_BUF_SIZE);
    debug_print(
        "DMA TX : dma chunks = %d, count_left = %08lx, dst = %08lx, src = %08lx \n", dma_chunks, count_left, dst, src);
    uint64_t pending_buf = 0;
    for (i = 0; i < dma_chunks; i++) {
      res = _do_dma(dma_h,
                    dma_h->dma_buf_iova[i % (FPGA_DMA_MAX_BUF)] | FPGA_DMA_HOST_MASK,
                    (src + i * FPGA_DMA_BUF_SIZE),
                    FPGA_DMA_BUF_SIZE,
                    1,
                    type,
                    false /*intr_en */);
      ON_ERR_GOTO(res, out, "FPGA_TO_HOST_MM Transfer failed");

      const int num_pending = i - pending_buf + 1;
      if (num_pending == (FPGA_DMA_MAX_BUF / 2)) {  // Enters this loop only once,after first batch of descriptors.
        res = _issue_magic(dma_h);
        ON_ERR_GOTO(res, out, "Magic number issue failed");
        wf_issued = 1;
      }
      if (num_pending > (FPGA_DMA_MAX_BUF - 1) || i == (dma_chunks - 1) /*last descriptor */) {
        if (wf_issued) {
          _wait_magic(dma_h);
          for (j = 0; j < (FPGA_DMA_MAX_BUF / 2); j++) {
            // constant size transfer; no length check required
            memcpy_s_fast((void *)(dst + pending_buf * FPGA_DMA_BUF_SIZE),
                          FPGA_DMA_BUF_SIZE,
                          dma_h->dma_buf_ptr[pending_buf % (FPGA_DMA_MAX_BUF)],
                          FPGA_DMA_BUF_SIZE);
            pending_buf++;
          }
          wf_issued = 0;
        }
        res = _issue_magic(dma_h);
        ON_ERR_GOTO(res, out, "Magic number issue failed");
        wf_issued = 1;
      }
    }

    if (wf_issued) _wait_magic(dma_h);

    // clear out final dma memcpy operations
    while (pending_buf < dma_chunks) {
      // constant size transfer; no length check required
      memcpy_s_fast((void *)(dst + pending_buf * FPGA_DMA_BUF_SIZE),
                    FPGA_DMA_BUF_SIZE,
                    dma_h->dma_buf_ptr[pending_buf % (FPGA_DMA_MAX_BUF)],
                    FPGA_DMA_BUF_SIZE);
      pending_buf++;
    }
    if (count_left > 0) {
      uint64_t dma_tx_bytes = (count_left / FPGA_DMA_ALIGN_BYTES) * FPGA_DMA_ALIGN_BYTES;
      if (dma_tx_bytes != 0) {
        debug_print("dma_tx_bytes = %08lx  was transfered using DMA\n", dma_tx_bytes);
        res = _do_dma(dma_h,
                      dma_h->dma_buf_iova[0] | FPGA_DMA_HOST_MASK,
                      (src + dma_chunks * FPGA_DMA_BUF_SIZE),
                      dma_tx_bytes,
                      1,
                      type,
                      false /*intr_en */);
        ON_ERR_GOTO(res, out, "FPGA_TO_HOST_MM Transfer failed");
        res = _issue_magic(dma_h);
        ON_ERR_GOTO(res, out, "Magic number issue failed");
        _wait_magic(dma_h);
        if (dma_tx_bytes > FPGA_DMA_BUF_SIZE) {
          res = FPGA_NO_MEMORY;
          ON_ERR_GOTO(res, out, "Illegal transfer size\n");
        }
        memcpy_s_fast(
            (void *)(dst + dma_chunks * FPGA_DMA_BUF_SIZE), dma_tx_bytes, dma_h->dma_buf_ptr[0], dma_tx_bytes);
      }
      count_left -= dma_tx_bytes;
      if (count_left) {
        dst = dst + dma_chunks * FPGA_DMA_BUF_SIZE + dma_tx_bytes;
        src = src + dma_chunks * FPGA_DMA_BUF_SIZE + dma_tx_bytes;
        res = _ase_fpga_to_host(dma_h, &src, &dst, count_left);
        ON_ERR_GOTO(res, out, "FPGA_TO_HOST_MM Transfer failed");
      }
    }
  }
out:
  return res;
}

fpga_result transferFpgaToFpga(
    fpga_dma_handle dma_h, uint64_t dst, uint64_t src, size_t count, fpga_dma_transfer_t type) {
  fpga_result res = FPGA_OK;
  uint64_t i = 0;
  uint64_t count_left = count;
  uint64_t *tmp_buf = NULL;
  if (IS_DMA_ALIGNED(dst) && IS_DMA_ALIGNED(src) && IS_DMA_ALIGNED(count_left)) {
    uint32_t dma_chunks = count_left / FPGA_DMA_BUF_SIZE;
    count_left -= (dma_chunks * FPGA_DMA_BUF_SIZE);
    debug_print("!!!FPGA to FPGA!!! TX :dma chunks = %d, count = %08lx, dst = %08lx, src = %08lx \n",
                dma_chunks,
                count_left,
                dst,
                src);

    for (i = 0; i < dma_chunks; i++) {
      res = _do_dma(dma_h,
                    (dst + i * FPGA_DMA_BUF_SIZE),
                    (src + i * FPGA_DMA_BUF_SIZE),
                    FPGA_DMA_BUF_SIZE,
                    0,
                    type,
                    false /*intr_en */);
      ON_ERR_GOTO(res, out, "FPGA_TO_FPGA_MM Transfer failed");
      if ((i + 1) % FPGA_DMA_MAX_BUF == 0 || i == (dma_chunks - 1) /*last descriptor */) {
        res = _issue_magic(dma_h);
        ON_ERR_GOTO(res, out, "Magic number issue failed");
        _wait_magic(dma_h);
      }
    }
    if (count_left > 0) {
      debug_print("Count_left = %08lx  was transfered using DMA\n", count_left);
      res = _do_dma(dma_h,
                    (dst + dma_chunks * FPGA_DMA_BUF_SIZE),
                    (src + dma_chunks * FPGA_DMA_BUF_SIZE),
                    count_left,
                    1,
                    type,
                    false /*intr_en */);
      ON_ERR_GOTO(res, out, "FPGA_TO_FPGA_MM Transfer failed");
      res = _issue_magic(dma_h);
      ON_ERR_GOTO(res, out, "Magic number issue failed");
      _wait_magic(dma_h);
    }
  } else {
    if ((src < dst) && (src + count_left >= dst)) {
      debug_print("Overlapping addresses, Provide correct dst address\n");
      return FPGA_NOT_SUPPORTED;
    }
    uint32_t tx_chunks = count_left / FPGA_DMA_BUF_ALIGN_SIZE;
    count_left -= (tx_chunks * FPGA_DMA_BUF_ALIGN_SIZE);
    debug_print("!!!FPGA to FPGA TX!!! : tx chunks = %d, count = %08lx, dst = %08lx, src = %08lx \n",
                tx_chunks,
                count_left,
                dst,
                src);
    tmp_buf = (uint64_t *)malloc(FPGA_DMA_BUF_ALIGN_SIZE);
    for (i = 0; i < tx_chunks; i++) {
      res = transferFpgaToHost(
          dma_h, (uint64_t)tmp_buf, (src + i * FPGA_DMA_BUF_ALIGN_SIZE), FPGA_DMA_BUF_ALIGN_SIZE, FPGA_TO_HOST_MM);
      ON_ERR_GOTO(res, out_spl, "FPGA_TO_FPGA_MM Transfer failed");
      res = transferHostToFpga(
          dma_h, (dst + i * FPGA_DMA_BUF_ALIGN_SIZE), (uint64_t)tmp_buf, FPGA_DMA_BUF_ALIGN_SIZE, HOST_TO_FPGA_MM);
      ON_ERR_GOTO(res, out_spl, "FPGA_TO_FPGA_MM Transfer failed");
    }
    if (count_left > 0) {
      res = transferFpgaToHost(
          dma_h, (uint64_t)tmp_buf, (src + tx_chunks * FPGA_DMA_BUF_ALIGN_SIZE), count_left, FPGA_TO_HOST_MM);
      ON_ERR_GOTO(res, out_spl, "FPGA_TO_FPGA_MM Transfer failed");
      res = transferHostToFpga(
          dma_h, (dst + tx_chunks * FPGA_DMA_BUF_ALIGN_SIZE), (uint64_t)tmp_buf, count_left, HOST_TO_FPGA_MM);
      ON_ERR_GOTO(res, out_spl, "FPGA_TO_FPGA_MM Transfer failed");
    }
    free(tmp_buf);
  }
out:
  return res;
out_spl:
  free(tmp_buf);
  return res;
}

fpga_result fpgaDmaTransferSync(
    fpga_dma_handle dma_h, uint64_t dst, uint64_t src, size_t count, fpga_dma_transfer_t type) {
  fpga_result res = FPGA_OK;

  if (!dma_h) return FPGA_INVALID_PARAM;

  if (type >= FPGA_MAX_TRANSFER_TYPE) return FPGA_INVALID_PARAM;

  if (!dma_h->fpga_h) return FPGA_INVALID_PARAM;

  if (type == HOST_TO_FPGA_MM) {
    res = transferHostToFpga(dma_h, dst, src, count, HOST_TO_FPGA_MM);
  } else if (type == FPGA_TO_HOST_MM) {
    res = transferFpgaToHost(dma_h, dst, src, count, FPGA_TO_HOST_MM);
  } else if (type == FPGA_TO_FPGA_MM) {
    res = transferFpgaToFpga(dma_h, dst, src, count, FPGA_TO_FPGA_MM);
  } else {
    // Should not be possible, since we have handled all fpga_dma_transfer_t types
    assert(0);
  }

  return res;
}

fpga_result fpgaDmaTransferAsync(fpga_dma_handle dma,
                                 uint64_t dst,
                                 uint64_t src,
                                 size_t count,
                                 fpga_dma_transfer_t type,
                                 fpga_dma_transfer_cb cb,
                                 void *context) {
  // TODO
  return FPGA_NOT_SUPPORTED;
}

fpga_result fpgaDmaClose(fpga_dma_handle dma_h) {
  fpga_result res = FPGA_OK;
  int i = 0;
  if (!dma_h) {
    res = FPGA_INVALID_PARAM;
    goto out;
  }

  if (!dma_h->fpga_h) {
    res = FPGA_INVALID_PARAM;
    goto out;
  }

  for (i = 0; i < FPGA_DMA_MAX_BUF; i++) {
    res = fpgaReleaseBuffer(dma_h->fpga_h, dma_h->dma_buf_wsid[i]);
    ON_ERR_GOTO(res, out, "fpgaReleaseBuffer failed");
  }

  res = fpgaReleaseBuffer(dma_h->fpga_h, dma_h->magic_wsid);
  ON_ERR_GOTO(res, out, "fpgaReleaseBuffer");

  fpgaUnregisterEvent(dma_h->fpga_h, FPGA_EVENT_INTERRUPT, dma_h->eh);
  fpgaDestroyEventHandle(&dma_h->eh);

  // turn off global interrupts
  msgdma_ctrl_t ctrl = {0};
  ctrl.ct.global_intr_en_mask = 0;
  res = MMIOWrite32Blk(dma_h, CSR_CONTROL(dma_h), (uint64_t)&ctrl.reg, sizeof(ctrl.reg));
  ON_ERR_GOTO(res, out, "MMIOWrite32Blk");

out:
  free((void *)dma_h);
  return res;
}
