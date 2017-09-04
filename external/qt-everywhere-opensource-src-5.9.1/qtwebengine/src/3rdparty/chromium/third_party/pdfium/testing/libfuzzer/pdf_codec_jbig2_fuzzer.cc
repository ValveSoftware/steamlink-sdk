// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcodec/JBig2_DocumentContext.h"
#include "core/fxcodec/codec/ccodec_jbig2module.h"
#include "core/fxcodec/jbig2/JBig2_Context.h"
#include "core/fxge/fx_dib.h"

static uint32_t GetInteger(const uint8_t* data) {
  return data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const size_t kParameterSize = 8;
  if (size < kParameterSize)
    return 0;

  uint32_t width = GetInteger(data);
  uint32_t height = GetInteger(data + 4);
  size -= kParameterSize;
  data += kParameterSize;

  std::unique_ptr<CFX_DIBitmap> bitmap(new CFX_DIBitmap);
  if (!bitmap->Create(width, height, FXDIB_1bppRgb))
    return 0;

  std::unique_ptr<CPDF_Object> stream(new CPDF_Stream);
  stream->AsStream()->SetData(data, size);
  CPDF_StreamAcc src_stream;
  src_stream.LoadAllData(stream->AsStream(), true);

  CCodec_Jbig2Module module;
  CCodec_Jbig2Context jbig2_context;
  std::unique_ptr<JBig2_DocumentContext> document_context;
  FXCODEC_STATUS status = module.StartDecode(
      &jbig2_context, &document_context, width, height, &src_stream, nullptr,
      bitmap->GetBuffer(), bitmap->GetPitch(), nullptr);

  while (status == FXCODEC_STATUS_DECODE_TOBECONTINUE)
    status = module.ContinueDecode(&jbig2_context, nullptr);
  return 0;
}
