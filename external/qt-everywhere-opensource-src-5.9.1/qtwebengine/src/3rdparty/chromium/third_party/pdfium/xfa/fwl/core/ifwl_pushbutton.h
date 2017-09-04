// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_PUSHBUTTON_H_
#define XFA_FWL_CORE_IFWL_PUSHBUTTON_H_

#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_PSB_Left (0L << 0)
#define FWL_STYLEEXT_PSB_Center (1L << 0)
#define FWL_STYLEEXT_PSB_Right (2L << 0)
#define FWL_STYLEEXT_PSB_Top (0L << 2)
#define FWL_STYLEEXT_PSB_VCenter (1L << 2)
#define FWL_STYLEEXT_PSB_Bottom (2L << 2)
#define FWL_STYLEEXT_PSB_TextOnly (0L << 4)
#define FWL_STYLEEXT_PSB_IconOnly (1L << 4)
#define FWL_STYLEEXT_PSB_TextIcon (2L << 4)
#define FWL_STYLEEXT_PSB_HLayoutMask (3L << 0)
#define FWL_STYLEEXT_PSB_VLayoutMask (3L << 2)
#define FWL_STYLEEXT_PSB_ModeMask (3L << 4)
#define FWL_STATE_PSB_Hovered (1 << FWL_WGTSTATE_MAX)
#define FWL_STATE_PSB_Pressed (1 << (FWL_WGTSTATE_MAX + 1))
#define FWL_STATE_PSB_Default (1 << (FWL_WGTSTATE_MAX + 2))

class CFWL_MsgMouse;
class CFX_DIBitmap;
class IFWL_Widget;

class IFWL_PushButtonDP : public IFWL_DataProvider {};

class IFWL_PushButton : public IFWL_Widget {
 public:
  IFWL_PushButton(const IFWL_App* app,
                  std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_PushButton() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void SetStates(uint32_t dwStates, bool bSet = true) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

 private:
  void DrawBkground(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    const CFX_Matrix* pMatrix);
  void DrawText(CFX_Graphics* pGraphics,
                IFWL_ThemeProvider* pTheme,
                const CFX_Matrix* pMatrix);
  uint32_t GetPartStates();
  void UpdateTextOutStyles();
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnMouseLeave(CFWL_MsgMouse* pMsg);
  void OnKeyDown(CFWL_MsgKey* pMsg);

  CFX_RectF m_rtClient;
  CFX_RectF m_rtCaption;
  bool m_bBtnDown;
  uint32_t m_dwTTOStyles;
  int32_t m_iTTOAlign;
};

#endif  // XFA_FWL_CORE_IFWL_PUSHBUTTON_H_
