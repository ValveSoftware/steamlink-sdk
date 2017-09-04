// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_CBC_ONECODE_H_
#define XFA_FXBARCODE_CBC_ONECODE_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxbarcode/cbc_codebase.h"

class CFX_DIBitmap;
class CFX_Font;
class CFX_RenderDevice;

class CBC_OneCode : public CBC_CodeBase {
 public:
  explicit CBC_OneCode(CBC_Writer* pWriter);
  ~CBC_OneCode() override;

  virtual bool CheckContentValidity(const CFX_WideStringC& contents);
  virtual CFX_WideString FilterContents(const CFX_WideStringC& contents);

  virtual void SetPrintChecksum(bool checksum);
  virtual void SetDataLength(int32_t length);
  virtual void SetCalChecksum(bool calc);
  virtual bool SetFont(CFX_Font* cFont);
  virtual void SetFontSize(FX_FLOAT size);
  virtual void SetFontStyle(int32_t style);
  virtual void SetFontColor(FX_ARGB color);
};

#endif  // XFA_FXBARCODE_CBC_ONECODE_H_
