// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFCHECKBUTTON_H_
#define XFA_FXFA_APP_XFA_FFCHECKBUTTON_H_

#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/xfa_ffpageview.h"

class CXFA_FFCheckButton : public CXFA_FFField {
 public:
  CXFA_FFCheckButton(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFCheckButton() override;

  // CXFA_FFField
  void RenderWidget(CFX_Graphics* pGS,
                    CFX_Matrix* pMatrix,
                    uint32_t dwStatus) override;

  bool LoadWidget() override;
  bool PerformLayout() override;
  bool UpdateFWLData() override;
  void UpdateWidgetProperty() override;
  bool OnLButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  void SetFWLCheckState(XFA_CHECKSTATE eCheckState);

 protected:
  bool CommitData() override;
  bool IsDataChanged() override;

  void CapLeftRightPlacement(CXFA_Margin mgCap);
  void AddUIMargin(int32_t iCapPlacement);
  XFA_CHECKSTATE FWLState2XFAState();
  IFWL_WidgetDelegate* m_pOldDelegate;
  CFX_RectF m_rtCheckBox;
};

#endif  // XFA_FXFA_APP_XFA_FFCHECKBUTTON_H_
