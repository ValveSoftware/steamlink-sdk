// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_LISTBOX_H_
#define XFA_FWL_CORE_CFWL_LISTBOX_H_

#include <memory>
#include <vector>

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/fwl_error.h"
#include "xfa/fwl/core/ifwl_listbox.h"
#include "xfa/fwl/core/ifwl_widget.h"

class CFWL_ListBox : public CFWL_Widget, public IFWL_ListBoxDP {
 public:
  explicit CFWL_ListBox(const IFWL_App* pApp);
  ~CFWL_ListBox() override;

  void Initialize();

  // IFWL_DataProvider:
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override;

  // IFWL_ListBoxDP:
  int32_t CountItems(const IFWL_Widget* pWidget) const override;
  CFWL_ListItem* GetItem(const IFWL_Widget* pWidget,
                         int32_t nIndex) const override;
  int32_t GetItemIndex(IFWL_Widget* pWidget, CFWL_ListItem* pItem) override;
  uint32_t GetItemStyles(IFWL_Widget* pWidget, CFWL_ListItem* pItem) override;
  void GetItemText(IFWL_Widget* pWidget,
                   CFWL_ListItem* pItem,
                   CFX_WideString& wsText) override;
  void GetItemRect(IFWL_Widget* pWidget,
                   CFWL_ListItem* pItem,
                   CFX_RectF& rtItem) override;
  void* GetItemData(IFWL_Widget* pWidget, CFWL_ListItem* pItem) override;
  void SetItemStyles(IFWL_Widget* pWidget,
                     CFWL_ListItem* pItem,
                     uint32_t dwStyle) override;
  void SetItemRect(IFWL_Widget* pWidget,
                   CFWL_ListItem* pItem,
                   const CFX_RectF& rtItem) override;
  CFX_DIBitmap* GetItemIcon(IFWL_Widget* pWidget,
                            CFWL_ListItem* pItem) override;
  void GetItemCheckRect(IFWL_Widget* pWidget,
                        CFWL_ListItem* pItem,
                        CFX_RectF& rtCheck) override;
  void SetItemCheckRect(IFWL_Widget* pWidget,
                        CFWL_ListItem* pItem,
                        const CFX_RectF& rtCheck) override;
  uint32_t GetItemCheckState(IFWL_Widget* pWidget,
                             CFWL_ListItem* pItem) override;
  void SetItemCheckState(IFWL_Widget* pWidget,
                         CFWL_ListItem* pItem,
                         uint32_t dwCheckState) override;

  CFWL_ListItem* AddString(const CFX_WideStringC& wsAdd, bool bSelect = false);
  bool DeleteString(CFWL_ListItem* pItem);
  void DeleteAll();

  int32_t CountSelItems();
  void SetSelItem(CFWL_ListItem* pItem, bool bSelect = true);
  CFWL_ListItem* GetSelItem(int32_t nIndexSel);
  int32_t GetSelIndex(int32_t nIndex);

  CFWL_ListItem* GetItem(int32_t nIndex);
  void GetItemText(CFWL_ListItem* pItem, CFX_WideString& wsText);
  uint32_t GetItemStates(CFWL_ListItem* pItem);

 private:
  std::vector<std::unique_ptr<CFWL_ListItem>> m_ItemArray;
};

#endif  // XFA_FWL_CORE_CFWL_LISTBOX_H_
