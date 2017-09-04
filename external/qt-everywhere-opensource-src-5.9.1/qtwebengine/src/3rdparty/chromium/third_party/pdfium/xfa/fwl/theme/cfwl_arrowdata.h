// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_THEME_CFWL_ARROWDATA_H_
#define XFA_FWL_THEME_CFWL_ARROWDATA_H_

#include <memory>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"

class CFWL_ArrowData {
 public:
  struct CColorData {
    FX_ARGB clrBorder[4];
    FX_ARGB clrStart[4];
    FX_ARGB clrEnd[4];
    FX_ARGB clrSign[4];
  };

  static CFWL_ArrowData* GetInstance();
  static bool HasInstance();
  static void DestroyInstance();
  void SetColorData(uint32_t dwID);

  std::unique_ptr<CColorData> m_pColorData;

 private:
  CFWL_ArrowData();
  ~CFWL_ArrowData();
};

#endif  // XFA_FWL_THEME_CFWL_ARROWDATA_H_
