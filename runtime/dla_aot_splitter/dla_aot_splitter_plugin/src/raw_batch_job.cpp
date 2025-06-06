// Copyright 2022 Altera Corporation.
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

/*
  The raw_batch_job, raw_graph_job, and raw_device implement the interfaces
  used by dliaPlugin to mimic a inference flow without actually providing a
  inference. It is used to get the transformed input performed by the dliaPlugin
  upper layers
*/

#include "raw_batch_job.h"
#include "dla_aot_utils.h"

unique_ptr<BatchJob> RawBatchJob::MakeUnique(const CompiledResult * compiledResult,
                            DLAInput* dlaBuffers,
                            int instance,
                            uint32_t debugLevel,
                            std::string AES_key,
                            std::string IV_key,
                            bool encryption_enabled) {
    return unique_ptr<BatchJob>(new RawBatchJob(compiledResult, dlaBuffers, instance, debugLevel, AES_key, IV_key, encryption_enabled));
}

RawBatchJob::RawBatchJob(const CompiledResult * compiledResult,
        DLAInput* dlaBuffers,
        int instance,
        uint32_t debugLevel,
        std::string AES_key,
        std::string IV_key,
        bool encryption_enabled) : compiledResult(compiledResult) {
  dlaBuffers_ = dlaBuffers;
  instance_ = instance;
  debugLevel_= debugLevel;
  AES_key_ = AES_key;
  IV_key_ = IV_key;
  encryption_enabled_ = encryption_enabled;
  output_.output_feature_buffer = new uint8_t[dlaBuffers_->output_feature_buffer_size];
  memset(output_.output_feature_buffer, 0, dlaBuffers_->output_feature_buffer_size);
  assert(nullptr != output_.output_feature_buffer);
}

// Emulation device has no DDR. This function is just storing a pointer to the array
// Note: inputAray should not be deleted until the end of the Emulation runs
// i.e. StartDla completes
void RawBatchJob::LoadInputFeatureToDDR(void* inputArray) {
  dlaBuffers_->input_feature_buffer = (uint8_t*) inputArray;
  StartDla();
}

void RawBatchJob::StartDla() {
  // Write input / output buffers to files
  writeInputOutputToFiles(compiledResult->get_arch_hash(), compiledResult->get_build_version_string(), compiledResult->get_arch_name(), *dlaBuffers_, output_);
}

// Emulation device has no DDR. Output is copied into the outputArray.
void RawBatchJob::ReadOutputFeatureFromDDR(void* outputArray) const {
  memcpy(outputArray, output_.output_feature_buffer, dlaBuffers_->output_feature_buffer_size);
}
