// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_ONED_BC_ONEDCODABARWRITER_H_
#define XFA_FXBARCODE_ONED_BC_ONEDCODABARWRITER_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxbarcode/BC_Library.h"
#include "xfa/fxbarcode/oned/BC_OneDimWriter.h"

class CBC_OnedCodaBarWriter : public CBC_OneDimWriter {
 public:
  CBC_OnedCodaBarWriter();
  ~CBC_OnedCodaBarWriter() override;

  // CBC_OneDimWriter
  uint8_t* Encode(const CFX_ByteString& contents,
                  int32_t& outLength,
                  int32_t& e) override;
  uint8_t* Encode(const CFX_ByteString& contents,
                  BCFORMAT format,
                  int32_t& outWidth,
                  int32_t& outHeight,
                  int32_t& e) override;
  uint8_t* Encode(const CFX_ByteString& contents,
                  BCFORMAT format,
                  int32_t& outWidth,
                  int32_t& outHeight,
                  int32_t hints,
                  int32_t& e) override;
  bool CheckContentValidity(const CFX_WideStringC& contents) override;
  CFX_WideString FilterContents(const CFX_WideStringC& contents) override;
  void SetDataLength(int32_t length) override;

  virtual CFX_WideString encodedContents(const CFX_WideStringC& contents);
  virtual bool SetStartChar(FX_CHAR start);
  virtual bool SetEndChar(FX_CHAR end);
  virtual bool SetTextLocation(BC_TEXT_LOC location);
  virtual bool SetWideNarrowRatio(int32_t ratio);
  virtual bool FindChar(FX_WCHAR ch, bool isContent);

 private:
  void RenderResult(const CFX_WideStringC& contents,
                    uint8_t* code,
                    int32_t codeLength,
                    bool isDevice,
                    int32_t& e) override;

  FX_CHAR m_chStart;
  FX_CHAR m_chEnd;
  int32_t m_iWideNarrRatio;
};

#endif  // XFA_FXBARCODE_ONED_BC_ONEDCODABARWRITER_H_
