/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "dl/api/omxtypes.h"
#include "dl/sp/api/mipsSP.h"
#include "dl/sp/api/omxSP.h"

OMXResult omxSP_FFTGetBufSize_R_F32(OMX_INT order, OMX_INT* pSize) {
  OMX_INT fft_size;

  if (!pSize || (order < 1) || (order > TWIDDLE_TABLE_ORDER))
    return OMX_Sts_BadArgErr;

  /* For order larger than 4, compute Real FFT as Complex FFT of (order - 1). */
  if (order > 4)
    fft_size = 1 << (order - 1);
  else
    fft_size = 1 << order;

  *pSize = sizeof(MIPSFFTSpec_R_FC32) +
           /* BitRev Table. */
           sizeof(OMX_U16) * fft_size +
           /* BitRevInv Table. */
           sizeof(OMX_U16) * fft_size +
           /* Offsets table. */
           sizeof(OMX_U16) *
               ((SUBTRANSFORM_CONST >> (16 - TWIDDLE_TABLE_ORDER)) | 1) +
           /* pBuf. */
           sizeof(OMX_F32) * (fft_size << 1) +
           /*
            * Extra bytes to get 32 byte alignment of
            * pBitRev, pBitRevInv, pOffset and pBuf.
            */
           124;

  return OMX_Sts_NoErr;
}
