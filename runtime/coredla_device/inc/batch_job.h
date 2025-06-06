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

#ifndef BATCH_JOB_H
#define BATCH_JOB_H

class BatchJob {
 public:
  // @param inputArray - ptr to CPU array containing input data to be copied to DDR
  // blocking function
  virtual void LoadInputFeatureToDDR(void* inputArray) = 0;
  // @param outputArray - ptr to CPU array where the output data in DDR is copied into
  // outputArray must be allocated by the caller (size >= output_size_ddr)
  // blocking function
  virtual void ReadOutputFeatureFromDDR(void* outputArray) const = 0;
  virtual void ScheduleInputFeature() const = 0;
  virtual void StartDla() = 0;
  virtual ~BatchJob() {}
};

#endif
