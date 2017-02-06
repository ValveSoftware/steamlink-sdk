// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FXEDIT_INCLUDE_FXET_EDIT_H_
#define FPDFSDK_FXEDIT_INCLUDE_FXET_EDIT_H_

#include <memory>

#include "core/fpdfdoc/include/cpvt_secprops.h"
#include "core/fpdfdoc/include/cpvt_wordprops.h"
#include "fpdfsdk/fxedit/include/fx_edit.h"

class CFX_Edit;
class CFX_Edit_Iterator;
class CFX_Edit_Provider;

#define FX_EDIT_IsFloatZero(f) (f < 0.0001 && f > -0.0001)
#define FX_EDIT_IsFloatEqual(fa, fb) FX_EDIT_IsFloatZero(fa - fb)
#define FX_EDIT_IsFloatBigger(fa, fb) (fa > fb && !FX_EDIT_IsFloatEqual(fa, fb))
#define FX_EDIT_IsFloatSmaller(fa, fb) \
  (fa < fb && !FX_EDIT_IsFloatEqual(fa, fb))

enum REFRESH_PLAN_E { RP_ANALYSE, RP_NOANALYSE, RP_OPTIONAL };

enum EDIT_PROPS_E {
  EP_LINELEADING,
  EP_LINEINDENT,
  EP_ALIGNMENT,
  EP_FONTINDEX,
  EP_FONTSIZE,
  EP_WORDCOLOR,
  EP_SCRIPTTYPE,
  EP_UNDERLINE,
  EP_CROSSOUT,
  EP_CHARSPACE,
  EP_HORZSCALE,
  EP_BOLD,
  EP_ITALIC
};

struct CFX_Edit_LineRect {
  CFX_Edit_LineRect(const CPVT_WordRange& wrLine, const CFX_FloatRect& rcLine)
      : m_wrLine(wrLine), m_rcLine(rcLine) {}

  FX_BOOL operator!=(const CFX_Edit_LineRect& linerect) const {
    return FXSYS_memcmp(this, &linerect, sizeof(CFX_Edit_LineRect)) != 0;
  }

  FX_BOOL IsSameHeight(const CFX_Edit_LineRect& linerect) const {
    return FX_EDIT_IsFloatZero(
        (m_rcLine.top - m_rcLine.bottom) -
        (linerect.m_rcLine.top - linerect.m_rcLine.bottom));
  }

  FX_BOOL IsSameTop(const CFX_Edit_LineRect& linerect) const {
    return FX_EDIT_IsFloatZero(m_rcLine.top - linerect.m_rcLine.top);
  }

  FX_BOOL IsSameLeft(const CFX_Edit_LineRect& linerect) const {
    return FX_EDIT_IsFloatZero(m_rcLine.left - linerect.m_rcLine.left);
  }

  FX_BOOL IsSameRight(const CFX_Edit_LineRect& linerect) const {
    return FX_EDIT_IsFloatZero(m_rcLine.right - linerect.m_rcLine.right);
  }

  CPVT_WordRange m_wrLine;
  CFX_FloatRect m_rcLine;
};

class CFX_Edit_LineRectArray {
 public:
  CFX_Edit_LineRectArray();
  virtual ~CFX_Edit_LineRectArray();

  void Empty();
  void RemoveAll();
  void operator=(CFX_Edit_LineRectArray& rects);
  void Add(const CPVT_WordRange& wrLine, const CFX_FloatRect& rcLine);

  int32_t GetSize() const;
  CFX_Edit_LineRect* GetAt(int32_t nIndex) const;

 private:
  CFX_ArrayTemplate<CFX_Edit_LineRect*> m_LineRects;
};

class CFX_Edit_RectArray {
 public:
  CFX_Edit_RectArray();
  virtual ~CFX_Edit_RectArray();

  void Empty();
  void Add(const CFX_FloatRect& rect);

  int32_t GetSize() const;
  CFX_FloatRect* GetAt(int32_t nIndex) const;

 private:
  CFX_ArrayTemplate<CFX_FloatRect*> m_Rects;
};

class CFX_Edit_Refresh {
 public:
  CFX_Edit_Refresh();
  virtual ~CFX_Edit_Refresh();

  void BeginRefresh();
  void Push(const CPVT_WordRange& linerange, const CFX_FloatRect& rect);
  void NoAnalyse();
  void Analyse(int32_t nAlignment);
  void AddRefresh(const CFX_FloatRect& rect);
  const CFX_Edit_RectArray* GetRefreshRects() const;
  void EndRefresh();

 private:
  CFX_Edit_LineRectArray m_NewLineRects;
  CFX_Edit_LineRectArray m_OldLineRects;
  CFX_Edit_RectArray m_RefreshRects;
};

class CFX_Edit_Select {
 public:
  CFX_Edit_Select();
  CFX_Edit_Select(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);
  explicit CFX_Edit_Select(const CPVT_WordRange& range);

  void Default();
  void Set(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);
  void SetBeginPos(const CPVT_WordPlace& begin);
  void SetEndPos(const CPVT_WordPlace& end);

  CPVT_WordRange ConvertToWordRange() const;
  FX_BOOL IsExist() const;
  FX_BOOL operator!=(const CPVT_WordRange& wr) const;

  CPVT_WordPlace BeginPos, EndPos;
};

class CFX_Edit_Undo {
 public:
  explicit CFX_Edit_Undo(int32_t nBufsize);
  virtual ~CFX_Edit_Undo();

  void Undo();
  void Redo();

  void AddItem(IFX_Edit_UndoItem* pItem);

  FX_BOOL CanUndo() const;
  FX_BOOL CanRedo() const;
  FX_BOOL IsModified() const;
  FX_BOOL IsWorking() const;

  void Reset();

  IFX_Edit_UndoItem* GetItem(int32_t nIndex);
  int32_t GetItemCount() { return m_UndoItemStack.GetSize(); }
  int32_t GetCurUndoPos() { return m_nCurUndoPos; }

 private:
  void SetBufSize(int32_t nSize) { m_nBufSize = nSize; }
  int32_t GetBufSize() { return m_nBufSize; }

  void RemoveHeads();
  void RemoveTails();

 private:
  CFX_ArrayTemplate<IFX_Edit_UndoItem*> m_UndoItemStack;

  int32_t m_nCurUndoPos;
  int32_t m_nBufSize;
  FX_BOOL m_bModified;
  FX_BOOL m_bVirgin;
  FX_BOOL m_bWorking;
};

class CFX_Edit_UndoItem : public IFX_Edit_UndoItem {
 public:
  CFX_Edit_UndoItem();
  ~CFX_Edit_UndoItem() override;

  CFX_WideString GetUndoTitle() override;

  void SetFirst(FX_BOOL bFirst);
  FX_BOOL IsFirst();
  void SetLast(FX_BOOL bLast);
  FX_BOOL IsLast();

 private:
  FX_BOOL m_bFirst;
  FX_BOOL m_bLast;
};

class CFX_Edit_GroupUndoItem : public IFX_Edit_UndoItem {
 public:
  explicit CFX_Edit_GroupUndoItem(const CFX_WideString& sTitle);
  ~CFX_Edit_GroupUndoItem() override;

  // IFX_Edit_UndoItem
  void Undo() override;
  void Redo() override;
  CFX_WideString GetUndoTitle() override;

  void AddUndoItem(CFX_Edit_UndoItem* pUndoItem);
  void UpdateItems();

 private:
  CFX_WideString m_sTitle;
  CFX_ArrayTemplate<CFX_Edit_UndoItem*> m_Items;
};

class CFXEU_InsertWord : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertWord(CFX_Edit* pEdit,
                   const CPVT_WordPlace& wpOldPlace,
                   const CPVT_WordPlace& wpNewPlace,
                   uint16_t word,
                   int32_t charset,
                   const CPVT_WordProps* pWordProps);
  ~CFXEU_InsertWord() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_WordProps m_WordProps;
};

class CFXEU_InsertReturn : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertReturn(CFX_Edit* pEdit,
                     const CPVT_WordPlace& wpOldPlace,
                     const CPVT_WordPlace& wpNewPlace,
                     const CPVT_SecProps* pSecProps,
                     const CPVT_WordProps* pWordProps);
  ~CFXEU_InsertReturn() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_Backspace : public CFX_Edit_UndoItem {
 public:
  CFXEU_Backspace(CFX_Edit* pEdit,
                  const CPVT_WordPlace& wpOldPlace,
                  const CPVT_WordPlace& wpNewPlace,
                  uint16_t word,
                  int32_t charset,
                  const CPVT_SecProps& SecProps,
                  const CPVT_WordProps& WordProps);
  ~CFXEU_Backspace() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_Delete : public CFX_Edit_UndoItem {
 public:
  CFXEU_Delete(CFX_Edit* pEdit,
               const CPVT_WordPlace& wpOldPlace,
               const CPVT_WordPlace& wpNewPlace,
               uint16_t word,
               int32_t charset,
               const CPVT_SecProps& SecProps,
               const CPVT_WordProps& WordProps,
               FX_BOOL bSecEnd);
  ~CFXEU_Delete() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
  FX_BOOL m_bSecEnd;
};

class CFXEU_Clear : public CFX_Edit_UndoItem {
 public:
  CFXEU_Clear(CFX_Edit* pEdit,
              const CPVT_WordRange& wrSel,
              const CFX_WideString& swText);
  ~CFXEU_Clear() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordRange m_wrSel;
  CFX_WideString m_swText;
};

class CFXEU_ClearRich : public CFX_Edit_UndoItem {
 public:
  CFXEU_ClearRich(CFX_Edit* pEdit,
                  const CPVT_WordPlace& wpOldPlace,
                  const CPVT_WordPlace& wpNewPlace,
                  const CPVT_WordRange& wrSel,
                  uint16_t word,
                  int32_t charset,
                  const CPVT_SecProps& SecProps,
                  const CPVT_WordProps& WordProps);
  ~CFXEU_ClearRich() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  CPVT_WordRange m_wrSel;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_InsertText : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertText(CFX_Edit* pEdit,
                   const CPVT_WordPlace& wpOldPlace,
                   const CPVT_WordPlace& wpNewPlace,
                   const CFX_WideString& swText,
                   int32_t charset,
                   const CPVT_SecProps* pSecProps,
                   const CPVT_WordProps* pWordProps);
  ~CFXEU_InsertText() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  CFX_WideString m_swText;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_SetSecProps : public CFX_Edit_UndoItem {
 public:
  CFXEU_SetSecProps(CFX_Edit* pEdit,
                    const CPVT_WordPlace& place,
                    EDIT_PROPS_E ep,
                    const CPVT_SecProps& oldsecprops,
                    const CPVT_WordProps& oldwordprops,
                    const CPVT_SecProps& newsecprops,
                    const CPVT_WordProps& newwordprops,
                    const CPVT_WordRange& range);
  ~CFXEU_SetSecProps() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;
  CPVT_WordPlace m_wpPlace;
  CPVT_WordRange m_wrPlace;
  EDIT_PROPS_E m_eProps;

  CPVT_SecProps m_OldSecProps;
  CPVT_SecProps m_NewSecProps;
  CPVT_WordProps m_OldWordProps;
  CPVT_WordProps m_NewWordProps;
};

class CFXEU_SetWordProps : public CFX_Edit_UndoItem {
 public:
  CFXEU_SetWordProps(CFX_Edit* pEdit,
                     const CPVT_WordPlace& place,
                     EDIT_PROPS_E ep,
                     const CPVT_WordProps& oldprops,
                     const CPVT_WordProps& newprops,
                     const CPVT_WordRange& range);
  ~CFXEU_SetWordProps() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;
  CPVT_WordPlace m_wpPlace;
  CPVT_WordRange m_wrPlace;
  EDIT_PROPS_E m_eProps;

  CPVT_WordProps m_OldWordProps;
  CPVT_WordProps m_NewWordProps;
};

class CFX_Edit : public IFX_Edit {
  friend class CFX_Edit_Iterator;
  friend class CFXEU_InsertWord;
  friend class CFXEU_InsertReturn;
  friend class CFXEU_Backspace;
  friend class CFXEU_Delete;
  friend class CFXEU_Clear;
  friend class CFXEU_ClearRich;
  friend class CFXEU_SetSecProps;
  friend class CFXEU_SetWordProps;
  friend class CFXEU_InsertText;

 public:
  explicit CFX_Edit(CPDF_VariableText* pVT);
  ~CFX_Edit() override;

  // IFX_Edit
  void SetFontMap(IPVT_FontMap* pFontMap) override;
  void SetNotify(IFX_Edit_Notify* pNotify) override;
  void SetOprNotify(IFX_Edit_OprNotify* pOprNotify) override;
  IFX_Edit_Iterator* GetIterator() override;
  CPDF_VariableText* GetVariableText() override;
  IPVT_FontMap* GetFontMap() override;
  void Initialize() override;
  void SetPlateRect(const CFX_FloatRect& rect, FX_BOOL bPaint = TRUE) override;
  void SetScrollPos(const CFX_FloatPoint& point) override;
  void SetAlignmentH(int32_t nFormat = 0, FX_BOOL bPaint = TRUE) override;
  void SetAlignmentV(int32_t nFormat = 0, FX_BOOL bPaint = TRUE) override;
  void SetPasswordChar(uint16_t wSubWord = '*', FX_BOOL bPaint = TRUE) override;
  void SetLimitChar(int32_t nLimitChar = 0, FX_BOOL bPaint = TRUE) override;
  void SetCharArray(int32_t nCharArray = 0, FX_BOOL bPaint = TRUE) override;
  void SetCharSpace(FX_FLOAT fCharSpace = 0.0f, FX_BOOL bPaint = TRUE) override;
  void SetHorzScale(int32_t nHorzScale = 100, FX_BOOL bPaint = TRUE) override;
  void SetLineLeading(FX_FLOAT fLineLeading, FX_BOOL bPaint = TRUE) override;
  void SetMultiLine(FX_BOOL bMultiLine = TRUE, FX_BOOL bPaint = TRUE) override;
  void SetAutoReturn(FX_BOOL bAuto = TRUE, FX_BOOL bPaint = TRUE) override;
  void SetAutoFontSize(FX_BOOL bAuto = TRUE, FX_BOOL bPaint = TRUE) override;
  void SetAutoScroll(FX_BOOL bAuto = TRUE, FX_BOOL bPaint = TRUE) override;
  void SetFontSize(FX_FLOAT fFontSize, FX_BOOL bPaint = TRUE) override;
  void SetTextOverflow(FX_BOOL bAllowed = FALSE,
                       FX_BOOL bPaint = TRUE) override;
  FX_BOOL IsRichText() const override;
  void SetRichText(FX_BOOL bRichText = TRUE, FX_BOOL bPaint = TRUE) override;
  FX_BOOL SetRichFontSize(FX_FLOAT fFontSize) override;
  FX_BOOL SetRichFontIndex(int32_t nFontIndex) override;
  FX_BOOL SetRichTextColor(FX_COLORREF dwColor) override;
  FX_BOOL SetRichTextScript(CPDF_VariableText::ScriptType nScriptType) override;
  FX_BOOL SetRichTextBold(FX_BOOL bBold = TRUE) override;
  FX_BOOL SetRichTextItalic(FX_BOOL bItalic = TRUE) override;
  FX_BOOL SetRichTextUnderline(FX_BOOL bUnderline = TRUE) override;
  FX_BOOL SetRichTextCrossout(FX_BOOL bCrossout = TRUE) override;
  FX_BOOL SetRichTextCharSpace(FX_FLOAT fCharSpace) override;
  FX_BOOL SetRichTextHorzScale(int32_t nHorzScale = 100) override;
  FX_BOOL SetRichTextLineLeading(FX_FLOAT fLineLeading) override;
  FX_BOOL SetRichTextLineIndent(FX_FLOAT fLineIndent) override;
  FX_BOOL SetRichTextAlignment(int32_t nAlignment) override;
  void OnMouseDown(const CFX_FloatPoint& point,
                   FX_BOOL bShift,
                   FX_BOOL bCtrl) override;
  void OnMouseMove(const CFX_FloatPoint& point,
                   FX_BOOL bShift,
                   FX_BOOL bCtrl) override;
  void OnVK_UP(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void OnVK_DOWN(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void OnVK_LEFT(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void OnVK_RIGHT(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void OnVK_HOME(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void OnVK_END(FX_BOOL bShift, FX_BOOL bCtrl) override;
  void SetText(const FX_WCHAR* text,
               int32_t charset = DEFAULT_CHARSET,
               const CPVT_SecProps* pSecProps = nullptr,
               const CPVT_WordProps* pWordProps = nullptr) override;
  FX_BOOL InsertWord(uint16_t word,
                     int32_t charset = DEFAULT_CHARSET,
                     const CPVT_WordProps* pWordProps = nullptr) override;
  FX_BOOL InsertReturn(const CPVT_SecProps* pSecProps = nullptr,
                       const CPVT_WordProps* pWordProps = nullptr) override;
  FX_BOOL Backspace() override;
  FX_BOOL Delete() override;
  FX_BOOL Clear() override;
  FX_BOOL InsertText(const FX_WCHAR* text,
                     int32_t charset = DEFAULT_CHARSET,
                     const CPVT_SecProps* pSecProps = nullptr,
                     const CPVT_WordProps* pWordProps = nullptr) override;
  FX_BOOL Redo() override;
  FX_BOOL Undo() override;
  int32_t WordPlaceToWordIndex(const CPVT_WordPlace& place) const override;
  CPVT_WordPlace WordIndexToWordPlace(int32_t index) const override;
  CPVT_WordPlace GetLineBeginPlace(const CPVT_WordPlace& place) const override;
  CPVT_WordPlace GetLineEndPlace(const CPVT_WordPlace& place) const override;
  CPVT_WordPlace GetSectionBeginPlace(
      const CPVT_WordPlace& place) const override;
  CPVT_WordPlace GetSectionEndPlace(const CPVT_WordPlace& place) const override;
  CPVT_WordPlace SearchWordPlace(const CFX_FloatPoint& point) const override;
  int32_t GetCaret() const override;
  CPVT_WordPlace GetCaretWordPlace() const override;
  CFX_WideString GetSelText() const override;
  CFX_WideString GetText() const override;
  FX_FLOAT GetFontSize() const override;
  uint16_t GetPasswordChar() const override;
  CFX_FloatPoint GetScrollPos() const override;
  int32_t GetCharArray() const override;
  CFX_FloatRect GetPlateRect() const override;
  CFX_FloatRect GetContentRect() const override;
  CFX_WideString GetRangeText(const CPVT_WordRange& range) const override;
  int32_t GetHorzScale() const override;
  FX_FLOAT GetCharSpace() const override;
  int32_t GetTotalWords() const override;
  void SetSel(int32_t nStartChar, int32_t nEndChar) override;
  void GetSel(int32_t& nStartChar, int32_t& nEndChar) const override;
  void SelectAll() override;
  void SelectNone() override;
  FX_BOOL IsSelected() const override;
  void Paint() override;
  void EnableNotify(FX_BOOL bNotify) override;
  void EnableRefresh(FX_BOOL bRefresh) override;
  void RefreshWordRange(const CPVT_WordRange& wr) override;
  void SetCaret(int32_t nPos) override;
  CPVT_WordRange GetWholeWordRange() const override;
  CPVT_WordRange GetSelectWordRange() const override;
  void EnableUndo(FX_BOOL bUndo) override;
  void EnableOprNotify(FX_BOOL bNotify) override;
  FX_BOOL IsTextFull() const override;
  FX_BOOL IsTextOverflow() const;
  FX_BOOL CanUndo() const override;
  FX_BOOL CanRedo() const override;
  FX_BOOL IsModified() const override;
  CPVT_WordRange GetVisibleWordRange() const override;
  void AddUndoItem(IFX_Edit_UndoItem* pUndoItem) override;

  FX_BOOL Empty();

  CPVT_WordPlace DoInsertText(const CPVT_WordPlace& place,
                              const FX_WCHAR* text,
                              int32_t charset,
                              const CPVT_SecProps* pSecProps,
                              const CPVT_WordProps* pWordProps);
  int32_t GetCharSetFromUnicode(uint16_t word, int32_t nOldCharset);

  int32_t GetTotalLines() const;

 private:
  void SetSel(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);

  void RearrangeAll();
  void RearrangePart(const CPVT_WordRange& range);
  void ScrollToCaret();
  void SetScrollInfo();
  void SetScrollPosX(FX_FLOAT fx);
  void SetScrollPosY(FX_FLOAT fy);
  void SetScrollLimit();
  void SetContentChanged();

  void SetText(const FX_WCHAR* text,
               int32_t charset,
               const CPVT_SecProps* pSecProps,
               const CPVT_WordProps* pWordProps,
               FX_BOOL bAddUndo,
               FX_BOOL bPaint);
  FX_BOOL InsertWord(uint16_t word,
                     int32_t charset,
                     const CPVT_WordProps* pWordProps,
                     FX_BOOL bAddUndo,
                     FX_BOOL bPaint);
  FX_BOOL InsertReturn(const CPVT_SecProps* pSecProps,
                       const CPVT_WordProps* pWordProps,
                       FX_BOOL bAddUndo,
                       FX_BOOL bPaint);
  FX_BOOL Backspace(FX_BOOL bAddUndo, FX_BOOL bPaint);
  FX_BOOL Delete(FX_BOOL bAddUndo, FX_BOOL bPaint);
  FX_BOOL Clear(FX_BOOL bAddUndo, FX_BOOL bPaint);
  FX_BOOL InsertText(const FX_WCHAR* text,
                     int32_t charset,
                     const CPVT_SecProps* pSecProps,
                     const CPVT_WordProps* pWordProps,
                     FX_BOOL bAddUndo,
                     FX_BOOL bPaint);
  FX_BOOL SetRichTextProps(EDIT_PROPS_E eProps,
                           const CPVT_SecProps* pSecProps,
                           const CPVT_WordProps* pWordProps);
  FX_BOOL SetSecProps(EDIT_PROPS_E eProps,
                      const CPVT_WordPlace& place,
                      const CPVT_SecProps* pSecProps,
                      const CPVT_WordProps* pWordProps,
                      const CPVT_WordRange& wr,
                      FX_BOOL bAddUndo);
  FX_BOOL SetWordProps(EDIT_PROPS_E eProps,
                       const CPVT_WordPlace& place,
                       const CPVT_WordProps* pWordProps,
                       const CPVT_WordRange& wr,
                       FX_BOOL bAddUndo);
  void PaintSetProps(EDIT_PROPS_E eProps, const CPVT_WordRange& wr);
  void PaintInsertText(const CPVT_WordPlace& wpOld,
                       const CPVT_WordPlace& wpNew);

  inline CFX_FloatPoint VTToEdit(const CFX_FloatPoint& point) const;
  inline CFX_FloatPoint EditToVT(const CFX_FloatPoint& point) const;
  inline CFX_FloatRect VTToEdit(const CFX_FloatRect& rect) const;
  inline CFX_FloatRect EditToVT(const CFX_FloatRect& rect) const;

  void Refresh(REFRESH_PLAN_E ePlan,
               const CPVT_WordRange* pRange1 = nullptr,
               const CPVT_WordRange* pRange2 = nullptr);
  void RefreshPushLineRects(const CPVT_WordRange& wr);
  void RefreshPushRandomRects(const CPVT_WordRange& wr);

  void SetCaret(const CPVT_WordPlace& place);
  void SetCaretInfo();
  void SetCaretOrigin();
  void SetCaretChange();

  CPVT_WordRange GetLatinWordsRange(const CPVT_WordPlace& place) const;
  CPVT_WordRange CombineWordRange(const CPVT_WordRange& wr1,
                                  const CPVT_WordRange& wr2);

  void BeginGroupUndo(const CFX_WideString& sTitle);
  void EndGroupUndo();
  void AddEditUndoItem(CFX_Edit_UndoItem* pEditUndoItem);

  void SetPageInfo(const CPVT_WordPlace& place);
  CPVT_WordPlace SearchPageEndPlace(const CPVT_WordPlace& wpPageBegin,
                                    const CFX_FloatPoint& point) const;
  FX_FLOAT GetLineTop(const CPVT_WordPlace& place) const;
  FX_FLOAT GetLineBottom(const CPVT_WordPlace& place) const;

 private:
  CPDF_VariableText* m_pVT;
  IFX_Edit_Notify* m_pNotify;
  IFX_Edit_OprNotify* m_pOprNotify;
  std::unique_ptr<CFX_Edit_Provider> m_pVTProvider;

  CPVT_WordPlace m_wpCaret;
  CPVT_WordPlace m_wpOldCaret;
  CFX_Edit_Select m_SelState;

  CFX_FloatPoint m_ptScrollPos;
  CFX_FloatPoint m_ptRefreshScrollPos;
  FX_BOOL m_bEnableScroll;
  std::unique_ptr<IFX_Edit_Iterator> m_pIterator;
  CFX_Edit_Refresh m_Refresh;
  CFX_FloatPoint m_ptCaret;
  CFX_Edit_Undo m_Undo;
  int32_t m_nAlignment;
  FX_BOOL m_bNotifyFlag;
  FX_BOOL m_bEnableOverflow;
  FX_BOOL m_bEnableRefresh;
  CFX_FloatRect m_rcOldContent;
  FX_BOOL m_bEnableUndo;
  FX_BOOL m_bNotify;
  FX_BOOL m_bOprNotify;
  CFX_Edit_GroupUndoItem* m_pGroupUndoItem;
};

class CFX_Edit_Iterator : public IFX_Edit_Iterator {
 public:
  CFX_Edit_Iterator(CFX_Edit* pEdit, CPDF_VariableText::Iterator* pVTIterator);
  ~CFX_Edit_Iterator() override;

  // IFX_Edit_Iterator
  FX_BOOL NextWord() override;
  FX_BOOL NextLine() override;
  FX_BOOL NextSection() override;
  FX_BOOL PrevWord() override;
  FX_BOOL PrevLine() override;
  FX_BOOL PrevSection() override;
  FX_BOOL GetWord(CPVT_Word& word) const override;
  FX_BOOL GetLine(CPVT_Line& line) const override;
  FX_BOOL GetSection(CPVT_Section& section) const override;
  void SetAt(int32_t nWordIndex) override;
  void SetAt(const CPVT_WordPlace& place) override;
  const CPVT_WordPlace& GetAt() const override;
  IFX_Edit* GetEdit() const override;

 private:
  CFX_Edit* m_pEdit;
  CPDF_VariableText::Iterator* m_pVTIterator;
};

class CFX_Edit_Provider : public CPDF_VariableText::Provider {
 public:
  explicit CFX_Edit_Provider(IPVT_FontMap* pFontMap);
  ~CFX_Edit_Provider() override;

  IPVT_FontMap* GetFontMap();

  // CPDF_VariableText::Provider:
  int32_t GetCharWidth(int32_t nFontIndex,
                       uint16_t word,
                       int32_t nWordStyle) override;
  int32_t GetTypeAscent(int32_t nFontIndex) override;
  int32_t GetTypeDescent(int32_t nFontIndex) override;
  int32_t GetWordFontIndex(uint16_t word,
                           int32_t charset,
                           int32_t nFontIndex) override;
  int32_t GetDefaultFontIndex() override;
  FX_BOOL IsLatinWord(uint16_t word) override;

 private:
  IPVT_FontMap* m_pFontMap;
};

#endif  // FPDFSDK_FXEDIT_INCLUDE_FXET_EDIT_H_
