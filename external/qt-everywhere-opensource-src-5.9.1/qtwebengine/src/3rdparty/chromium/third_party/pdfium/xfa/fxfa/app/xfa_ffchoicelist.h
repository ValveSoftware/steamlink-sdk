// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFCHOICELIST_H_
#define XFA_FXFA_APP_XFA_FFCHOICELIST_H_

#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/xfa_ffpageview.h"

class CXFA_FFListBox : public CXFA_FFField {
 public:
  CXFA_FFListBox(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFListBox() override;

  // CXFA_FFField
  bool LoadWidget() override;
  bool OnKillFocus(CXFA_FFWidget* pNewWidget) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  void OnSelectChanged(IFWL_Widget* pWidget, const CFX_Int32Array& arrSels);
  void SetItemState(int32_t nIndex, bool bSelected);
  void InsertItem(const CFX_WideStringC& wsLabel, int32_t nIndex = -1);
  void DeleteItem(int32_t nIndex);

 protected:
  bool CommitData() override;
  bool UpdateFWLData() override;
  bool IsDataChanged() override;

  uint32_t GetAlignment();

  IFWL_WidgetDelegate* m_pOldDelegate;
};

class CXFA_FFComboBox : public CXFA_FFField {
 public:
  CXFA_FFComboBox(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFComboBox() override;

  // CXFA_FFField
  bool GetBBox(CFX_RectF& rtBox,
               uint32_t dwStatus,
               bool bDrawFocus = false) override;
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;
  bool OnRButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnKillFocus(CXFA_FFWidget* pNewWidget) override;
  bool CanUndo() override;
  bool CanRedo() override;
  bool Undo() override;
  bool Redo() override;

  bool CanCopy() override;
  bool CanCut() override;
  bool CanPaste() override;
  bool CanSelectAll() override;
  bool Copy(CFX_WideString& wsCopy) override;
  bool Cut(CFX_WideString& wsCut) override;
  bool Paste(const CFX_WideString& wsPaste) override;
  void SelectAll() override;
  void Delete() override;
  void DeSelect() override;

  // IFWL_WidgetDelegate
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  virtual void OpenDropDownList();

  void OnTextChanged(IFWL_Widget* pWidget, const CFX_WideString& wsChanged);
  void OnSelectChanged(IFWL_Widget* pWidget,
                       const CFX_Int32Array& arrSels,
                       bool bLButtonUp);
  void OnPreOpen(IFWL_Widget* pWidget);
  void OnPostOpen(IFWL_Widget* pWidget);
  void SetItemState(int32_t nIndex, bool bSelected);
  void InsertItem(const CFX_WideStringC& wsLabel, int32_t nIndex = -1);
  void DeleteItem(int32_t nIndex);

 protected:
  // CXFA_FFField
  bool PtInActiveRect(FX_FLOAT fx, FX_FLOAT fy) override;
  bool CommitData() override;
  bool UpdateFWLData() override;
  bool IsDataChanged() override;

  uint32_t GetAlignment();
  void FWLEventSelChange(CXFA_EventParam* pParam);

  CFX_WideString m_wsNewValue;
  IFWL_WidgetDelegate* m_pOldDelegate;
};

#endif  // XFA_FXFA_APP_XFA_FFCHOICELIST_H_
