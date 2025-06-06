// Copyright 2023 Altera Corporation.
//
// This software and the related documents are Altera copyrighted materials,
// and your use of them is governed by the express license under which they
// were provided to you ("License"). Unless the License provides otherwise,
// you may not use, modify, copy, publish, distribute, disclose or transmit
// this software or the related documents without Altera's prior written
// permission.
//
// This software and the related documents are provided as is, with no express
// or implied warranties, other than those that are expressly stated in the
// License.

#include "UioDevice.h"
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>

namespace UIO {
static const std::string uioDriverFolder = "/sys/class/uio";

std::shared_ptr<IDevice> IDevice::Load(const std::string& deviceName, uint32_t index) {
  std::vector<DeviceItem> deviceItems = GetDevices();
  std::string indexedDeviceName = deviceName + std::to_string(index);

  for (auto& deviceItem : deviceItems) {
    if (deviceItem._indexedName == indexedDeviceName) {
      auto spUioDevice = std::make_shared<Device>(deviceItem);
      return spUioDevice->IsValid() ? spUioDevice : nullptr;
    }
  }

  return nullptr;
}

std::vector<DeviceItem> IDevice::GetDevices() {
  std::vector<DeviceItem> deviceItems;

  for (const auto& entry : std::filesystem::directory_iterator(uioDriverFolder)) {
    // Filter out uio*
    if (entry.is_directory()) {
      std::filesystem::path filePath = entry.path();
      std::string stem = filePath.filename();
      if (stem.substr(0, 3) == "uio") {
        std::string indexedDeviceName = Device::ReadStringFromFile(filePath / "name");
        if (not indexedDeviceName.empty()) {
          std::string deviceName;
          uint32_t index = 0;
          Device::SplitIndexedDeviceName(indexedDeviceName, deviceName, index);
          deviceItems.push_back({deviceName, index, indexedDeviceName, filePath});
        }
      }
    }
  }

  return deviceItems;
}

///////////////////////////////////////////////////////////////////////////

Device::Device(const DeviceItem& deviceItem) : _deviceItem(deviceItem) { MapRegion(); }

Device::~Device() { UnmapRegion(); }

bool Device::IsValid() { return (_fd >= 0); }

uint32_t Device::Read(uint32_t registerIndex) {
  if (registerIndex >= _maximumRegisterIndex) return 0;

  uint32_t* pRegister = (uint32_t*)_pPtr;

  uint32_t value = pRegister[registerIndex];
  return value;
}

void Device::Write(uint32_t registerIndex, uint32_t value) {
  if (registerIndex < _maximumRegisterIndex) {
    uint32_t* pRegister = (uint32_t*)_pPtr;
    pRegister[registerIndex] = value;
  }
}

void Device::ReadBlock(void* pHostDestination, size_t offset, size_t size) {
  if ((offset + size) < _size) {
    uint8_t* pDeviceMem = (uint8_t*)_pPtr + offset;
    ::memcpy(pHostDestination, pDeviceMem, size);
  }
}

void Device::WriteBlock(const void* pHostSourceAddress, size_t offset, size_t size) {
  if ((offset + size) < _size) {
    uint8_t* pDeviceMem = (uint8_t*)_pPtr + offset;
    ::memcpy(pDeviceMem, pHostSourceAddress, size);
  }
}

uint64_t Device::ReadValueFromFile(const std::filesystem::path& path) {
  std::string line = ReadStringFromFile(path);
  int base = (line.substr(0, 2) == "0x") ? 16 : 10;
  return std::stoull(line, nullptr, base);
}

std::string Device::ReadStringFromFile(const std::filesystem::path& path) {
  std::ifstream inputStream(path);
  if (inputStream.good()) {
    std::string line;
    std::getline(inputStream, line);
    return line;
  }
  return "";
}

bool Device::MapRegion() {
  _size = ReadValueFromFile(_deviceItem._rootPath / "maps/map0/size");
  _offset = ReadValueFromFile(_deviceItem._rootPath / "maps/map0/offset");
  _physicalAddress = ReadValueFromFile(_deviceItem._rootPath / "maps/map0/addr");
  _maximumRegisterIndex = _size / sizeof(uint32_t);

  std::filesystem::path uioDevicePath = "/dev";
  std::filesystem::path uioDeviceId = _deviceItem._rootPath.stem();
  uioDevicePath /= uioDeviceId;

  _fd = ::open(uioDevicePath.c_str(), O_RDWR);
  if (_fd < 0) {
    return false;
  }

  // Map the region into userspace
  _pBase = (uint8_t*)::mmap(NULL, (size_t)_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
  if (_pBase == MAP_FAILED) {
    return false;
  }

  // CST base address is at _pBase + _offset
  _pPtr = (uint32_t*)(_pBase + _offset);

  return true;
};

void Device::UnmapRegion() {
  int r = 0;
  if (_pBase) {
    r = ::munmap(_pBase, _size);
    _pBase = nullptr;
  }

  if (_fd >= 0) {
    r = ::close(_fd);
    _fd = -1;
  }
  (void)r;
}

void Device::SplitIndexedDeviceName(const std::string& indexedDeviceName, std::string& deviceName, uint32_t& index) {
  int32_t len = static_cast<int32_t>(indexedDeviceName.length());
  int32_t nDecimals = 0;
  for (int32_t i = (len - 1); i >= 0; i--) {
    if (::isdigit(indexedDeviceName[i])) nDecimals++;
  }

  deviceName = indexedDeviceName.substr(0, len - nDecimals);
  index = std::stoul(indexedDeviceName.substr(len - nDecimals));
}

}  // namespace UIO
