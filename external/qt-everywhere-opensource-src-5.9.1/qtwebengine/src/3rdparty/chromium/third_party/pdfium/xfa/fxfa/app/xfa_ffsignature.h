// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFSIGNATURE_H_
#define XFA_FXFA_APP_XFA_FFSIGNATURE_H_

#include "xfa/fxfa/app/xfa_fffield.h"

class CXFA_FFSignature final : public CXFA_FFField {
 public:
  CXFA_FFSignature(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFSignature() override;

  // CXFA_FFField
  void RenderWidget(CFX_Graphics* pGS,
                    CFX_Matrix* pMatrix,
                    uint32_t dwStatus) override;
  bool LoadWidget() override;
  bool OnMouseEnter() override;
  bool OnMouseExit() override;
  bool OnLButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnLButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnLButtonDblClk(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnMouseMove(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnMouseWheel(uint32_t dwFlags,
                    int16_t zDelta,
                    FX_FLOAT fx,
                    FX_FLOAT fy) override;
  bool OnRButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnRButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnRButtonDblClk(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;

  bool OnKeyDown(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnKeyUp(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnChar(uint32_t dwChar, uint32_t dwFlags) override;
  FWL_WidgetHit OnHitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnSetCursor(FX_FLOAT fx, FX_FLOAT fy) override;
};

#endif  // XFA_FXFA_APP_XFA_FFSIGNATURE_H_
