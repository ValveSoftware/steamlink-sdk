// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_COMBOLIST_H_
#define XFA_FWL_CORE_IFWL_COMBOLIST_H_

#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_listbox.h"
#include "xfa/fwl/core/ifwl_widget.h"

class IFWL_ComboList : public IFWL_ListBox {
 public:
  IFWL_ComboList(const IFWL_App* app,
                 std::unique_ptr<CFWL_WidgetProperties> properties,
                 IFWL_Widget* pOuter);

  // IFWL_ListBox.
  void OnProcessMessage(CFWL_Message* pMessage) override;

  int32_t MatchItem(const CFX_WideString& wsMatch);

  void ChangeSelected(int32_t iSel);
  int32_t CountItems();

  void SetNotifyOwner(bool notify) { m_bNotifyOwner = notify; }

 private:
  void GetItemRect(int32_t nIndex, CFX_RectF& rtItem);
  void ClientToOuter(FX_FLOAT& fx, FX_FLOAT& fy);
  void OnDropListFocusChanged(CFWL_Message* pMsg, bool bSet);
  void OnDropListMouseMove(CFWL_MsgMouse* pMsg);
  void OnDropListLButtonDown(CFWL_MsgMouse* pMsg);
  void OnDropListLButtonUp(CFWL_MsgMouse* pMsg);
  bool OnDropListKey(CFWL_MsgKey* pKey);
  void OnDropListKeyDown(CFWL_MsgKey* pKey);

  bool m_bNotifyOwner;
};

#endif  // XFA_FWL_CORE_IFWL_COMBOLIST_H_
