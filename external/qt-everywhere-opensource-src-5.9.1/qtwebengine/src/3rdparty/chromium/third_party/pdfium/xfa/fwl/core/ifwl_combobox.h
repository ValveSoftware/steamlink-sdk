// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_COMBOBOX_H_
#define XFA_FWL_CORE_IFWL_COMBOBOX_H_

#include "xfa/fwl/core/ifwl_comboboxproxy.h"
#include "xfa/fwl/core/ifwl_comboedit.h"
#include "xfa/fwl/core/ifwl_combolist.h"
#include "xfa/fwl/core/ifwl_form.h"
#include "xfa/fwl/core/ifwl_listbox.h"
#include "xfa/fxgraphics/cfx_graphics.h"

class CFWL_WidgetProperties;
class IFWL_ComboBox;
class IFWL_ComboBoxProxy;
class IFWL_FormProxy;
class IFWL_ListBox;
class IFWL_Widget;

#define FWL_STYLEEXT_CMB_DropList (0L << 0)
#define FWL_STYLEEXT_CMB_DropDown (1L << 0)
#define FWL_STYLEEXT_CMB_Sort (1L << 1)
#define FWL_STYLEEXT_CMB_ListDrag (1L << 2)
#define FWL_STYLEEXT_CMB_OwnerDraw (1L << 3)
#define FWL_STYLEEXT_CMB_EditHNear (0L << 4)
#define FWL_STYLEEXT_CMB_EditHCenter (1L << 4)
#define FWL_STYLEEXT_CMB_EditHFar (2L << 4)
#define FWL_STYLEEXT_CMB_EditVNear (0L << 6)
#define FWL_STYLEEXT_CMB_EditVCenter (1L << 6)
#define FWL_STYLEEXT_CMB_EditVFar (2L << 6)
#define FWL_STYLEEXT_CMB_EditJustified (1L << 8)
#define FWL_STYLEEXT_CMB_EditDistributed (2L << 8)
#define FWL_STYLEEXT_CMB_EditHAlignMask (3L << 4)
#define FWL_STYLEEXT_CMB_EditVAlignMask (3L << 6)
#define FWL_STYLEEXT_CMB_EditHAlignModeMask (3L << 8)
#define FWL_STYLEEXT_CMB_ListItemLeftAlign (0L << 10)
#define FWL_STYLEEXT_CMB_ListItemCenterAlign (1L << 10)
#define FWL_STYLEEXT_CMB_ListItemRightAlign (2L << 10)
#define FWL_STYLEEXT_CMB_ListItemAlignMask (3L << 10)
#define FWL_STYLEEXT_CMB_ListItemText (0L << 12)
#define FWL_STYLEEXT_CMB_ListItemIconText (1L << 12)
#define FWL_STYLEEXT_CMB_ReadOnly (1L << 13)

FWL_EVENT_DEF(CFWL_EvtCmbSelChanged,
              CFWL_EventType::SelectChanged,
              CFX_Int32Array iArraySels;
              bool bLButtonUp;)

class IFWL_ComboBoxDP : public IFWL_ListBoxDP {
 public:
  virtual FX_FLOAT GetListHeight(IFWL_Widget* pWidget) = 0;
};

class IFWL_ComboBox : public IFWL_Widget {
 public:
  explicit IFWL_ComboBox(const IFWL_App* app,
                         std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_ComboBox() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void ModifyStylesEx(uint32_t dwStylesExAdded,
                      uint32_t dwStylesExRemoved) override;
  void SetStates(uint32_t dwStates, bool bSet = true) override;
  void Update() override;
  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  int32_t GetCurSel() const { return m_iCurSel; }
  void SetCurSel(int32_t iSel);

  void SetEditText(const CFX_WideString& wsText);
  void GetEditText(CFX_WideString& wsText,
                   int32_t nStart = 0,
                   int32_t nCount = -1) const;

  void OpenDropDownList(bool bActivate);

  bool EditCanUndo() const { return m_pEdit->CanUndo(); }
  bool EditCanRedo() const { return m_pEdit->CanRedo(); }
  bool EditUndo() { return m_pEdit->Undo(); }
  bool EditRedo() { return m_pEdit->Redo(); }
  bool EditCanCopy() const { return m_pEdit->CountSelRanges() > 0; }
  bool EditCanCut() const {
    if (m_pEdit->GetStylesEx() & FWL_STYLEEXT_EDT_ReadOnly)
      return false;
    return EditCanCopy();
  }
  bool EditCanSelectAll() const { return m_pEdit->GetTextLength() > 0; }
  bool EditCopy(CFX_WideString& wsCopy) const { return m_pEdit->Copy(wsCopy); }
  bool EditCut(CFX_WideString& wsCut) { return m_pEdit->Cut(wsCut); }
  bool EditPaste(const CFX_WideString& wsPaste) {
    return m_pEdit->Paste(wsPaste);
  }
  void EditSelectAll() { m_pEdit->AddSelRange(0); }
  void EditDelete() { m_pEdit->ClearText(); }
  void EditDeSelect() { m_pEdit->ClearSelections(); }

  void GetBBox(CFX_RectF& rect) const;
  void EditModifyStylesEx(uint32_t dwStylesExAdded, uint32_t dwStylesExRemoved);

  void DrawStretchHandler(CFX_Graphics* pGraphics, const CFX_Matrix* pMatrix);
  bool IsDropListVisible() const {
    return m_pComboBoxProxy &&
           !(m_pComboBoxProxy->GetStates() & FWL_WGTSTATE_Invisible);
  }
  void ShowDropList(bool bActivate);

  IFWL_ComboEdit* GetComboEdit() const { return m_pEdit.get(); }

  void ProcessSelChanged(bool bLButtonUp);
  int32_t GetCurrentSelection() const { return m_iCurSel; }

 private:
  FX_FLOAT GetListHeight();
  bool IsDropDownStyle() const {
    return !!(m_pProperties->m_dwStyleExes & FWL_STYLEEXT_CMB_DropDown);
  }
  void MatchEditText();
  void SyncEditText(int32_t iListItem);
  void Layout();
  void ResetTheme();
  void ResetEditAlignment();
  void ResetListItemAlignment();
  void InitProxyForm();
  void DisForm_InitComboList();
  void DisForm_InitComboEdit();
  void DisForm_ShowDropList(bool bActivate);
  bool DisForm_IsDropListVisible() const {
    return !(m_pListBox->GetStates() & FWL_WGTSTATE_Invisible);
  }
  void DisForm_ModifyStylesEx(uint32_t dwStylesExAdded,
                              uint32_t dwStylesExRemoved);
  void DisForm_Update();
  FWL_WidgetHit DisForm_HitTest(FX_FLOAT fx, FX_FLOAT fy);
  void DisForm_DrawWidget(CFX_Graphics* pGraphics,
                          const CFX_Matrix* pMatrix = nullptr);
  void DisForm_GetBBox(CFX_RectF& rect) const;
  void DisForm_Layout();
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet = true);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnMouseLeave(CFWL_MsgMouse* pMsg);
  void OnKey(CFWL_MsgKey* pMsg);
  void DoSubCtrlKey(CFWL_MsgKey* pMsg);
  void DisForm_OnProcessMessage(CFWL_Message* pMessage);
  void DisForm_OnLButtonDown(CFWL_MsgMouse* pMsg);
  void DisForm_OnFocusChanged(CFWL_Message* pMsg, bool bSet = true);
  void DisForm_OnKey(CFWL_MsgKey* pMsg);

  CFX_RectF m_rtClient;
  CFX_RectF m_rtContent;
  CFX_RectF m_rtBtn;
  CFX_RectF m_rtList;
  CFX_RectF m_rtProxy;
  CFX_RectF m_rtHandler;
  std::unique_ptr<IFWL_ComboEdit> m_pEdit;
  std::unique_ptr<IFWL_ComboList> m_pListBox;
  IFWL_ComboBoxProxy* m_pComboBoxProxy;  // Can this be a unique_ptr?
  bool m_bLButtonDown;
  bool m_bUpFormHandler;
  int32_t m_iCurSel;
  int32_t m_iBtnState;
  FX_FLOAT m_fComboFormHandler;
  FX_FLOAT m_fItemHeight;
};

#endif  // XFA_FWL_CORE_IFWL_COMBOBOX_H_
