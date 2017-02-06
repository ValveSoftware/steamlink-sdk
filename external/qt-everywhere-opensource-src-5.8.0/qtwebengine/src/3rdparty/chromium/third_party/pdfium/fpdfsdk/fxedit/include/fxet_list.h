// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FXEDIT_INCLUDE_FXET_LIST_H_
#define FPDFSDK_FXEDIT_INCLUDE_FXET_LIST_H_

#include "fpdfsdk/fxedit/include/fx_edit.h"

class IFX_Edit;

class CLST_Size {
 public:
  CLST_Size() : x(0.0f), y(0.0f) {}

  CLST_Size(FX_FLOAT other_x, FX_FLOAT other_y) {
    x = other_x;
    y = other_y;
  }

  void Default() {
    x = 0.0f;
    y = 0.0f;
  }

  FX_BOOL operator!=(const CLST_Size& size) const {
    return FXSYS_memcmp(this, &size, sizeof(CLST_Size)) != 0;
  }

  FX_FLOAT x, y;
};

class CLST_Rect : public CFX_FloatRect {
 public:
  CLST_Rect() { left = top = right = bottom = 0.0f; }

  CLST_Rect(FX_FLOAT other_left,
            FX_FLOAT other_top,
            FX_FLOAT other_right,
            FX_FLOAT other_bottom) {
    left = other_left;
    top = other_top;
    right = other_right;
    bottom = other_bottom;
  }

  explicit CLST_Rect(const CFX_FloatRect& rect) {
    left = rect.left;
    top = rect.top;
    right = rect.right;
    bottom = rect.bottom;
  }

  ~CLST_Rect() {}

  void Default() { left = top = right = bottom = 0.0f; }

  const CLST_Rect operator=(const CFX_FloatRect& rect) {
    left = rect.left;
    top = rect.top;
    right = rect.right;
    bottom = rect.bottom;

    return *this;
  }

  bool operator==(const CLST_Rect& rect) const {
    return FXSYS_memcmp(this, &rect, sizeof(CLST_Rect)) == 0;
  }

  bool operator!=(const CLST_Rect& rect) const { return !(*this == rect); }

  FX_FLOAT Width() const { return right - left; }

  FX_FLOAT Height() const {
    if (top > bottom)
      return top - bottom;
    return bottom - top;
  }

  CFX_FloatPoint LeftTop() const { return CFX_FloatPoint(left, top); }

  CFX_FloatPoint RightBottom() const { return CFX_FloatPoint(right, bottom); }

  const CLST_Rect operator+=(const CFX_FloatPoint& point) {
    left += point.x;
    right += point.x;
    top += point.y;
    bottom += point.y;

    return *this;
  }

  const CLST_Rect operator-=(const CFX_FloatPoint& point) {
    left -= point.x;
    right -= point.x;
    top -= point.y;
    bottom -= point.y;

    return *this;
  }

  CLST_Rect operator+(const CFX_FloatPoint& point) const {
    return CLST_Rect(left + point.x, top + point.y, right + point.x,
                     bottom + point.y);
  }

  CLST_Rect operator-(const CFX_FloatPoint& point) const {
    return CLST_Rect(left - point.x, top - point.y, right - point.x,
                     bottom - point.y);
  }
};

class CFX_ListItem final {
 public:
  CFX_ListItem();
  ~CFX_ListItem();

  void SetFontMap(IPVT_FontMap* pFontMap);
  IFX_Edit* GetEdit() const;

  void SetRect(const CLST_Rect& rect);
  void SetSelect(FX_BOOL bSelected);
  void SetText(const FX_WCHAR* text);
  void SetFontSize(FX_FLOAT fFontSize);
  CFX_WideString GetText() const;

  CLST_Rect GetRect() const;
  FX_BOOL IsSelected() const;
  FX_FLOAT GetItemHeight() const;
  uint16_t GetFirstChar() const;

 private:
  IFX_Edit_Iterator* GetIterator() const;

  IFX_Edit* m_pEdit;
  FX_BOOL m_bSelected;
  CLST_Rect m_rcListItem;
};

class CFX_ListContainer {
 public:
  CFX_ListContainer();
  virtual ~CFX_ListContainer();
  virtual void SetPlateRect(const CFX_FloatRect& rect);

  CFX_FloatRect GetPlateRect() const { return m_rcPlate; }
  void SetContentRect(const CLST_Rect& rect) { m_rcContent = rect; }
  CLST_Rect GetContentRect() const { return m_rcContent; }
  CFX_FloatPoint GetBTPoint() const {
    return CFX_FloatPoint(m_rcPlate.left, m_rcPlate.top);
  }
  CFX_FloatPoint GetETPoint() const {
    return CFX_FloatPoint(m_rcPlate.right, m_rcPlate.bottom);
  }

 public:
  CFX_FloatPoint InnerToOuter(const CFX_FloatPoint& point) const {
    return CFX_FloatPoint(point.x + GetBTPoint().x, GetBTPoint().y - point.y);
  }
  CFX_FloatPoint OuterToInner(const CFX_FloatPoint& point) const {
    return CFX_FloatPoint(point.x - GetBTPoint().x, GetBTPoint().y - point.y);
  }
  CFX_FloatRect InnerToOuter(const CLST_Rect& rect) const {
    CFX_FloatPoint ptLeftTop =
        InnerToOuter(CFX_FloatPoint(rect.left, rect.top));
    CFX_FloatPoint ptRightBottom =
        InnerToOuter(CFX_FloatPoint(rect.right, rect.bottom));
    return CFX_FloatRect(ptLeftTop.x, ptRightBottom.y, ptRightBottom.x,
                         ptLeftTop.y);
  }
  CLST_Rect OuterToInner(const CFX_FloatRect& rect) const {
    CFX_FloatPoint ptLeftTop =
        OuterToInner(CFX_FloatPoint(rect.left, rect.top));
    CFX_FloatPoint ptRightBottom =
        OuterToInner(CFX_FloatPoint(rect.right, rect.bottom));
    return CLST_Rect(ptLeftTop.x, ptLeftTop.y, ptRightBottom.x,
                     ptRightBottom.y);
  }

 private:
  CFX_FloatRect m_rcPlate;
  CLST_Rect m_rcContent;  // positive forever!
};

template <class TYPE>
class CLST_ArrayTemplate : public CFX_ArrayTemplate<TYPE> {
 public:
  FX_BOOL IsEmpty() { return CFX_ArrayTemplate<TYPE>::GetSize() <= 0; }
  TYPE GetAt(int32_t nIndex) const {
    if (nIndex >= 0 && nIndex < CFX_ArrayTemplate<TYPE>::GetSize())
      return CFX_ArrayTemplate<TYPE>::GetAt(nIndex);
    return nullptr;
  }
  void RemoveAt(int32_t nIndex) {
    if (nIndex >= 0 && nIndex < CFX_ArrayTemplate<TYPE>::GetSize())
      CFX_ArrayTemplate<TYPE>::RemoveAt(nIndex);
  }
};

class CFX_List : protected CFX_ListContainer, public IFX_List {
 public:
  CFX_List();
  ~CFX_List() override;

  // IFX_List:
  void SetFontMap(IPVT_FontMap* pFontMap) override;
  void SetFontSize(FX_FLOAT fFontSize) override;
  CFX_FloatRect GetPlateRect() const override;
  CFX_FloatRect GetContentRect() const override;
  FX_FLOAT GetFontSize() const override;
  IFX_Edit* GetItemEdit(int32_t nIndex) const override;
  int32_t GetCount() const override;
  FX_BOOL IsItemSelected(int32_t nIndex) const override;
  FX_FLOAT GetFirstHeight() const override;
  void SetMultipleSel(FX_BOOL bMultiple) override;
  FX_BOOL IsMultipleSel() const override;
  FX_BOOL IsValid(int32_t nItemIndex) const override;
  int32_t FindNext(int32_t nIndex, FX_WCHAR nChar) const override;
  void Empty() override;
  CFX_FloatRect GetItemRect(int32_t nIndex) const override;
  int32_t GetItemIndex(const CFX_FloatPoint& point) const override;
  int32_t GetFirstSelected() const override;

 protected:
  void AddItem(const FX_WCHAR* str);
  virtual void ReArrange(int32_t nItemIndex);
  CFX_WideString GetItemText(int32_t nIndex) const;
  void SetItemSelect(int32_t nItemIndex, FX_BOOL bSelected);
  int32_t GetLastSelected() const;
  FX_WCHAR Toupper(FX_WCHAR c) const;

 private:
  CLST_ArrayTemplate<CFX_ListItem*> m_aListItems;
  FX_FLOAT m_fFontSize;
  IPVT_FontMap* m_pFontMap;
  FX_BOOL m_bMultiple;
};

struct CPLST_Select_Item {
  CPLST_Select_Item(int32_t other_nItemIndex, int32_t other_nState) {
    nItemIndex = other_nItemIndex;
    nState = other_nState;
  }

  int32_t nItemIndex;
  int32_t nState;  // 0:normal select -1:to deselect 1: to select
};

class CPLST_Select {
 public:
  CPLST_Select();
  virtual ~CPLST_Select();

 public:
  void Add(int32_t nItemIndex);
  void Add(int32_t nBeginIndex, int32_t nEndIndex);
  void Sub(int32_t nItemIndex);
  void Sub(int32_t nBeginIndex, int32_t nEndIndex);
  FX_BOOL IsExist(int32_t nItemIndex) const;
  int32_t Find(int32_t nItemIndex) const;
  int32_t GetCount() const;
  int32_t GetItemIndex(int32_t nIndex) const;
  int32_t GetState(int32_t nIndex) const;
  void Done();
  void DeselectAll();

 private:
  CFX_ArrayTemplate<CPLST_Select_Item*> m_aItems;
};

class CFX_ListCtrl : public CFX_List {
 public:
  CFX_ListCtrl();
  ~CFX_ListCtrl() override;

  // CFX_List
  void SetNotify(IFX_List_Notify* pNotify) override;
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
  void OnVK(int32_t nItemIndex, FX_BOOL bShift, FX_BOOL bCtrl) override;
  FX_BOOL OnChar(uint16_t nChar, FX_BOOL bShift, FX_BOOL bCtrl) override;
  void SetPlateRect(const CFX_FloatRect& rect) override;
  void SetScrollPos(const CFX_FloatPoint& point) override;
  void ScrollToListItem(int32_t nItemIndex) override;
  CFX_FloatRect GetItemRect(int32_t nIndex) const override;
  int32_t GetCaret() const override;
  int32_t GetSelect() const override;
  int32_t GetTopItem() const override;
  CFX_FloatRect GetContentRect() const override;
  int32_t GetItemIndex(const CFX_FloatPoint& point) const override;
  void AddString(const FX_WCHAR* str) override;
  void SetTopItem(int32_t nIndex) override;
  void Select(int32_t nItemIndex) override;
  void SetCaret(int32_t nItemIndex) override;
  void Empty() override;
  void Cancel() override;
  CFX_WideString GetText() const override;
  void ReArrange(int32_t nItemIndex) override;

  virtual CFX_FloatPoint InToOut(const CFX_FloatPoint& point) const;
  virtual CFX_FloatPoint OutToIn(const CFX_FloatPoint& point) const;
  virtual CFX_FloatRect InToOut(const CFX_FloatRect& rect) const;
  virtual CFX_FloatRect OutToIn(const CFX_FloatRect& rect) const;

 private:
  void SetMultipleSelect(int32_t nItemIndex, FX_BOOL bSelected);
  void SetSingleSelect(int32_t nItemIndex);
  void InvalidateItem(int32_t nItemIndex);
  void SelectItems();
  FX_BOOL IsItemVisible(int32_t nItemIndex) const;
  void SetScrollInfo();
  void SetScrollPosY(FX_FLOAT fy);

 private:
  IFX_List_Notify* m_pNotify;
  FX_BOOL m_bNotifyFlag;
  CFX_FloatPoint m_ptScrollPos;
  CPLST_Select m_aSelItems;  // for multiple
  int32_t m_nSelItem;        // for single
  int32_t m_nFootIndex;      // for multiple
  FX_BOOL m_bCtrlSel;        // for multiple
  int32_t m_nCaretIndex;     // for multiple
};

#endif  // FPDFSDK_FXEDIT_INCLUDE_FXET_LIST_H_
