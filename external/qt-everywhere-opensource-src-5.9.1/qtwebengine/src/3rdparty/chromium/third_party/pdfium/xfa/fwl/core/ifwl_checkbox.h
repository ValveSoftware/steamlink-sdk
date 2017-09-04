// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_CHECKBOX_H_
#define XFA_FWL_CORE_IFWL_CHECKBOX_H_

#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_CKB_Left (0L << 0)
#define FWL_STYLEEXT_CKB_Center (1L << 0)
#define FWL_STYLEEXT_CKB_Right (2L << 0)
#define FWL_STYLEEXT_CKB_Top (0L << 2)
#define FWL_STYLEEXT_CKB_VCenter (1L << 2)
#define FWL_STYLEEXT_CKB_Bottom (2L << 2)
#define FWL_STYLEEXT_CKB_LeftText (1L << 4)
#define FWL_STYLEEXT_CKB_MultiLine (1L << 5)
#define FWL_STYLEEXT_CKB_3State (1L << 6)
#define FWL_STYLEEXT_CKB_RadioButton (1L << 7)
#define FWL_STYLEEXT_CKB_ShapeSolidSquare (0L << 8)
#define FWL_STYLEEXT_CKB_ShapeSunkenSquare (1L << 8)
#define FWL_STYLEEXT_CKB_ShapeSolidCircle (2L << 8)
#define FWL_STYLEEXT_CKB_ShapeSunkenCircle (3L << 8)
#define FWL_STYLEEXT_CKB_SignShapeCheck (0L << 10)
#define FWL_STYLEEXT_CKB_SignShapeCircle (1L << 10)
#define FWL_STYLEEXT_CKB_SignShapeCross (2L << 10)
#define FWL_STYLEEXT_CKB_SignShapeDiamond (3L << 10)
#define FWL_STYLEEXT_CKB_SignShapeSquare (4L << 10)
#define FWL_STYLEEXT_CKB_SignShapeStar (5L << 10)
#define FWL_STYLEEXT_CKB_HLayoutMask (3L << 0)
#define FWL_STYLEEXT_CKB_VLayoutMask (3L << 2)
#define FWL_STYLEEXT_CKB_ShapeMask (3L << 8)
#define FWL_STYLEEXT_CKB_SignShapeMask (7L << 10)
#define FWL_STATE_CKB_Hovered (1 << FWL_WGTSTATE_MAX)
#define FWL_STATE_CKB_Pressed (1 << (FWL_WGTSTATE_MAX + 1))
#define FWL_STATE_CKB_Unchecked (0 << (FWL_WGTSTATE_MAX + 2))
#define FWL_STATE_CKB_Checked (1 << (FWL_WGTSTATE_MAX + 2))
#define FWL_STATE_CKB_Neutral (2 << (FWL_WGTSTATE_MAX + 2))
#define FWL_STATE_CKB_CheckMask (3L << (FWL_WGTSTATE_MAX + 2))

class CFWL_MsgMouse;
class CFWL_WidgetProperties;
class IFWL_Widget;

class IFWL_CheckBoxDP : public IFWL_DataProvider {
 public:
  virtual FX_FLOAT GetBoxSize(IFWL_Widget* pWidget) = 0;
};

class IFWL_CheckBox : public IFWL_Widget {
 public:
  IFWL_CheckBox(const IFWL_App* app,
                std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_CheckBox() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;

  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

 private:
  void SetCheckState(int32_t iCheck);
  void Layout();
  uint32_t GetPartStates() const;
  void UpdateTextOutStyles();
  void NextStates();
  void OnFocusChanged(bool bSet);
  void OnLButtonDown();
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnMouseLeave();
  void OnKeyDown(CFWL_MsgKey* pMsg);

  CFX_RectF m_rtClient;
  CFX_RectF m_rtBox;
  CFX_RectF m_rtCaption;
  CFX_RectF m_rtFocus;
  uint32_t m_dwTTOStyles;
  int32_t m_iTTOAlign;
  bool m_bBtnDown;
};

#endif  // XFA_FWL_CORE_IFWL_CHECKBOX_H_
