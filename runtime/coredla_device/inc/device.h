// Copyright 2020-2023 Altera Corporation.
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

#ifndef DEVICE_H
#define DEVICE_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dla_runtime_log.h"

using namespace std;
using DebugNetworkData = std::map<std::string, uint64_t>;

// dla log macro
#define DLA_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__);
#define DLA_ERROR(fmt, ...) printf(fmt, ##__VA_ARGS__);

class GraphJob;
class arch_params;
namespace dla {
class CompiledResult;
}
class Device {
 public:
  // Not all arguments of Device::MakeUnique are used by the definitions in derived objects
  // Parameters                       archParams          waitForDlaTimeoutSeconds            enableLogging
  // CoreDlaDevice::CoreDlaDevice     Not Used            Used                                Used
  // RefDevice::RefDevice             Used                Not Used                            Not Used
  // RawDevice::RawDevice             Used                Not Used                            Not Used
  static unique_ptr<Device> MakeUnique(const arch_params* archParams, uint32_t waitForDlaTimeoutSeconds, bool enableLogging=false);
  virtual GraphJob* CreateGraphJob(const dla::CompiledResult* compiledResult,
                                   size_t numPipelines,
                                   int instance,
                                   std::string AES_key,
                                   std::string IV_key,
                                   bool encryption_enabled,
                                   const std::string export_dir,
                                   const std::string parameter_rom_export_dir) = 0;
  // Return number of DLA jobs completed till now
  // Used for debugging
  virtual int GetNumInferencesCompleted(int instance) const = 0;
  // Must be called when there are no active jobs on DLA
  // Returns the total time taken by DLA jobs on hardware (in milliseconds)
  virtual double GetActiveHWTimeMs(int instance) const = 0;
  // Must be called when there are no active jobs on DLA
  // Returns the average of time taken per job (in milliseconds)
  // Avg Time per job < Active Time
  virtual double GetAvgHWTimePerJobMs(uint64_t num_jobs, int instance) const = 0;
  // Must be called when there are no active jobs on DLA
  // Returns the number of memory read made by the input feature reader
  virtual uint64_t GetNumInputFeatureMemoryReads(int instance) const = 0;
  // Must be called when there are no active jobs on DLA
  // Returns the number of memory read made by the filter reader
  virtual uint64_t GetNumFilterMemoryReads(int instance) const = 0;
  // Must be called when there are no active jobs on DLA
  // Returns the number of memory writes made by the output feature writer
  virtual uint64_t GetNumOutputFeatureMemoryWrites(int instance) const = 0;
  // Waits for a job to finish on specified instance
  virtual void WaitForDla(int instance, size_t threadId = 0, std::function<bool()> isCancelled = nullptr) = 0;
  virtual int GetNumInstances() const = 0;
  virtual double GetCoreDlaClockFreq() const = 0;
  virtual int GetSizeCsrDescriptorQueue() const = 0;
  virtual std::string SchedulerGetStatus() const = 0;
  virtual bool InitializeScheduler(uint32_t sourceBufferSize,
                                   uint32_t dropSourceBuffers,
                                   uint32_t numInferenceRequests,
                                   const std::string source_fifo_file="") = 0;
  virtual DebugNetworkData ReadDebugNetwork(int instance) const = 0;
  virtual ~Device(){}
};

#endif  // DEVICE_H
