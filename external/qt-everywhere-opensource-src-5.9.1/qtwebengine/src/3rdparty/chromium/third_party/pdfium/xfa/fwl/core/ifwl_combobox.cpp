// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_combobox.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_evteditchanged.h"
#include "xfa/fwl/core/cfwl_evtpostdropdown.h"
#include "xfa/fwl/core/cfwl_evtpredropdown.h"
#include "xfa/fwl/core/cfwl_evttextchanged.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgkillfocus.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_msgsetfocus.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_formproxy.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

IFWL_ComboBox::IFWL_ComboBox(const IFWL_App* app,
                             std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Widget(app, std::move(properties), nullptr),
      m_pComboBoxProxy(nullptr),
      m_bLButtonDown(false),
      m_iCurSel(-1),
      m_iBtnState(CFWL_PartState_Normal),
      m_fComboFormHandler(0) {
  m_rtClient.Reset();
  m_rtBtn.Reset();
  m_rtHandler.Reset();

  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_InitComboList();
    DisForm_InitComboEdit();
    return;
  }

  auto prop =
      pdfium::MakeUnique<CFWL_WidgetProperties>(m_pProperties->m_pDataProvider);
  prop->m_pThemeProvider = m_pProperties->m_pThemeProvider;
  prop->m_dwStyles |= FWL_WGTSTYLE_Border | FWL_WGTSTYLE_VScroll;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_ListItemIconText)
    prop->m_dwStyleExes |= FWL_STYLEEXT_LTB_Icon;

  m_pListBox.reset(new IFWL_ComboList(m_pOwnerApp, std::move(prop), this));
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_DropDown) && !m_pEdit) {
    m_pEdit.reset(new IFWL_ComboEdit(
        m_pOwnerApp, pdfium::MakeUnique<CFWL_WidgetProperties>(), this));
    m_pEdit->SetOuter(this);
  }
  if (m_pEdit)
    m_pEdit->SetParent(this);

  SetStates(m_pProperties->m_dwStates);
}

IFWL_ComboBox::~IFWL_ComboBox() {}

FWL_Type IFWL_ComboBox::GetClassID() const {
  return FWL_Type::ComboBox;
}

void IFWL_ComboBox::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Reset();
  if (IsDropDownStyle() && m_pEdit) {
    m_pEdit->GetWidgetRect(rect, true);
  } else {
    rect.width = 100;
    rect.height = 16;
  }
  if (!m_pProperties->m_pThemeProvider)
    ResetTheme();

  FX_FLOAT* pFWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pFWidth)
    return;

  rect.Inflate(0, 0, *pFWidth, 0);
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_ComboBox::ModifyStylesEx(uint32_t dwStylesExAdded,
                                   uint32_t dwStylesExRemoved) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
    return;
  }

  bool bAddDropDown = !!(dwStylesExAdded & FWL_STYLEEXT_CMB_DropDown);
  bool bRemoveDropDown = !!(dwStylesExRemoved & FWL_STYLEEXT_CMB_DropDown);
  if (bAddDropDown && !m_pEdit) {
    m_pEdit = pdfium::MakeUnique<IFWL_ComboEdit>(
        m_pOwnerApp, pdfium::MakeUnique<CFWL_WidgetProperties>(), nullptr);
    m_pEdit->SetOuter(this);
    m_pEdit->SetParent(this);
  } else if (bRemoveDropDown && m_pEdit) {
    m_pEdit->SetStates(FWL_WGTSTATE_Invisible, true);
  }
  IFWL_Widget::ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
}

void IFWL_ComboBox::Update() {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_Update();
    return;
  }
  if (IsLocked())
    return;

  ResetTheme();
  if (IsDropDownStyle() && m_pEdit)
    ResetEditAlignment();
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  Layout();
  CFWL_ThemePart part;
  part.m_pWidget = this;
  m_fComboFormHandler =
      *static_cast<FX_FLOAT*>(m_pProperties->m_pThemeProvider->GetCapacity(
          &part, CFWL_WidgetCapacity::ComboFormHandler));
}

FWL_WidgetHit IFWL_ComboBox::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_HitTest(fx, fy);
  return IFWL_Widget::HitTest(fx, fy);
}

void IFWL_ComboBox::DrawWidget(CFX_Graphics* pGraphics,
                               const CFX_Matrix* pMatrix) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_DrawWidget(pGraphics, pMatrix);
    return;
  }

  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);

  if (!IsDropDownStyle()) {
    CFX_RectF rtTextBk(m_rtClient);
    rtTextBk.width -= m_rtBtn.width;

    CFWL_ThemeBackground param;
    param.m_pWidget = this;
    param.m_iPart = CFWL_Part::Background;
    param.m_pGraphics = pGraphics;
    if (pMatrix)
      param.m_matrix.Concat(*pMatrix);
    param.m_rtPart = rtTextBk;

    if (m_iCurSel >= 0) {
      IFWL_ListBoxDP* pData =
          static_cast<IFWL_ListBoxDP*>(m_pListBox->GetDataProvider());
      void* p = pData->GetItemData(m_pListBox.get(),
                                   pData->GetItem(m_pListBox.get(), m_iCurSel));
      if (p)
        param.m_pData = p;
    }

    if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled) {
      param.m_dwStates = CFWL_PartState_Disabled;
    } else if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) &&
               (m_iCurSel >= 0)) {
      param.m_dwStates = CFWL_PartState_Selected;
    } else {
      param.m_dwStates = CFWL_PartState_Normal;
    }
    pTheme->DrawBackground(&param);

    if (m_iCurSel >= 0) {
      if (!m_pListBox)
        return;

      CFX_WideString wsText;
      IFWL_ComboBoxDP* pData =
          static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
      CFWL_ListItem* hItem = pData->GetItem(this, m_iCurSel);
      m_pListBox->GetItemText(hItem, wsText);

      CFWL_ThemeText theme_text;
      theme_text.m_pWidget = this;
      theme_text.m_iPart = CFWL_Part::Caption;
      theme_text.m_dwStates = m_iBtnState;
      theme_text.m_pGraphics = pGraphics;
      theme_text.m_matrix.Concat(*pMatrix);
      theme_text.m_rtPart = rtTextBk;
      theme_text.m_dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
                                  ? CFWL_PartState_Selected
                                  : CFWL_PartState_Normal;
      theme_text.m_wsText = wsText;
      theme_text.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
      theme_text.m_iTTOAlign = FDE_TTOALIGNMENT_CenterLeft;
      pTheme->DrawText(&theme_text);
    }
  }

  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::DropDownButton;
  param.m_dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
                         ? CFWL_PartState_Disabled
                         : m_iBtnState;
  param.m_pGraphics = pGraphics;
  param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtBtn;
  pTheme->DrawBackground(&param);
}

void IFWL_ComboBox::SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) {
  if (!pThemeProvider)
    return;

  m_pProperties->m_pThemeProvider = pThemeProvider;
  if (m_pListBox)
    m_pListBox->SetThemeProvider(pThemeProvider);
  if (m_pEdit)
    m_pEdit->SetThemeProvider(pThemeProvider);
}

void IFWL_ComboBox::SetCurSel(int32_t iSel) {
  int32_t iCount = m_pListBox->CountItems();
  bool bClearSel = iSel < 0 || iSel >= iCount;
  if (IsDropDownStyle() && m_pEdit) {
    if (bClearSel) {
      m_pEdit->SetText(CFX_WideString());
    } else {
      CFX_WideString wsText;
      IFWL_ComboBoxDP* pData =
          static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
      CFWL_ListItem* hItem = pData->GetItem(this, iSel);
      m_pListBox->GetItemText(hItem, wsText);
      m_pEdit->SetText(wsText);
    }
    m_pEdit->Update();
  }
  m_iCurSel = bClearSel ? -1 : iSel;
}

void IFWL_ComboBox::SetStates(uint32_t dwStates, bool bSet) {
  if (IsDropDownStyle() && m_pEdit)
    m_pEdit->SetStates(dwStates, bSet);
  if (m_pListBox)
    m_pListBox->SetStates(dwStates, bSet);
  IFWL_Widget::SetStates(dwStates, bSet);
}

void IFWL_ComboBox::SetEditText(const CFX_WideString& wsText) {
  if (!m_pEdit)
    return;

  m_pEdit->SetText(wsText);
  m_pEdit->Update();
}

void IFWL_ComboBox::GetEditText(CFX_WideString& wsText,
                                int32_t nStart,
                                int32_t nCount) const {
  if (m_pEdit) {
    m_pEdit->GetText(wsText, nStart, nCount);
    return;
  }
  if (!m_pListBox)
    return;

  IFWL_ComboBoxDP* pData =
      static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
  CFWL_ListItem* hItem = pData->GetItem(this, m_iCurSel);
  m_pListBox->GetItemText(hItem, wsText);
}

void IFWL_ComboBox::OpenDropDownList(bool bActivate) {
  ShowDropList(bActivate);
}

void IFWL_ComboBox::GetBBox(CFX_RectF& rect) const {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_GetBBox(rect);
    return;
  }

  rect = m_pProperties->m_rtWidget;
  if (!m_pListBox || !IsDropListVisible())
    return;

  CFX_RectF rtList;
  m_pListBox->GetWidgetRect(rtList);
  rtList.Offset(rect.left, rect.top);
  rect.Union(rtList);
}

void IFWL_ComboBox::EditModifyStylesEx(uint32_t dwStylesExAdded,
                                       uint32_t dwStylesExRemoved) {
  if (m_pEdit)
    m_pEdit->ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
}

FX_FLOAT IFWL_ComboBox::GetListHeight() {
  return static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider)
      ->GetListHeight(this);
}

void IFWL_ComboBox::DrawStretchHandler(CFX_Graphics* pGraphics,
                                       const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pGraphics = pGraphics;
  param.m_iPart = CFWL_Part::StretchHandler;
  param.m_dwStates = CFWL_PartState_Normal;
  param.m_pWidget = this;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtHandler;
  m_pProperties->m_pThemeProvider->DrawBackground(&param);
}

void IFWL_ComboBox::ShowDropList(bool bActivate) {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_ShowDropList(bActivate);
  if (IsDropListVisible() == bActivate)
    return;
  if (!m_pComboBoxProxy)
    InitProxyForm();

  m_pComboBoxProxy->Reset();
  if (!bActivate) {
    m_pComboBoxProxy->EndDoModal();

    m_bLButtonDown = false;
    m_pListBox->SetNotifyOwner(true);
    SetFocus(true);
    return;
  }

  m_pListBox->ChangeSelected(m_iCurSel);
  ResetListItemAlignment();

  uint32_t dwStyleAdd = m_pProperties->m_dwStyleExes &
                        (FWL_STYLEEXT_CMB_Sort | FWL_STYLEEXT_CMB_OwnerDraw);
  m_pListBox->ModifyStylesEx(dwStyleAdd, 0);
  m_pListBox->GetWidgetRect(m_rtList, true);
  FX_FLOAT fHeight = GetListHeight();
  if (fHeight > 0 && m_rtList.height > GetListHeight()) {
    m_rtList.height = GetListHeight();
    m_pListBox->ModifyStyles(FWL_WGTSTYLE_VScroll, 0);
  }

  CFX_RectF rtAnchor;
  rtAnchor.Set(0, 0, m_pProperties->m_rtWidget.width,
               m_pProperties->m_rtWidget.height);

  m_rtList.width = std::max(m_rtList.width, m_rtClient.width);
  m_rtProxy = m_rtList;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_ListDrag)
    m_rtProxy.height += m_fComboFormHandler;

  FX_FLOAT fMinHeight = 0;
  GetPopupPos(fMinHeight, m_rtProxy.height, rtAnchor, m_rtProxy);
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_ListDrag) {
    FX_FLOAT fx = 0;
    FX_FLOAT fy = m_rtClient.top + m_rtClient.height / 2;
    TransformTo(nullptr, fx, fy);

    m_bUpFormHandler = fy > m_rtProxy.top;
    if (m_bUpFormHandler) {
      m_rtHandler.Set(0, 0, m_rtList.width, m_fComboFormHandler);
      m_rtList.top = m_fComboFormHandler;
    } else {
      m_rtHandler.Set(0, m_rtList.height, m_rtList.width, m_fComboFormHandler);
    }
  }
  m_pComboBoxProxy->SetWidgetRect(m_rtProxy);
  m_pComboBoxProxy->Update();
  m_pListBox->SetWidgetRect(m_rtList);
  m_pListBox->Update();

  CFWL_EvtPreDropDown ev;
  ev.m_pSrcTarget = this;
  DispatchEvent(&ev);

  m_fItemHeight = m_pListBox->GetItemHeight();
  m_pListBox->SetFocus(true);
  m_pComboBoxProxy->DoModal();
  m_pListBox->SetFocus(false);
}

void IFWL_ComboBox::MatchEditText() {
  CFX_WideString wsText;
  m_pEdit->GetText(wsText);
  int32_t iMatch = m_pListBox->MatchItem(wsText);
  if (iMatch != m_iCurSel) {
    m_pListBox->ChangeSelected(iMatch);
    if (iMatch >= 0)
      SyncEditText(iMatch);
  } else if (iMatch >= 0) {
    m_pEdit->SetSelected();
  }
  m_iCurSel = iMatch;
}

void IFWL_ComboBox::SyncEditText(int32_t iListItem) {
  CFX_WideString wsText;
  IFWL_ComboBoxDP* pData =
      static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
  CFWL_ListItem* hItem = pData->GetItem(this, iListItem);
  m_pListBox->GetItemText(hItem, wsText);
  m_pEdit->SetText(wsText);
  m_pEdit->Update();
  m_pEdit->SetSelected();
}

void IFWL_ComboBox::Layout() {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_Layout();

  GetClientRect(m_rtClient);
  FX_FLOAT* pFWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pFWidth)
    return;

  FX_FLOAT fBtn = *pFWidth;
  m_rtBtn.Set(m_rtClient.right() - fBtn, m_rtClient.top, fBtn,
              m_rtClient.height);
  if (!IsDropDownStyle() || !m_pEdit)
    return;

  CFX_RectF rtEdit;
  rtEdit.Set(m_rtClient.left, m_rtClient.top, m_rtClient.width - fBtn,
             m_rtClient.height);
  m_pEdit->SetWidgetRect(rtEdit);

  if (m_iCurSel >= 0) {
    CFX_WideString wsText;
    IFWL_ComboBoxDP* pData =
        static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
    CFWL_ListItem* hItem = pData->GetItem(this, m_iCurSel);
    m_pListBox->GetItemText(hItem, wsText);
    m_pEdit->LockUpdate();
    m_pEdit->SetText(wsText);
    m_pEdit->UnlockUpdate();
  }
  m_pEdit->Update();
}

void IFWL_ComboBox::ResetTheme() {
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (!pTheme) {
    pTheme = GetAvailableTheme();
    m_pProperties->m_pThemeProvider = pTheme;
  }
  if (m_pListBox && !m_pListBox->GetThemeProvider())
    m_pListBox->SetThemeProvider(pTheme);
  if (m_pEdit && !m_pEdit->GetThemeProvider())
    m_pEdit->SetThemeProvider(pTheme);
}

void IFWL_ComboBox::ResetEditAlignment() {
  if (!m_pEdit)
    return;

  uint32_t dwAdd = 0;
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_EditHAlignMask) {
    case FWL_STYLEEXT_CMB_EditHCenter: {
      dwAdd |= FWL_STYLEEXT_EDT_HCenter;
      break;
    }
    case FWL_STYLEEXT_CMB_EditHFar: {
      dwAdd |= FWL_STYLEEXT_EDT_HFar;
      break;
    }
    default: { dwAdd |= FWL_STYLEEXT_EDT_HNear; }
  }
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_EditVAlignMask) {
    case FWL_STYLEEXT_CMB_EditVCenter: {
      dwAdd |= FWL_STYLEEXT_EDT_VCenter;
      break;
    }
    case FWL_STYLEEXT_CMB_EditVFar: {
      dwAdd |= FWL_STYLEEXT_EDT_VFar;
      break;
    }
    default: {
      dwAdd |= FWL_STYLEEXT_EDT_VNear;
      break;
    }
  }
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_EditJustified)
    dwAdd |= FWL_STYLEEXT_EDT_Justified;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_EditDistributed)
    dwAdd |= FWL_STYLEEXT_EDT_Distributed;

  m_pEdit->ModifyStylesEx(dwAdd, FWL_STYLEEXT_EDT_HAlignMask |
                                     FWL_STYLEEXT_EDT_HAlignModeMask |
                                     FWL_STYLEEXT_EDT_VAlignMask);
}

void IFWL_ComboBox::ResetListItemAlignment() {
  if (!m_pListBox)
    return;

  uint32_t dwAdd = 0;
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_ListItemAlignMask) {
    case FWL_STYLEEXT_CMB_ListItemCenterAlign: {
      dwAdd |= FWL_STYLEEXT_LTB_CenterAlign;
      break;
    }
    case FWL_STYLEEXT_CMB_ListItemRightAlign: {
      dwAdd |= FWL_STYLEEXT_LTB_RightAlign;
      break;
    }
    default: {
      dwAdd |= FWL_STYLEEXT_LTB_LeftAlign;
      break;
    }
  }
  m_pListBox->ModifyStylesEx(dwAdd, FWL_STYLEEXT_CMB_ListItemAlignMask);
}

void IFWL_ComboBox::ProcessSelChanged(bool bLButtonUp) {
  IFWL_ComboBoxDP* pDatas =
      static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
  m_iCurSel = pDatas->GetItemIndex(this, m_pListBox->GetSelItem(0));
  if (!IsDropDownStyle()) {
    Repaint(&m_rtClient);
    return;
  }

  IFWL_ComboBoxDP* pData =
      static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
  CFWL_ListItem* hItem = pData->GetItem(this, m_iCurSel);
  if (!hItem)
    return;

  CFX_WideString wsText;
  pData->GetItemText(this, hItem, wsText);
  if (m_pEdit) {
    m_pEdit->SetText(wsText);
    m_pEdit->Update();
    m_pEdit->SetSelected();
  }

  CFWL_EvtCmbSelChanged ev;
  ev.bLButtonUp = bLButtonUp;
  ev.m_pSrcTarget = this;
  ev.iArraySels.Add(m_iCurSel);
  DispatchEvent(&ev);
}

void IFWL_ComboBox::InitProxyForm() {
  if (m_pComboBoxProxy)
    return;
  if (!m_pListBox)
    return;

  auto prop = pdfium::MakeUnique<CFWL_WidgetProperties>();
  prop->m_pOwner = this;
  prop->m_dwStyles = FWL_WGTSTYLE_Popup;
  prop->m_dwStates = FWL_WGTSTATE_Invisible;

  // TODO(dsinclair): Does this leak? I don't see a delete, but I'm not sure
  // if the SetParent call is going to transfer ownership.
  m_pComboBoxProxy = new IFWL_ComboBoxProxy(this, m_pOwnerApp, std::move(prop),
                                            m_pListBox.get());
  m_pListBox->SetParent(m_pComboBoxProxy);
}

void IFWL_ComboBox::DisForm_InitComboList() {
  if (m_pListBox)
    return;

  auto prop =
      pdfium::MakeUnique<CFWL_WidgetProperties>(m_pProperties->m_pDataProvider);
  prop->m_pParent = this;
  prop->m_dwStyles = FWL_WGTSTYLE_Border | FWL_WGTSTYLE_VScroll;
  prop->m_dwStates = FWL_WGTSTATE_Invisible;
  prop->m_pThemeProvider = m_pProperties->m_pThemeProvider;

  m_pListBox =
      pdfium::MakeUnique<IFWL_ComboList>(m_pOwnerApp, std::move(prop), this);
}

void IFWL_ComboBox::DisForm_InitComboEdit() {
  if (m_pEdit)
    return;

  auto prop = pdfium::MakeUnique<CFWL_WidgetProperties>();
  prop->m_pParent = this;
  prop->m_pThemeProvider = m_pProperties->m_pThemeProvider;

  m_pEdit =
      pdfium::MakeUnique<IFWL_ComboEdit>(m_pOwnerApp, std::move(prop), this);
  m_pEdit->SetOuter(this);
}

void IFWL_ComboBox::DisForm_ShowDropList(bool bActivate) {
  if (DisForm_IsDropListVisible() == bActivate)
    return;

  if (bActivate) {
    CFWL_EvtPreDropDown preEvent;
    preEvent.m_pSrcTarget = this;
    DispatchEvent(&preEvent);

    IFWL_ComboList* pComboList = m_pListBox.get();
    int32_t iItems = pComboList->CountItems();
    if (iItems < 1)
      return;

    ResetListItemAlignment();
    pComboList->ChangeSelected(m_iCurSel);

    FX_FLOAT fItemHeight = pComboList->CalcItemHeight();
    FX_FLOAT fBorder = GetBorderSize();
    FX_FLOAT fPopupMin = 0.0f;
    if (iItems > 3)
      fPopupMin = fItemHeight * 3 + fBorder * 2;

    FX_FLOAT fPopupMax = fItemHeight * iItems + fBorder * 2;
    CFX_RectF rtList;
    rtList.left = m_rtClient.left;
    rtList.width = m_pProperties->m_rtWidget.width;
    rtList.top = 0;
    rtList.height = 0;
    GetPopupPos(fPopupMin, fPopupMax, m_pProperties->m_rtWidget, rtList);

    m_pListBox->SetWidgetRect(rtList);
    m_pListBox->Update();
  } else {
    SetFocus(true);
  }

  m_pListBox->SetStates(FWL_WGTSTATE_Invisible, !bActivate);
  if (bActivate) {
    CFWL_EvtPostDropDown postEvent;
    postEvent.m_pSrcTarget = this;
    DispatchEvent(&postEvent);
  }

  CFX_RectF rect;
  m_pListBox->GetWidgetRect(rect);
  rect.Inflate(2, 2);
  Repaint(&rect);
}

void IFWL_ComboBox::DisForm_ModifyStylesEx(uint32_t dwStylesExAdded,
                                           uint32_t dwStylesExRemoved) {
  if (!m_pEdit)
    DisForm_InitComboEdit();

  bool bAddDropDown = !!(dwStylesExAdded & FWL_STYLEEXT_CMB_DropDown);
  bool bDelDropDown = !!(dwStylesExRemoved & FWL_STYLEEXT_CMB_DropDown);

  dwStylesExRemoved &= ~FWL_STYLEEXT_CMB_DropDown;
  m_pProperties->m_dwStyleExes |= FWL_STYLEEXT_CMB_DropDown;

  if (bAddDropDown)
    m_pEdit->ModifyStylesEx(0, FWL_STYLEEXT_EDT_ReadOnly);
  else if (bDelDropDown)
    m_pEdit->ModifyStylesEx(FWL_STYLEEXT_EDT_ReadOnly, 0);
  IFWL_Widget::ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
}

void IFWL_ComboBox::DisForm_Update() {
  if (m_iLock)
    return;
  if (m_pEdit)
    ResetEditAlignment();
  ResetTheme();
  Layout();
}

FWL_WidgetHit IFWL_ComboBox::DisForm_HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  CFX_RectF rect;
  rect.Set(0, 0, m_pProperties->m_rtWidget.width - m_rtBtn.width,
           m_pProperties->m_rtWidget.height);
  if (rect.Contains(fx, fy))
    return FWL_WidgetHit::Edit;
  if (m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::Client;
  if (DisForm_IsDropListVisible()) {
    m_pListBox->GetWidgetRect(rect);
    if (rect.Contains(fx, fy))
      return FWL_WidgetHit::Client;
  }
  return FWL_WidgetHit::Unknown;
}

void IFWL_ComboBox::DisForm_DrawWidget(CFX_Graphics* pGraphics,
                                       const CFX_Matrix* pMatrix) {
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  CFX_Matrix mtOrg;
  mtOrg.Set(1, 0, 0, 1, 0, 0);
  if (pMatrix)
    mtOrg = *pMatrix;

  pGraphics->SaveGraphState();
  pGraphics->ConcatMatrix(&mtOrg);
  if (!m_rtBtn.IsEmpty(0.1f)) {
    CFWL_ThemeBackground param;
    param.m_pWidget = this;
    param.m_iPart = CFWL_Part::DropDownButton;
    param.m_dwStates = m_iBtnState;
    param.m_pGraphics = pGraphics;
    param.m_rtPart = m_rtBtn;
    pTheme->DrawBackground(&param);
  }
  pGraphics->RestoreGraphState();

  if (m_pEdit) {
    CFX_RectF rtEdit;
    m_pEdit->GetWidgetRect(rtEdit);
    CFX_Matrix mt;
    mt.Set(1, 0, 0, 1, rtEdit.left, rtEdit.top);
    mt.Concat(mtOrg);
    m_pEdit->DrawWidget(pGraphics, &mt);
  }
  if (m_pListBox && DisForm_IsDropListVisible()) {
    CFX_RectF rtList;
    m_pListBox->GetWidgetRect(rtList);
    CFX_Matrix mt;
    mt.Set(1, 0, 0, 1, rtList.left, rtList.top);
    mt.Concat(mtOrg);
    m_pListBox->DrawWidget(pGraphics, &mt);
  }
}

void IFWL_ComboBox::DisForm_GetBBox(CFX_RectF& rect) const {
  rect = m_pProperties->m_rtWidget;
  if (!m_pListBox || !DisForm_IsDropListVisible())
    return;

  CFX_RectF rtList;
  m_pListBox->GetWidgetRect(rtList);
  rtList.Offset(rect.left, rect.top);
  rect.Union(rtList);
}

void IFWL_ComboBox::DisForm_Layout() {
  GetClientRect(m_rtClient);
  m_rtContent = m_rtClient;
  FX_FLOAT* pFWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pFWidth)
    return;

  FX_FLOAT borderWidth = 1;
  FX_FLOAT fBtn = *pFWidth;
  if (!(GetStylesEx() & FWL_STYLEEXT_CMB_ReadOnly)) {
    m_rtBtn.Set(m_rtClient.right() - fBtn, m_rtClient.top + borderWidth,
                fBtn - borderWidth, m_rtClient.height - 2 * borderWidth);
  }

  CFX_RectF* pUIMargin =
      static_cast<CFX_RectF*>(GetThemeCapacity(CFWL_WidgetCapacity::UIMargin));
  if (pUIMargin) {
    m_rtContent.Deflate(pUIMargin->left, pUIMargin->top, pUIMargin->width,
                        pUIMargin->height);
  }

  if (!IsDropDownStyle() || !m_pEdit)
    return;

  CFX_RectF rtEdit;
  rtEdit.Set(m_rtContent.left, m_rtContent.top, m_rtContent.width - fBtn,
             m_rtContent.height);
  m_pEdit->SetWidgetRect(rtEdit);

  if (m_iCurSel >= 0) {
    CFX_WideString wsText;
    IFWL_ComboBoxDP* pData =
        static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
    CFWL_ListItem* hItem = pData->GetItem(this, m_iCurSel);
    m_pListBox->GetItemText(hItem, wsText);
    m_pEdit->LockUpdate();
    m_pEdit->SetText(wsText);
    m_pEdit->UnlockUpdate();
  }
  m_pEdit->Update();
}

void IFWL_ComboBox::OnProcessMessage(CFWL_Message* pMessage) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_OnProcessMessage(pMessage);
    return;
  }
  if (!pMessage)
    return;

  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::SetFocus:
      OnFocusChanged(pMessage, true);
      break;
    case CFWL_MessageType::KillFocus:
      OnFocusChanged(pMessage, false);
      break;
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::Key:
      OnKey(static_cast<CFWL_MsgKey*>(pMessage));
      break;
    default:
      break;
  }

  IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_ComboBox::OnProcessEvent(CFWL_Event* pEvent) {
  CFWL_EventType dwFlag = pEvent->GetClassID();
  if (dwFlag == CFWL_EventType::Scroll) {
    CFWL_EvtScroll* pScrollEvent = static_cast<CFWL_EvtScroll*>(pEvent);
    CFWL_EvtScroll pScrollEv;
    pScrollEv.m_pSrcTarget = this;
    pScrollEv.m_iScrollCode = pScrollEvent->m_iScrollCode;
    pScrollEv.m_fPos = pScrollEvent->m_fPos;
    DispatchEvent(&pScrollEv);
  } else if (dwFlag == CFWL_EventType::TextChanged) {
    CFWL_EvtEditChanged pTemp;
    pTemp.m_pSrcTarget = this;
    DispatchEvent(&pTemp);
  }
}

void IFWL_ComboBox::OnDrawWidget(CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_ComboBox::OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (bSet) {
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
    if (IsDropDownStyle() && pMsg->m_pSrcTarget != m_pListBox.get()) {
      if (!m_pEdit)
        return;
      m_pEdit->SetSelected();
      return;
    }

    Repaint(&m_rtClient);
    return;
  }

  m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
  if (!IsDropDownStyle() || pMsg->m_pDstTarget == m_pListBox.get()) {
    Repaint(&m_rtClient);
    return;
  }
  if (!m_pEdit)
    return;

  m_pEdit->FlagFocus(false);
  m_pEdit->ClearSelected();
}

void IFWL_ComboBox::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    return;

  CFX_RectF& rtBtn = IsDropDownStyle() ? m_rtBtn : m_rtClient;
  if (!rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  if (IsDropDownStyle() && m_pEdit)
    MatchEditText();

  m_bLButtonDown = true;
  m_iBtnState = CFWL_PartState_Pressed;
  Repaint(&m_rtClient);

  ShowDropList(true);
  m_iBtnState = CFWL_PartState_Normal;
  Repaint(&m_rtClient);
}

void IFWL_ComboBox::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  m_bLButtonDown = false;
  if (m_rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    m_iBtnState = CFWL_PartState_Hovered;
  else
    m_iBtnState = CFWL_PartState_Normal;

  Repaint(&m_rtBtn);
}

void IFWL_ComboBox::OnMouseMove(CFWL_MsgMouse* pMsg) {
  int32_t iOldState = m_iBtnState;
  if (m_rtBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iBtnState =
        m_bLButtonDown ? CFWL_PartState_Pressed : CFWL_PartState_Hovered;
  } else {
    m_iBtnState = CFWL_PartState_Normal;
  }
  if ((iOldState != m_iBtnState) &&
      !((m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled) ==
        FWL_WGTSTATE_Disabled)) {
    Repaint(&m_rtBtn);
  }
}

void IFWL_ComboBox::OnMouseLeave(CFWL_MsgMouse* pMsg) {
  if (!IsDropListVisible() &&
      !((m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled) ==
        FWL_WGTSTATE_Disabled)) {
    m_iBtnState = CFWL_PartState_Normal;
    Repaint(&m_rtBtn);
  }
}

void IFWL_ComboBox::OnKey(CFWL_MsgKey* pMsg) {
  uint32_t dwKeyCode = pMsg->m_dwKeyCode;
  if (dwKeyCode == FWL_VKEY_Tab) {
    DispatchKeyEvent(pMsg);
    return;
  }
  if (pMsg->m_pDstTarget == this)
    DoSubCtrlKey(pMsg);
}

void IFWL_ComboBox::DoSubCtrlKey(CFWL_MsgKey* pMsg) {
  uint32_t dwKeyCode = pMsg->m_dwKeyCode;
  const bool bUp = dwKeyCode == FWL_VKEY_Up;
  const bool bDown = dwKeyCode == FWL_VKEY_Down;
  if (bUp || bDown) {
    int32_t iCount = m_pListBox->CountItems();
    if (iCount < 1)
      return;

    bool bMatchEqual = false;
    int32_t iCurSel = m_iCurSel;
    bool bDropDown = IsDropDownStyle();
    if (bDropDown && m_pEdit) {
      CFX_WideString wsText;
      m_pEdit->GetText(wsText);
      iCurSel = m_pListBox->MatchItem(wsText);
      if (iCurSel >= 0) {
        CFX_WideString wsTemp;
        IFWL_ComboBoxDP* pData =
            static_cast<IFWL_ComboBoxDP*>(m_pProperties->m_pDataProvider);
        CFWL_ListItem* hItem = pData->GetItem(this, iCurSel);
        m_pListBox->GetItemText(hItem, wsTemp);
        bMatchEqual = wsText == wsTemp;
      }
    }
    if (iCurSel < 0) {
      iCurSel = 0;
    } else if (!bDropDown || bMatchEqual) {
      if ((bUp && iCurSel == 0) || (bDown && iCurSel == iCount - 1))
        return;
      if (bUp)
        iCurSel--;
      else
        iCurSel++;
    }
    m_iCurSel = iCurSel;
    if (bDropDown && m_pEdit)
      SyncEditText(m_iCurSel);
    else
      Repaint(&m_rtClient);
    return;
  }

  if (IsDropDownStyle())
    m_pEdit->GetDelegate()->OnProcessMessage(pMsg);
}

void IFWL_ComboBox::DisForm_OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  bool backDefault = true;
  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::SetFocus: {
      backDefault = false;
      DisForm_OnFocusChanged(pMessage, true);
      break;
    }
    case CFWL_MessageType::KillFocus: {
      backDefault = false;
      DisForm_OnFocusChanged(pMessage, false);
      break;
    }
    case CFWL_MessageType::Mouse: {
      backDefault = false;
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          DisForm_OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::Key: {
      backDefault = false;
      CFWL_MsgKey* pKey = static_cast<CFWL_MsgKey*>(pMessage);
      if (pKey->m_dwCmd == FWL_KeyCommand::KeyUp)
        break;
      if (DisForm_IsDropListVisible() &&
          pKey->m_dwCmd == FWL_KeyCommand::KeyDown) {
        bool bListKey = pKey->m_dwKeyCode == FWL_VKEY_Up ||
                        pKey->m_dwKeyCode == FWL_VKEY_Down ||
                        pKey->m_dwKeyCode == FWL_VKEY_Return ||
                        pKey->m_dwKeyCode == FWL_VKEY_Escape;
        if (bListKey) {
          m_pListBox->GetDelegate()->OnProcessMessage(pMessage);
          break;
        }
      }
      DisForm_OnKey(pKey);
      break;
    }
    default:
      break;
  }
  if (backDefault)
    IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_ComboBox::DisForm_OnLButtonDown(CFWL_MsgMouse* pMsg) {
  bool bDropDown = DisForm_IsDropListVisible();
  CFX_RectF& rtBtn = bDropDown ? m_rtBtn : m_rtClient;
  if (!rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  if (DisForm_IsDropListVisible()) {
    DisForm_ShowDropList(false);
    return;
  }
  if (m_pEdit)
    MatchEditText();
  DisForm_ShowDropList(true);
}

void IFWL_ComboBox::DisForm_OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (bSet) {
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
    if ((m_pEdit->GetStates() & FWL_WGTSTATE_Focused) == 0) {
      CFWL_MsgSetFocus msg;
      msg.m_pDstTarget = m_pEdit.get();
      msg.m_pSrcTarget = nullptr;
      m_pEdit->GetDelegate()->OnProcessMessage(&msg);
    }
  } else {
    m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
    DisForm_ShowDropList(false);
    CFWL_MsgKillFocus msg;
    msg.m_pDstTarget = nullptr;
    msg.m_pSrcTarget = m_pEdit.get();
    m_pEdit->GetDelegate()->OnProcessMessage(&msg);
  }
}

void IFWL_ComboBox::DisForm_OnKey(CFWL_MsgKey* pMsg) {
  uint32_t dwKeyCode = pMsg->m_dwKeyCode;
  const bool bUp = dwKeyCode == FWL_VKEY_Up;
  const bool bDown = dwKeyCode == FWL_VKEY_Down;
  if (bUp || bDown) {
    IFWL_ComboList* pComboList = m_pListBox.get();
    int32_t iCount = pComboList->CountItems();
    if (iCount < 1)
      return;

    bool bMatchEqual = false;
    int32_t iCurSel = m_iCurSel;
    if (m_pEdit) {
      CFX_WideString wsText;
      m_pEdit->GetText(wsText);
      iCurSel = pComboList->MatchItem(wsText);
      if (iCurSel >= 0) {
        CFX_WideString wsTemp;
        CFWL_ListItem* item = m_pListBox->GetSelItem(iCurSel);
        m_pListBox->GetItemText(item, wsTemp);
        bMatchEqual = wsText == wsTemp;
      }
    }
    if (iCurSel < 0) {
      iCurSel = 0;
    } else if (bMatchEqual) {
      if ((bUp && iCurSel == 0) || (bDown && iCurSel == iCount - 1))
        return;
      if (bUp)
        iCurSel--;
      else
        iCurSel++;
    }
    m_iCurSel = iCurSel;
    SyncEditText(m_iCurSel);
    return;
  }
  if (m_pEdit)
    m_pEdit->GetDelegate()->OnProcessMessage(pMsg);
}
