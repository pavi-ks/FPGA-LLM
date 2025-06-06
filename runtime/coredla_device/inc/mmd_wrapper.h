// Copyright 2020 Intel Corporation.
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

#pragma once

#include <cstdint>  //uint32_t
#include <string>

using interrupt_service_routine_signature = void (*)(int handle, void *data);

enum class MmdLogLevel {
  DISABLE,
  ENABLE,
  INTERNAL
};

class MmdWrapper {
 public:
  MmdWrapper(bool enableLog=false);
  // Note that ~MmdWrapper() can call std::exit(1) if aocl_mmd_close()
  // fails.  Ideally we would find some way to re-order the code so that it
  // can throw an exception (before calling the destructor) if aocl_mmd_close()
  // fails.
  ~MmdWrapper();

  // class cannot be copied
  MmdWrapper(const MmdWrapper &) = delete;
  MmdWrapper &operator=(const MmdWrapper &) = delete;

  // Register a function to run as the interrupt service routine
  void RegisterISR(interrupt_service_routine_signature func, void *data) const;

  // 32-bit handshake with each CSR
  void WriteToCsr(int instance, uint32_t addr, uint32_t data) const;
  uint32_t ReadFromCsr(int instance, uint32_t addr) const;

  // Copy data between host and device memory
  void WriteToDDR(int instance, uint64_t addr, uint64_t length, const void *data) const;
  void ReadFromDDR(int instance, uint64_t addr, uint64_t length, void *data) const;

  // If the mmd layer supports accesses to the STREAM CONTROLLER
  bool bIsStreamControllerValid(int instance) const;

  // 32-bit handshake with each Stream Controller CSR
  void WriteToStreamController(int instance, uint32_t addr, uint64_t length, const void *data) const;
  void ReadFromStreamController(int instance, uint32_t addr, uint64_t length, void *data) const;

  // Provide read-only access to board-specific constants
  int GetMaxInstances() const { return maxInstances_; }
  uint64_t GetDDRSizePerInstance() const { return ddrSizePerInstance_; }
  double GetCoreDlaClockFreq() const { return coreDlaClockFreq_; }
  double GetDDRClockFreq() const { return ddrClockFreq_; }

  // (linqiaol) CSR logging control. Only useful for hostless EDs for now
  void enableCSRLogger();
  void disableCSRLogger();

 private:
  int handle_;
  int maxInstances_;
  uint64_t ddrSizePerInstance_;
  double coreDlaClockFreq_;
  double ddrClockFreq_;
  MmdLogLevel logLevel_;
};
