// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_WIDGET_H_
#define XFA_FWL_CORE_CFWL_WIDGET_H_

#include <memory>

#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/ifwl_widget.h"

class CFWL_Event;
class CFWL_Message;
class CFWL_Widget;
class CFWL_WidgetDelegate;
class CFWL_WidgetMgr;

class CFWL_Widget {
 public:
  explicit CFWL_Widget(const IFWL_App*);
  virtual ~CFWL_Widget();

  IFWL_Widget* GetWidget() { return m_pIface.get(); }
  IFWL_Widget* GetWidget() const { return m_pIface.get(); }

  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false);
  void SetWidgetRect(const CFX_RectF& rect);

  void ModifyStyles(uint32_t dwStylesAdded, uint32_t dwStylesRemoved);
  uint32_t GetStylesEx();
  void ModifyStylesEx(uint32_t dwStylesExAdded, uint32_t dwStylesExRemoved);

  uint32_t GetStates();
  void SetStates(uint32_t dwStates, bool bSet = true);
  void SetLayoutItem(void* pItem);

  void Update();
  void LockUpdate();
  void UnlockUpdate();

  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy);
  void DrawWidget(CFX_Graphics* pGraphics, const CFX_Matrix* pMatrix = nullptr);

  IFWL_WidgetDelegate* GetDelegate() const;
  void SetDelegate(IFWL_WidgetDelegate*);

 protected:
  void Initialize();

  const IFWL_App* m_pApp;
  std::unique_ptr<IFWL_Widget> m_pIface;
};

#endif  // XFA_FWL_CORE_CFWL_WIDGET_H_
