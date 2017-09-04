// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_CBC_CODE128_H_
#define XFA_FXBARCODE_CBC_CODE128_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"
#include "xfa/fxbarcode/cbc_onecode.h"

class CBC_Code128 : public CBC_OneCode {
 public:
  explicit CBC_Code128(BC_TYPE type);
  ~CBC_Code128() override;

  // CBC_OneCode:
  bool Encode(const CFX_WideStringC& contents,
              bool isDevice,
              int32_t& e) override;
  bool RenderDevice(CFX_RenderDevice* device,
                    const CFX_Matrix* matrix,
                    int32_t& e) override;
  bool RenderBitmap(CFX_DIBitmap*& pOutBitmap, int32_t& e) override;
  BC_TYPE GetType() override;

  bool SetTextLocation(BC_TEXT_LOC loction);

 private:
  CFX_WideString m_renderContents;
};

#endif  // XFA_FXBARCODE_CBC_CODE128_H_
