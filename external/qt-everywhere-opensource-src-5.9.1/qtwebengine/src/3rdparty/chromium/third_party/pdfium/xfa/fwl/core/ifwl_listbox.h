// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_LISTBOX_H_
#define XFA_FWL_CORE_IFWL_LISTBOX_H_

#include <memory>

#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_listitem.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_edit.h"
#include "xfa/fwl/core/ifwl_listbox.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_LTB_MultiSelection (1L << 0)
#define FWL_STYLEEXT_LTB_ShowScrollBarAlaways (1L << 2)
#define FWL_STYLEEXT_LTB_MultiColumn (1L << 3)
#define FWL_STYLEEXT_LTB_LeftAlign (0L << 4)
#define FWL_STYLEEXT_LTB_CenterAlign (1L << 4)
#define FWL_STYLEEXT_LTB_RightAlign (2L << 4)
#define FWL_STYLEEXT_LTB_MultiLine (1L << 6)
#define FWL_STYLEEXT_LTB_OwnerDraw (1L << 7)
#define FWL_STYLEEXT_LTB_Icon (1L << 8)
#define FWL_STYLEEXT_LTB_Check (1L << 9)
#define FWL_STYLEEXT_LTB_AlignMask (3L << 4)
#define FWL_STYLEEXT_LTB_ShowScrollBarFocus (1L << 10)
#define FWL_ITEMSTATE_LTB_Selected (1L << 0)
#define FWL_ITEMSTATE_LTB_Focused (1L << 1)
#define FWL_ITEMSTATE_LTB_Checked (1L << 2)

class CFWL_MsgKillFocus;
class CFWL_MsgMouse;
class CFWL_MsgMouseWheel;
class CFX_DIBitmap;

FWL_EVENT_DEF(CFWL_EvtLtbSelChanged,
              CFWL_EventType::SelectChanged,
              CFX_Int32Array iarraySels;)

class IFWL_ListBoxDP : public IFWL_DataProvider {
 public:
  virtual int32_t CountItems(const IFWL_Widget* pWidget) const = 0;
  virtual CFWL_ListItem* GetItem(const IFWL_Widget* pWidget,
                                 int32_t nIndex) const = 0;
  virtual int32_t GetItemIndex(IFWL_Widget* pWidget, CFWL_ListItem* pItem) = 0;
  virtual uint32_t GetItemStyles(IFWL_Widget* pWidget,
                                 CFWL_ListItem* pItem) = 0;
  virtual void GetItemText(IFWL_Widget* pWidget,
                           CFWL_ListItem* pItem,
                           CFX_WideString& wsText) = 0;
  virtual void GetItemRect(IFWL_Widget* pWidget,
                           CFWL_ListItem* pItem,
                           CFX_RectF& rtItem) = 0;
  virtual void* GetItemData(IFWL_Widget* pWidget, CFWL_ListItem* pItem) = 0;
  virtual void SetItemStyles(IFWL_Widget* pWidget,
                             CFWL_ListItem* pItem,
                             uint32_t dwStyle) = 0;
  virtual void SetItemRect(IFWL_Widget* pWidget,
                           CFWL_ListItem* pItem,
                           const CFX_RectF& rtItem) = 0;
  virtual CFX_DIBitmap* GetItemIcon(IFWL_Widget* pWidget,
                                    CFWL_ListItem* pItem) = 0;
  virtual void GetItemCheckRect(IFWL_Widget* pWidget,
                                CFWL_ListItem* pItem,
                                CFX_RectF& rtCheck) = 0;
  virtual void SetItemCheckRect(IFWL_Widget* pWidget,
                                CFWL_ListItem* pItem,
                                const CFX_RectF& rtCheck) = 0;
  virtual uint32_t GetItemCheckState(IFWL_Widget* pWidget,
                                     CFWL_ListItem* pItem) = 0;
  virtual void SetItemCheckState(IFWL_Widget* pWidget,
                                 CFWL_ListItem* pItem,
                                 uint32_t dwCheckState) = 0;
};

class IFWL_ListBox : public IFWL_Widget {
 public:
  IFWL_ListBox(const IFWL_App* app,
               std::unique_ptr<CFWL_WidgetProperties> properties,
               IFWL_Widget* pOuter);
  ~IFWL_ListBox() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  int32_t CountSelItems();
  CFWL_ListItem* GetSelItem(int32_t nIndexSel);
  int32_t GetSelIndex(int32_t nIndex);
  void SetSelItem(CFWL_ListItem* hItem, bool bSelect = true);
  void GetItemText(CFWL_ListItem* hItem, CFX_WideString& wsText);

  FX_FLOAT GetItemHeight() const { return m_fItemHeight; }
  FX_FLOAT CalcItemHeight();

 protected:
  CFWL_ListItem* GetItem(CFWL_ListItem* hItem, uint32_t dwKeyCode);
  void SetSelection(CFWL_ListItem* hStart, CFWL_ListItem* hEnd, bool bSelected);
  CFWL_ListItem* GetItemAtPoint(FX_FLOAT fx, FX_FLOAT fy);
  bool ScrollToVisible(CFWL_ListItem* hItem);
  void InitScrollBar(bool bVert = true);
  bool IsShowScrollBar(bool bVert);
  IFWL_ScrollBar* GetVertScrollBar() const { return m_pVertScrollBar.get(); }
  const CFX_RectF& GetRTClient() const { return m_rtClient; }

 private:
  void SetSelectionDirect(CFWL_ListItem* hItem, bool bSelect);
  bool IsItemSelected(CFWL_ListItem* hItem);
  void ClearSelection();
  void SelectAll();
  CFWL_ListItem* GetFocusedItem();
  void SetFocusItem(CFWL_ListItem* hItem);
  bool GetItemCheckRect(CFWL_ListItem* hItem, CFX_RectF& rtCheck);
  bool SetItemChecked(CFWL_ListItem* hItem, bool bChecked);
  bool GetItemChecked(CFWL_ListItem* hItem);
  void DrawBkground(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    const CFX_Matrix* pMatrix = nullptr);
  void DrawItems(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 const CFX_Matrix* pMatrix = nullptr);
  void DrawItem(CFX_Graphics* pGraphics,
                IFWL_ThemeProvider* pTheme,
                CFWL_ListItem* hItem,
                int32_t Index,
                const CFX_RectF& rtItem,
                const CFX_Matrix* pMatrix = nullptr);
  void DrawStatic(CFX_Graphics* pGraphics, IFWL_ThemeProvider* pTheme);
  CFX_SizeF CalcSize(bool bAutoSize = false);
  void GetItemSize(CFX_SizeF& size,
                   CFWL_ListItem* hItem,
                   FX_FLOAT fWidth,
                   FX_FLOAT fHeight,
                   bool bAutoSize = false);
  FX_FLOAT GetMaxTextWidth();
  FX_FLOAT GetScrollWidth();
  void ProcessSelChanged();
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet = true);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseWheel(CFWL_MsgMouseWheel* pMsg);
  void OnKeyDown(CFWL_MsgKey* pMsg);
  void OnVK(CFWL_ListItem* hItem, bool bShift, bool bCtrl);
  bool OnScroll(IFWL_ScrollBar* pScrollBar, FWL_SCBCODE dwCode, FX_FLOAT fPos);
  void DispatchSelChangedEv();

  CFX_RectF m_rtClient;
  CFX_RectF m_rtStatic;
  CFX_RectF m_rtConent;
  std::unique_ptr<IFWL_ScrollBar> m_pHorzScrollBar;
  std::unique_ptr<IFWL_ScrollBar> m_pVertScrollBar;
  uint32_t m_dwTTOStyles;
  int32_t m_iTTOAligns;
  CFWL_ListItem* m_hAnchor;
  FX_FLOAT m_fItemHeight;
  FX_FLOAT m_fScorllBarWidth;
  bool m_bLButtonDown;
  IFWL_ThemeProvider* m_pScrollBarTP;
};

#endif  // XFA_FWL_CORE_IFWL_LISTBOX_H_
