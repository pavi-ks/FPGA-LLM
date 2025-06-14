#ifndef ACL_PCIE_TIMER_H
#define ACL_PCIE_TIMER_H

/* (c) 1992-2021 Intel Corporation.                             */
/* Intel, the Intel logo, Intel, MegaCore, NIOS II, Quartus and TalkBack words     */
/* and logos are trademarks of Intel Corporation or its subsidiaries in the U.S.   */
/* and/or other countries. Other marks and brands may be claimed as the property   */
/* of others. See Trademarks on intel.com for full list of Intel trademarks or     */
/* the Trademarks & Brands Names Database (if Intel) or See www.Intel.com/legal (if Altera)  */
/* Your use of Intel Corporation's design tools, logic functions and other         */
/* software and tools, and its AMPP partner logic functions, and any output        */
/* files any of the foregoing (including device programming or simulation          */
/* files), and any associated documentation or information are expressly subject   */
/* to the terms and conditions of the Altera Program License Subscription          */
/* Agreement, Intel MegaCore Function License Agreement, or other applicable       */
/* license agreement, including, without limitation, that your use is for the      */
/* sole purpose of programming logic devices manufactured by Intel and sold by     */
/* Intel or its authorized distributors.  Please refer to the applicable           */
/* agreement for further details.                                                  */

/* ===- acl_pcie_timer.h  --------------------------------------------- C++ -*-=== */
/*                                                                                 */
/*                         Intel(R) OpenCL MMD Driver                                */
/*                                                                                 */
/* ===-------------------------------------------------------------------------=== */
/*                                                                                 */
/* This file declares the class to query the host's system timer.                  */
/* The actual implementation of the class lives in the acl_pcie_timer.cpp          */
/*                                                                                 */
/* ===-------------------------------------------------------------------------=== */

#ifdef DLA_MMD
// don't assume opencl has been installed
#include "acl_pcie.h"
typedef UINT64 cl_ulong;
#endif

class ACL_PCIE_TIMER {
 public:
  ACL_PCIE_TIMER();
  ~ACL_PCIE_TIMER();

  // function to query the host's system timer
  cl_ulong get_time_ns();

 private:
  INT64 m_ticks_per_second;
};

#endif  // ACL_PCIE_TIMER_H
