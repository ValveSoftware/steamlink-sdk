// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_listbox.h"

#include <memory>

#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

IFWL_ListBox* ToListBox(IFWL_Widget* widget) {
  return static_cast<IFWL_ListBox*>(widget);
}

}  // namespace

CFWL_ListBox::CFWL_ListBox(const IFWL_App* app) : CFWL_Widget(app) {}

CFWL_ListBox::~CFWL_ListBox() {}

void CFWL_ListBox::Initialize() {
  ASSERT(!m_pIface);

  m_pIface = pdfium::MakeUnique<IFWL_ListBox>(
      m_pApp, pdfium::MakeUnique<CFWL_WidgetProperties>(this), nullptr);

  CFWL_Widget::Initialize();
}

CFWL_ListItem* CFWL_ListBox::AddString(const CFX_WideStringC& wsAdd,
                                       bool bSelect) {
  std::unique_ptr<CFWL_ListItem> pItem(new CFWL_ListItem);
  pItem->m_dwStates = 0;
  pItem->m_wsText = wsAdd;
  pItem->m_dwStates = bSelect ? FWL_ITEMSTATE_LTB_Selected : 0;
  m_ItemArray.push_back(std::move(pItem));
  return m_ItemArray.back().get();
}

bool CFWL_ListBox::DeleteString(CFWL_ListItem* pItem) {
  int32_t nIndex = GetItemIndex(GetWidget(), pItem);
  if (nIndex < 0 || static_cast<size_t>(nIndex) >= m_ItemArray.size())
    return false;

  int32_t iSel = nIndex + 1;
  if (iSel >= CountItems(m_pIface.get()))
    iSel = nIndex - 1;
  if (iSel >= 0) {
    CFWL_ListItem* pSel =
        static_cast<CFWL_ListItem*>(GetItem(m_pIface.get(), iSel));
    pSel->m_dwStates |= FWL_ITEMSTATE_LTB_Selected;
  }
  m_ItemArray.erase(m_ItemArray.begin() + nIndex);
  return true;
}

void CFWL_ListBox::DeleteAll() {
  m_ItemArray.clear();
}

int32_t CFWL_ListBox::CountSelItems() {
  return GetWidget() ? ToListBox(GetWidget())->CountSelItems() : 0;
}

CFWL_ListItem* CFWL_ListBox::GetSelItem(int32_t nIndexSel) {
  return GetWidget() ? ToListBox(GetWidget())->GetSelItem(nIndexSel) : nullptr;
}

int32_t CFWL_ListBox::GetSelIndex(int32_t nIndex) {
  return GetWidget() ? ToListBox(GetWidget())->GetSelIndex(nIndex) : 0;
}

void CFWL_ListBox::SetSelItem(CFWL_ListItem* pItem, bool bSelect) {
  if (GetWidget())
    ToListBox(GetWidget())->SetSelItem(pItem, bSelect);
}

void CFWL_ListBox::GetItemText(CFWL_ListItem* pItem, CFX_WideString& wsText) {
  if (GetWidget())
    ToListBox(GetWidget())->GetItemText(pItem, wsText);
}

CFWL_ListItem* CFWL_ListBox::GetItem(int32_t nIndex) {
  if (nIndex < 0 || nIndex >= CountItems(nullptr))
    return nullptr;
  return m_ItemArray[nIndex].get();
}

uint32_t CFWL_ListBox::GetItemStates(CFWL_ListItem* pItem) {
  if (!pItem)
    return 0;
  return pItem->m_dwStates | pItem->m_dwCheckState;
}

void CFWL_ListBox::GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) {
  wsCaption = L"";
}

int32_t CFWL_ListBox::CountItems(const IFWL_Widget* pWidget) const {
  return pdfium::CollectionSize<int32_t>(m_ItemArray);
}

CFWL_ListItem* CFWL_ListBox::GetItem(const IFWL_Widget* pWidget,
                                     int32_t nIndex) const {
  if (nIndex < 0 || nIndex >= CountItems(pWidget))
    return nullptr;
  return m_ItemArray[nIndex].get();
}

int32_t CFWL_ListBox::GetItemIndex(IFWL_Widget* pWidget, CFWL_ListItem* pItem) {
  auto it = std::find_if(
      m_ItemArray.begin(), m_ItemArray.end(),
      [pItem](const std::unique_ptr<CFWL_ListItem>& candidate) {
        return candidate.get() == static_cast<CFWL_ListItem*>(pItem);
      });
  return it != m_ItemArray.end() ? it - m_ItemArray.begin() : -1;
}

uint32_t CFWL_ListBox::GetItemStyles(IFWL_Widget* pWidget,
                                     CFWL_ListItem* pItem) {
  return pItem ? static_cast<CFWL_ListItem*>(pItem)->m_dwStates : 0;
}

void CFWL_ListBox::GetItemText(IFWL_Widget* pWidget,
                               CFWL_ListItem* pItem,
                               CFX_WideString& wsText) {
  if (pItem)
    wsText = static_cast<CFWL_ListItem*>(pItem)->m_wsText;
}

void CFWL_ListBox::GetItemRect(IFWL_Widget* pWidget,
                               CFWL_ListItem* pItem,
                               CFX_RectF& rtItem) {
  if (pItem)
    rtItem = static_cast<CFWL_ListItem*>(pItem)->m_rtItem;
}

void* CFWL_ListBox::GetItemData(IFWL_Widget* pWidget, CFWL_ListItem* pItem) {
  return pItem ? static_cast<CFWL_ListItem*>(pItem)->m_pData : nullptr;
}

void CFWL_ListBox::SetItemStyles(IFWL_Widget* pWidget,
                                 CFWL_ListItem* pItem,
                                 uint32_t dwStyle) {
  if (pItem)
    static_cast<CFWL_ListItem*>(pItem)->m_dwStates = dwStyle;
}

void CFWL_ListBox::SetItemRect(IFWL_Widget* pWidget,
                               CFWL_ListItem* pItem,
                               const CFX_RectF& rtItem) {
  if (pItem)
    static_cast<CFWL_ListItem*>(pItem)->m_rtItem = rtItem;
}

CFX_DIBitmap* CFWL_ListBox::GetItemIcon(IFWL_Widget* pWidget,
                                        CFWL_ListItem* pItem) {
  return static_cast<CFWL_ListItem*>(pItem)->m_pDIB;
}

void CFWL_ListBox::GetItemCheckRect(IFWL_Widget* pWidget,
                                    CFWL_ListItem* pItem,
                                    CFX_RectF& rtCheck) {
  rtCheck = static_cast<CFWL_ListItem*>(pItem)->m_rtCheckBox;
}

void CFWL_ListBox::SetItemCheckRect(IFWL_Widget* pWidget,
                                    CFWL_ListItem* pItem,
                                    const CFX_RectF& rtCheck) {
  static_cast<CFWL_ListItem*>(pItem)->m_rtCheckBox = rtCheck;
}

uint32_t CFWL_ListBox::GetItemCheckState(IFWL_Widget* pWidget,
                                         CFWL_ListItem* pItem) {
  return static_cast<CFWL_ListItem*>(pItem)->m_dwCheckState;
}

void CFWL_ListBox::SetItemCheckState(IFWL_Widget* pWidget,
                                     CFWL_ListItem* pItem,
                                     uint32_t dwCheckState) {
  static_cast<CFWL_ListItem*>(pItem)->m_dwCheckState = dwCheckState;
}
