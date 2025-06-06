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

#include "coredla_batch_job.h"  //CoreDlaBatchJob
#include "dla_dma_constants.h"  //DLA_DMA_CSR_OFFSET_***
#include "stream_controller_comms.h"

static constexpr int CONFIG_READER_DATA_BYTES = 8;

std::unique_ptr<BatchJob> CoreDlaBatchJob::MakeUnique(MmdWrapper* mmdWrapper,
                                                      uint64_t totalConfigWords,
                                                      uint64_t configBaseAddrDDR,
                                                      uint64_t inputAddrDDR,
                                                      uint64_t outputAddrDDR,
                                                      uint64_t inputSizeDDR,
                                                      uint64_t outputSizeDDR,
                                                      const bool enableIstream,
                                                      const bool enableOstream,
                                                      int instance,
                                                      std::shared_ptr<StreamControllerComms> spStreamControllerComms) {
  return std::unique_ptr<BatchJob>(new CoreDlaBatchJob(mmdWrapper,
                                                       totalConfigWords,
                                                       configBaseAddrDDR,
                                                       inputAddrDDR,
                                                       outputAddrDDR,
                                                       inputSizeDDR,
                                                       outputSizeDDR,
                                                       enableIstream,
                                                       enableOstream,
                                                       instance,
                                                       spStreamControllerComms));
}
CoreDlaBatchJob::CoreDlaBatchJob(MmdWrapper* mmdWrapper,
                                 uint64_t totalConfigWords,
                                 uint64_t configBaseAddrDDR,
                                 uint64_t inputAddrDDR,
                                 uint64_t outputAddrDDR,
                                 uint64_t inputSizeDDR,
                                 uint64_t outputSizeDDR,
                                 const bool enableIstream,
                                 const bool enableOstream,
                                 int instance,
                                 std::shared_ptr<StreamControllerComms> spStreamControllerComms)
: mmdWrapper_(mmdWrapper)
, instance_(instance)
, totalConfigWords_(totalConfigWords)
, configBaseAddrDDR_(configBaseAddrDDR)
, inputAddrDDR_(inputAddrDDR)
, outputAddrDDR_(outputAddrDDR)
, inputSizeDDR_(inputSizeDDR)
, outputSizeDDR_(outputSizeDDR)
, enableIstream_(enableIstream)
, enableOstream_(enableOstream)
, lastJobQueueNumber_(0)
, spStreamControllerComms_(spStreamControllerComms) {
}

// This function must be called by a single thread
// It can be called on a different thread than StartDla or WaitForDla
void CoreDlaBatchJob::LoadInputFeatureToDDR(void* inputArray) {
  mmdWrapper_->enableCSRLogger();
  mmdWrapper_->WriteToDDR(instance_, inputAddrDDR_, inputSizeDDR_, inputArray);
  mmdWrapper_->disableCSRLogger();
  StartDla();
}

void CoreDlaBatchJob::ScheduleInputFeature() const {
  if (spStreamControllerComms_) {
    // Send message to NIOS-V
    uint64_t configurationSize64 = (totalConfigWords_ / CONFIG_READER_DATA_BYTES) - 2;
    uint32_t configurationBaseAddressDDR = static_cast<uint32_t>(configBaseAddrDDR_);
    uint32_t configurationSize = static_cast<uint32_t>(configurationSize64);
    uint32_t inputAddressDDR = static_cast<uint32_t>(inputAddrDDR_);
    uint32_t outputAddressDDR = static_cast<uint32_t>(outputAddrDDR_);

    Payload<CoreDlaJobPayload> item;
    item._configurationBaseAddressDDR = configurationBaseAddressDDR;
    item._configurationSize = configurationSize;
    item._inputAddressDDR = inputAddressDDR;
    item._outputAddressDDR = outputAddressDDR;

    spStreamControllerComms_->ScheduleItems( { item } );
  }
}

// This function must be called by a single thread
// It can be called on a different thread than WaitForDla or LoadInputFeatureToDDR
void CoreDlaBatchJob::StartDla() {
  //////////////////////////////////////
  //  Write to CSR to start the FPGA  //
  //////////////////////////////////////
  mmdWrapper_->enableCSRLogger();

  // interrupt mask was already enabled in the DlaDevice constructor

  // intermediate buffer address was already set when the graph was loaded

  // base address for config reader
  mmdWrapper_->WriteToCsr(instance_, DLA_DMA_CSR_OFFSET_CONFIG_BASE_ADDR, configBaseAddrDDR_);

  // how many words for config reader to read
  // hardware wants the number of words minus 2 since the implementation is a down counter which ends at -1, the sign
  // bit is used to denote the end of the counter range
  mmdWrapper_->WriteToCsr(instance_, DLA_DMA_CSR_OFFSET_CONFIG_RANGE_MINUS_TWO, (totalConfigWords_ / CONFIG_READER_DATA_BYTES) - 2);

  if (enableIstream_ && enableOstream_) {
    // Arm the streaming interface. Will continuously load configs.
    const unsigned int enable = 1;
    mmdWrapper_->WriteToCsr(instance_, DLA_CSR_OFFSET_READY_STREAMING_IFACE, enable);
  } else {
    // base address for feature reader -- this will trigger one run of DLA
    mmdWrapper_->WriteToCsr(instance_, DLA_DMA_CSR_OFFSET_INPUT_OUTPUT_BASE_ADDR, inputAddrDDR_);
  }
  mmdWrapper_->disableCSRLogger();
}

void CoreDlaBatchJob::ReadOutputFeatureFromDDR(void* outputArray) const {
  mmdWrapper_->enableCSRLogger();
  mmdWrapper_->ReadFromDDR(instance_, outputAddrDDR_, outputSizeDDR_, outputArray);
  mmdWrapper_->disableCSRLogger();
}
