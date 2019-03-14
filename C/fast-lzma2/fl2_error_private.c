/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 * Modified for FL2 by Conor McCarthy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* The purpose of this file is to have a single list of error strings embedded in binary */

#include "fl2_error_private.h"

const char* ERR_getErrorString(ERR_enum code)
{
    static const char* const notErrorCode = "Unspecified error code";
    switch( code )
    {
    case PREFIX(no_error): return "No error detected";
    case PREFIX(GENERIC):  return "Error (generic)";
    case PREFIX(corruption_detected): return "Corrupted block detected";
    case PREFIX(checksum_wrong): return "Restored data doesn't match checksum";
    case PREFIX(parameter_unsupported): return "Unsupported parameter";
    case PREFIX(parameter_outOfBound): return "Parameter is out of bound";
    case PREFIX(lclpMax_exceeded): return "Parameters lc+lp > 4";
    case PREFIX(stage_wrong): return "Not possible at this stage of encoding";
    case PREFIX(init_missing): return "Context should be init first";
    case PREFIX(memory_allocation): return "Allocation error : not enough memory";
    case PREFIX(dstSize_tooSmall): return "Destination buffer is too small";
    case PREFIX(srcSize_wrong): return "Src size is incorrect";
    case PREFIX(canceled): return "Processing was canceled by a call to FL2_cancelCStream() or FL2_cancelDStream()";
    case PREFIX(buffer): return "Streaming progress halted due to buffer(s) full/empty";
    case PREFIX(timedOut): return "Wait timed out. Timeouts should be handled before errors using FL2_isTimedOut()";
        /* following error codes are not stable and may be removed or changed in a future version */
    case PREFIX(maxCode):
    default: return notErrorCode;
    }
}
