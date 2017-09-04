// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_EDIT_H_
#define XFA_FWL_CORE_IFWL_EDIT_H_

#include <deque>
#include <vector>

#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fde/ifde_txtedtdorecord.h"
#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_scrollbar.h"
#include "xfa/fxgraphics/cfx_path.h"

#define FWL_STYLEEXT_EDT_ReadOnly (1L << 0)
#define FWL_STYLEEXT_EDT_MultiLine (1L << 1)
#define FWL_STYLEEXT_EDT_WantReturn (1L << 2)
#define FWL_STYLEEXT_EDT_NoHideSel (1L << 3)
#define FWL_STYLEEXT_EDT_AutoHScroll (1L << 4)
#define FWL_STYLEEXT_EDT_AutoVScroll (1L << 5)
#define FWL_STYLEEXT_EDT_NoRedoUndo (1L << 6)
#define FWL_STYLEEXT_EDT_Validate (1L << 7)
#define FWL_STYLEEXT_EDT_Password (1L << 8)
#define FWL_STYLEEXT_EDT_Number (1L << 9)
#define FWL_STYLEEXT_EDT_VerticalLayout (1L << 12)
#define FWL_STYLEEXT_EDT_VerticalChars (1L << 13)
#define FWL_STYLEEXT_EDT_ReverseLine (1L << 14)
#define FWL_STYLEEXT_EDT_ArabicShapes (1L << 15)
#define FWL_STYLEEXT_EDT_ExpandTab (1L << 16)
#define FWL_STYLEEXT_EDT_CombText (1L << 17)
#define FWL_STYLEEXT_EDT_HNear (0L << 18)
#define FWL_STYLEEXT_EDT_HCenter (1L << 18)
#define FWL_STYLEEXT_EDT_HFar (2L << 18)
#define FWL_STYLEEXT_EDT_VNear (0L << 20)
#define FWL_STYLEEXT_EDT_VCenter (1L << 20)
#define FWL_STYLEEXT_EDT_VFar (2L << 20)
#define FWL_STYLEEXT_EDT_Justified (1L << 22)
#define FWL_STYLEEXT_EDT_Distributed (2L << 22)
#define FWL_STYLEEXT_EDT_HAlignMask (3L << 18)
#define FWL_STYLEEXT_EDT_VAlignMask (3L << 20)
#define FWL_STYLEEXT_EDT_HAlignModeMask (3L << 22)
#define FWL_STYLEEXT_EDT_InnerCaret (1L << 24)
#define FWL_STYLEEXT_EDT_ShowScrollbarFocus (1L << 25)
#define FWL_STYLEEXT_EDT_OuterScrollbar (1L << 26)
#define FWL_STYLEEXT_EDT_LastLineHeight (1L << 27)

class IFDE_TxtEdtDoRecord;
class IFWL_Edit;
class CFWL_MsgMouse;
class CFWL_WidgetProperties;
class IFWL_Caret;

class IFWL_Edit : public IFWL_Widget {
 public:
  IFWL_Edit(const IFWL_App* app,
            std::unique_ptr<CFWL_WidgetProperties> properties,
            IFWL_Widget* pOuter);
  ~IFWL_Edit() override;

  // IFWL_Widget:
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  void SetStates(uint32_t dwStates, bool bSet = true) override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  virtual void SetText(const CFX_WideString& wsText);

  int32_t GetTextLength() const;
  void GetText(CFX_WideString& wsText,
               int32_t nStart = 0,
               int32_t nCount = -1) const;
  void ClearText();

  void AddSelRange(int32_t nStart, int32_t nCount = -1);
  int32_t CountSelRanges() const;
  int32_t GetSelRange(int32_t nIndex, int32_t& nStart) const;
  void ClearSelections();
  int32_t GetLimit() const;
  void SetLimit(int32_t nLimit);
  void SetAliasChar(FX_WCHAR wAlias);
  bool Copy(CFX_WideString& wsCopy);
  bool Cut(CFX_WideString& wsCut);
  bool Paste(const CFX_WideString& wsPaste);
  bool Redo(const IFDE_TxtEdtDoRecord* pRecord);
  bool Undo(const IFDE_TxtEdtDoRecord* pRecord);
  bool Undo();
  bool Redo();
  bool CanUndo();
  bool CanRedo();

  void SetOuter(IFWL_Widget* pOuter);

  void On_CaretChanged(CFDE_TxtEdtEngine* pEdit,
                       int32_t nPage,
                       bool bVisible = true);
  void On_TextChanged(CFDE_TxtEdtEngine* pEdit,
                      FDE_TXTEDT_TEXTCHANGE_INFO& ChangeInfo);
  void On_SelChanged(CFDE_TxtEdtEngine* pEdit);
  bool On_PageLoad(CFDE_TxtEdtEngine* pEdit,
                   int32_t nPageIndex,
                   int32_t nPurpose);
  bool On_PageUnload(CFDE_TxtEdtEngine* pEdit,
                     int32_t nPageIndex,
                     int32_t nPurpose);
  void On_AddDoRecord(CFDE_TxtEdtEngine* pEdit, IFDE_TxtEdtDoRecord* pRecord);
  bool On_Validate(CFDE_TxtEdtEngine* pEdit, CFX_WideString& wsText);
  void SetScrollOffset(FX_FLOAT fScrollOffset);

 protected:
  void ShowCaret(bool bVisible, CFX_RectF* pRect = nullptr);
  const CFX_RectF& GetRTClient() const { return m_rtClient; }
  CFDE_TxtEdtEngine* GetTxtEdtEngine() { return &m_EdtEngine; }

 private:
  void DrawTextBk(CFX_Graphics* pGraphics,
                  IFWL_ThemeProvider* pTheme,
                  const CFX_Matrix* pMatrix = nullptr);
  void DrawContent(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix = nullptr);
  void UpdateEditEngine();
  void UpdateEditParams();
  void UpdateEditLayout();
  bool UpdateOffset();
  bool UpdateOffset(IFWL_ScrollBar* pScrollBar, FX_FLOAT fPosChanged);
  void UpdateVAlignment();
  void UpdateCaret();
  IFWL_ScrollBar* UpdateScroll();
  void Layout();
  void LayoutScrollBar();
  void DeviceToEngine(CFX_PointF& pt);
  void InitScrollBar(bool bVert = true);
  void InitEngine();
  bool ValidateNumberChar(FX_WCHAR cNum);
  void InitCaret();
  void ClearRecord();
  bool IsShowScrollBar(bool bVert);
  bool IsContentHeightOverflow();
  int32_t AddDoRecord(IFDE_TxtEdtDoRecord* pRecord);
  void ProcessInsertError(int32_t iError);

  void DrawSpellCheck(CFX_Graphics* pGraphics,
                      const CFX_Matrix* pMatrix = nullptr);
  void AddSpellCheckObj(CFX_Path& PathData,
                        int32_t nStart,
                        int32_t nCount,
                        FX_FLOAT fOffSetX,
                        FX_FLOAT fOffSetY);
  void DoButtonDown(CFWL_MsgMouse* pMsg);
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnButtonDblClk(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnKeyDown(CFWL_MsgKey* pMsg);
  void OnChar(CFWL_MsgKey* pMsg);
  bool OnScroll(IFWL_ScrollBar* pScrollBar, FWL_SCBCODE dwCode, FX_FLOAT fPos);

  CFX_RectF m_rtClient;
  CFX_RectF m_rtEngine;
  CFX_RectF m_rtStatic;
  FX_FLOAT m_fVAlignOffset;
  FX_FLOAT m_fScrollOffsetX;
  FX_FLOAT m_fScrollOffsetY;
  CFDE_TxtEdtEngine m_EdtEngine;
  bool m_bLButtonDown;
  int32_t m_nSelStart;
  int32_t m_nLimit;
  FX_FLOAT m_fFontSize;
  bool m_bSetRange;
  int32_t m_iMax;
  std::unique_ptr<IFWL_ScrollBar> m_pVertScrollBar;
  std::unique_ptr<IFWL_ScrollBar> m_pHorzScrollBar;
  std::unique_ptr<IFWL_Caret> m_pCaret;
  CFX_WideString m_wsCache;
  CFX_WideString m_wsFont;
  std::deque<std::unique_ptr<IFDE_TxtEdtDoRecord>> m_DoRecords;
  int32_t m_iCurRecord;
  int32_t m_iMaxRecord;
};

#endif  // XFA_FWL_CORE_IFWL_EDIT_H_
