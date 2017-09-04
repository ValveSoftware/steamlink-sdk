// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_combobox.h"

#include <utility>

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/fwl_error.h"
#include "xfa/fwl/core/ifwl_widget.h"

namespace {

IFWL_ComboBox* ToComboBox(IFWL_Widget* widget) {
  return static_cast<IFWL_ComboBox*>(widget);
}

}  // namespace

CFWL_ComboBox::CFWL_ComboBox(const IFWL_App* app)
    : CFWL_Widget(app), m_fMaxListHeight(0) {}

CFWL_ComboBox::~CFWL_ComboBox() {}

void CFWL_ComboBox::Initialize() {
  ASSERT(!m_pIface);

  m_pIface = pdfium::MakeUnique<IFWL_ComboBox>(
      m_pApp, pdfium::MakeUnique<CFWL_WidgetProperties>(this));

  CFWL_Widget::Initialize();
}

int32_t CFWL_ComboBox::AddString(const CFX_WideStringC& wsText) {
  std::unique_ptr<CFWL_ListItem> pItem(new CFWL_ListItem);
  pItem->m_wsText = wsText;
  pItem->m_dwStyles = 0;
  m_ItemArray.push_back(std::move(pItem));
  return m_ItemArray.size() - 1;
}

bool CFWL_ComboBox::RemoveAt(int32_t iIndex) {
  if (iIndex < 0 || static_cast<size_t>(iIndex) >= m_ItemArray.size())
    return false;

  m_ItemArray.erase(m_ItemArray.begin() + iIndex);
  return true;
}

void CFWL_ComboBox::RemoveAll() {
  m_ItemArray.clear();
}

void CFWL_ComboBox::GetTextByIndex(int32_t iIndex,
                                   CFX_WideString& wsText) const {
  CFWL_ListItem* pItem =
      static_cast<CFWL_ListItem*>(GetItem(m_pIface.get(), iIndex));
  if (pItem)
    wsText = pItem->m_wsText;
}

int32_t CFWL_ComboBox::GetCurSel() const {
  return GetWidget() ? ToComboBox(GetWidget())->GetCurSel() : -1;
}

void CFWL_ComboBox::SetCurSel(int32_t iSel) {
  if (GetWidget())
    ToComboBox(GetWidget())->SetCurSel(iSel);
}

void CFWL_ComboBox::SetEditText(const CFX_WideString& wsText) {
  if (GetWidget())
    ToComboBox(GetWidget())->SetEditText(wsText);
}

void CFWL_ComboBox::GetEditText(CFX_WideString& wsText,
                                int32_t nStart,
                                int32_t nCount) const {
  if (GetWidget())
    ToComboBox(GetWidget())->GetEditText(wsText, nStart, nCount);
}

void CFWL_ComboBox::OpenDropDownList(bool bActivate) {
  ToComboBox(GetWidget())->OpenDropDownList(bActivate);
}

bool CFWL_ComboBox::EditCanUndo() {
  return GetWidget() ? ToComboBox(GetWidget())->EditCanUndo() : false;
}

bool CFWL_ComboBox::EditCanRedo() {
  return GetWidget() ? ToComboBox(GetWidget())->EditCanRedo() : false;
}

bool CFWL_ComboBox::EditUndo() {
  return GetWidget() ? ToComboBox(GetWidget())->EditUndo() : false;
}

bool CFWL_ComboBox::EditRedo() {
  return GetWidget() ? ToComboBox(GetWidget())->EditRedo() : false;
}

bool CFWL_ComboBox::EditCanCopy() {
  return GetWidget() ? ToComboBox(GetWidget())->EditCanCopy() : false;
}

bool CFWL_ComboBox::EditCanCut() {
  return GetWidget() ? ToComboBox(GetWidget())->EditCanCut() : false;
}

bool CFWL_ComboBox::EditCanSelectAll() {
  return GetWidget() ? ToComboBox(GetWidget())->EditCanSelectAll() : false;
}

bool CFWL_ComboBox::EditCopy(CFX_WideString& wsCopy) {
  return GetWidget() ? ToComboBox(GetWidget())->EditCopy(wsCopy) : false;
}

bool CFWL_ComboBox::EditCut(CFX_WideString& wsCut) {
  return GetWidget() ? ToComboBox(GetWidget())->EditCut(wsCut) : false;
}

bool CFWL_ComboBox::EditPaste(const CFX_WideString& wsPaste) {
  return GetWidget() ? ToComboBox(GetWidget())->EditPaste(wsPaste) : false;
}

void CFWL_ComboBox::EditSelectAll() {
  if (GetWidget())
    ToComboBox(GetWidget())->EditSelectAll();
}

void CFWL_ComboBox::EditDelete() {
  if (GetWidget())
    ToComboBox(GetWidget())->EditDelete();
}

void CFWL_ComboBox::EditDeSelect() {
  if (GetWidget())
    ToComboBox(GetWidget())->EditDeSelect();
}

void CFWL_ComboBox::GetBBox(CFX_RectF& rect) {
  if (GetWidget())
    ToComboBox(GetWidget())->GetBBox(rect);
}

void CFWL_ComboBox::EditModifyStylesEx(uint32_t dwStylesExAdded,
                                       uint32_t dwStylesExRemoved) {
  if (GetWidget()) {
    ToComboBox(GetWidget())
        ->EditModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
  }
}

void CFWL_ComboBox::GetCaption(IFWL_Widget* pWidget,
                               CFX_WideString& wsCaption) {}

int32_t CFWL_ComboBox::CountItems(const IFWL_Widget* pWidget) const {
  return m_ItemArray.size();
}

CFWL_ListItem* CFWL_ComboBox::GetItem(const IFWL_Widget* pWidget,
                                      int32_t nIndex) const {
  if (nIndex < 0 || static_cast<size_t>(nIndex) >= m_ItemArray.size())
    return nullptr;
  return m_ItemArray[nIndex].get();
}

int32_t CFWL_ComboBox::GetItemIndex(IFWL_Widget* pWidget,
                                    CFWL_ListItem* pItem) {
  auto it = std::find_if(
      m_ItemArray.begin(), m_ItemArray.end(),
      [pItem](const std::unique_ptr<CFWL_ListItem>& candidate) {
        return candidate.get() == static_cast<CFWL_ListItem*>(pItem);
      });
  return it != m_ItemArray.end() ? it - m_ItemArray.begin() : -1;
}

uint32_t CFWL_ComboBox::GetItemStyles(IFWL_Widget* pWidget,
                                      CFWL_ListItem* pItem) {
  return pItem ? static_cast<CFWL_ListItem*>(pItem)->m_dwStyles : 0;
}

void CFWL_ComboBox::GetItemText(IFWL_Widget* pWidget,
                                CFWL_ListItem* pItem,
                                CFX_WideString& wsText) {
  if (pItem)
    wsText = static_cast<CFWL_ListItem*>(pItem)->m_wsText;
}

void CFWL_ComboBox::GetItemRect(IFWL_Widget* pWidget,
                                CFWL_ListItem* pItem,
                                CFX_RectF& rtItem) {
  if (!pItem)
    return;

  CFWL_ListItem* pComboItem = static_cast<CFWL_ListItem*>(pItem);
  rtItem.Set(pComboItem->m_rtItem.left, pComboItem->m_rtItem.top,
             pComboItem->m_rtItem.width, pComboItem->m_rtItem.height);
}

void* CFWL_ComboBox::GetItemData(IFWL_Widget* pWidget, CFWL_ListItem* pItem) {
  return pItem ? static_cast<CFWL_ListItem*>(pItem)->m_pData : nullptr;
}

void CFWL_ComboBox::SetItemStyles(IFWL_Widget* pWidget,
                                  CFWL_ListItem* pItem,
                                  uint32_t dwStyle) {
  if (pItem)
    static_cast<CFWL_ListItem*>(pItem)->m_dwStyles = dwStyle;
}

void CFWL_ComboBox::SetItemRect(IFWL_Widget* pWidget,
                                CFWL_ListItem* pItem,
                                const CFX_RectF& rtItem) {
  if (pItem)
    static_cast<CFWL_ListItem*>(pItem)->m_rtItem = rtItem;
}

CFX_DIBitmap* CFWL_ComboBox::GetItemIcon(IFWL_Widget* pWidget,
                                         CFWL_ListItem* pItem) {
  return pItem ? static_cast<CFWL_ListItem*>(pItem)->m_pDIB : nullptr;
}

void CFWL_ComboBox::GetItemCheckRect(IFWL_Widget* pWidget,
                                     CFWL_ListItem* pItem,
                                     CFX_RectF& rtCheck) {
  rtCheck = static_cast<CFWL_ListItem*>(pItem)->m_rtCheckBox;
}

void CFWL_ComboBox::SetItemCheckRect(IFWL_Widget* pWidget,
                                     CFWL_ListItem* pItem,
                                     const CFX_RectF& rtCheck) {
  static_cast<CFWL_ListItem*>(pItem)->m_rtCheckBox = rtCheck;
}

uint32_t CFWL_ComboBox::GetItemCheckState(IFWL_Widget* pWidget,
                                          CFWL_ListItem* pItem) {
  return static_cast<CFWL_ListItem*>(pItem)->m_dwCheckState;
}

void CFWL_ComboBox::SetItemCheckState(IFWL_Widget* pWidget,
                                      CFWL_ListItem* pItem,
                                      uint32_t dwCheckState) {
  static_cast<CFWL_ListItem*>(pItem)->m_dwCheckState = dwCheckState;
}

FX_FLOAT CFWL_ComboBox::GetListHeight(IFWL_Widget* pWidget) {
  return m_fMaxListHeight;
}
