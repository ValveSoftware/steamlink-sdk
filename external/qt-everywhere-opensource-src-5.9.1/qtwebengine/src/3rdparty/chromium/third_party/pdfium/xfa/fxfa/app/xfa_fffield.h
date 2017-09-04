// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFFIELD_H_
#define XFA_FXFA_APP_XFA_FFFIELD_H_

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_widgetdelegate.h"
#include "xfa/fxfa/xfa_ffpageview.h"
#include "xfa/fxfa/xfa_ffwidget.h"

#define XFA_MINUI_HEIGHT 4.32f
#define XFA_DEFAULTUI_HEIGHT 2.0f

class CXFA_FFField : public CXFA_FFWidget, public IFWL_WidgetDelegate {
 public:
  CXFA_FFField(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFField() override;

  // CXFA_FFWidget
  bool GetBBox(CFX_RectF& rtBox,
               uint32_t dwStatus,
               bool bDrawFocus = false) override;
  void RenderWidget(CFX_Graphics* pGS,
                    CFX_Matrix* pMatrix,
                    uint32_t dwStatus) override;
  bool IsLoaded() override;
  bool LoadWidget() override;
  void UnloadWidget() override;
  bool PerformLayout() override;
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

  bool OnSetFocus(CXFA_FFWidget* pOldWidget) override;
  bool OnKillFocus(CXFA_FFWidget* pNewWidget) override;
  bool OnKeyDown(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnKeyUp(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnChar(uint32_t dwChar, uint32_t dwFlags) override;
  FWL_WidgetHit OnHitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnSetCursor(FX_FLOAT fx, FX_FLOAT fy) override;

  // IFWL_WidgetDelegate
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  void UpdateFWL();
  uint32_t UpdateUIProperty();

 protected:
  bool PtInActiveRect(FX_FLOAT fx, FX_FLOAT fy) override;

  virtual void SetFWLRect();
  void SetFWLThemeProvider();
  CFWL_Widget* GetNormalWidget() { return m_pNormalWidget; }
  void FWLToClient(FX_FLOAT& fx, FX_FLOAT& fy);
  void LayoutCaption();
  void RenderCaption(CFX_Graphics* pGS, CFX_Matrix* pMatrix = nullptr);

  int32_t CalculateOverride();
  int32_t CalculateWidgetAcc(CXFA_WidgetAcc* pAcc);
  bool ProcessCommittedData();
  virtual bool CommitData();
  virtual bool IsDataChanged();
  void DrawHighlight(CFX_Graphics* pGS,
                     CFX_Matrix* pMatrix,
                     uint32_t dwStatus,
                     bool bEllipse = false);
  void DrawFocus(CFX_Graphics* pGS, CFX_Matrix* pMatrix);
  void TranslateFWLMessage(CFWL_Message* pMessage);
  void CapPlacement();
  void CapTopBottomPlacement(CXFA_Caption caption,
                             const CFX_RectF& rtWidget,
                             int32_t iCapPlacement);
  void CapLeftRightPlacement(CXFA_Caption caption,
                             const CFX_RectF& rtWidget,
                             int32_t iCapPlacement);
  void SetEditScrollOffset();

  CFWL_Widget* m_pNormalWidget;
  CFX_RectF m_rtUI;
  CFX_RectF m_rtCaption;
};

#endif  // XFA_FXFA_APP_XFA_FFFIELD_H_
