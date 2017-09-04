// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_CBC_CODEBASE_H_
#define XFA_FXBARCODE_CBC_CODEBASE_H_

#include <memory>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"
#include "xfa/fxbarcode/BC_Library.h"

class CBC_Writer;
class CBC_Reader;
class CFX_DIBitmap;
class CFX_RenderDevice;

class CBC_CodeBase {
 public:
  explicit CBC_CodeBase(CBC_Writer* pWriter);
  virtual ~CBC_CodeBase();

  virtual BC_TYPE GetType() = 0;
  virtual bool Encode(const CFX_WideStringC& contents,
                      bool isDevice,
                      int32_t& e) = 0;
  virtual bool RenderDevice(CFX_RenderDevice* device,
                            const CFX_Matrix* matrix,
                            int32_t& e) = 0;
  virtual bool RenderBitmap(CFX_DIBitmap*& pOutBitmap, int32_t& e) = 0;

  bool SetCharEncoding(int32_t encoding);
  bool SetModuleHeight(int32_t moduleHeight);
  bool SetModuleWidth(int32_t moduleWidth);
  bool SetHeight(int32_t height);
  bool SetWidth(int32_t width);
  void SetBackgroundColor(FX_ARGB backgroundColor);
  void SetBarcodeColor(FX_ARGB foregroundColor);

 protected:
  std::unique_ptr<CBC_Writer> m_pBCWriter;
};

#endif  // XFA_FXBARCODE_CBC_CODEBASE_H_
