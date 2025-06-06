// Copyright 2020 Altera Corporation.
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

#pragma once

// save a copy
#pragma push_macro("localparam")

// convert the syntax of verilog into C++, replace "localparam int MY_VAR = 123;" with "constexpr int MY_VAR = 123;"
#undef localparam
#define localparam constexpr

// include the verilog header
#include "dla_dma_constants.svh"

// undo the syntax change
#pragma pop_macro("localparam")
