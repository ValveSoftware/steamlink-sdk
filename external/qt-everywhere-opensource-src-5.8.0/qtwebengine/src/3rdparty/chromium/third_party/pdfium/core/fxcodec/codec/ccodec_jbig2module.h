// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_CODEC_CCODEC_JBIG2MODULE_H_
#define CORE_FXCODEC_CODEC_CCODEC_JBIG2MODULE_H_

#include <memory>

#include "core/fxcrt/include/fx_basic.h"

class CPDF_StreamAcc;
class IFX_Pause;

class CCodec_Jbig2Module {
 public:
  CCodec_Jbig2Module() {}
  ~CCodec_Jbig2Module();

  void* CreateJbig2Context();
  FXCODEC_STATUS StartDecode(void* pJbig2Context,
                             std::unique_ptr<CFX_Deletable>* pContextHolder,
                             uint32_t width,
                             uint32_t height,
                             CPDF_StreamAcc* src_stream,
                             CPDF_StreamAcc* global_stream,
                             uint8_t* dest_buf,
                             uint32_t dest_pitch,
                             IFX_Pause* pPause);
  FXCODEC_STATUS ContinueDecode(void* pJbig2Context, IFX_Pause* pPause);
  void DestroyJbig2Context(void* pJbig2Context);
};

#endif  // CORE_FXCODEC_CODEC_CCODEC_JBIG2MODULE_H_
