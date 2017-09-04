// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_COMBOBOXPROXY_H_
#define XFA_FWL_CORE_IFWL_COMBOBOXPROXY_H_

#include "xfa/fwl/core/ifwl_formproxy.h"

class IFWL_ComboBox;

class IFWL_ComboBoxProxy : public IFWL_FormProxy {
 public:
  IFWL_ComboBoxProxy(IFWL_ComboBox* pCombobBox,
                     const IFWL_App* app,
                     std::unique_ptr<CFWL_WidgetProperties> properties,
                     IFWL_Widget* pOuter);
  ~IFWL_ComboBoxProxy() override;

  // IFWL_FormProxy
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  void Reset() { m_bLButtonUpSelf = false; }

 private:
  void OnLButtonDown(CFWL_Message* pMsg);
  void OnLButtonUp(CFWL_Message* pMsg);
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet);

  bool m_bLButtonDown;
  bool m_bLButtonUpSelf;
  IFWL_ComboBox* m_pComboBox;
};

#endif  // XFA_FWL_CORE_IFWL_COMBOBOXPROXY_H_
