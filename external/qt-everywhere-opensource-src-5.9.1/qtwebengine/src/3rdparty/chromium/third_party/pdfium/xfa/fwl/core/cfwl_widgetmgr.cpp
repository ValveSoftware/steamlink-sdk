// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_widgetmgr.h"

#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_form.h"
#include "xfa/fxfa/app/xfa_fwladapter.h"
#include "xfa/fxfa/xfa_ffapp.h"

namespace {

const int kNeedRepaintHitPoints = 12;
const int kNeedRepaintHitPiece = 3;

struct FWL_NEEDREPAINTHITDATA {
  CFX_PointF hitPoint;
  bool bNotNeedRepaint;
  bool bNotContainByDirty;
};

}  // namespace

bool FWL_UseOffscreen(IFWL_Widget* pWidget) {
#if (_FX_OS_ == _FX_MACOSX_)
  return false;
#else
  return !!(pWidget->GetStyles() & FWL_WGTSTYLE_Offscreen);
#endif
}

CFWL_WidgetMgr::CFWL_WidgetMgr(CXFA_FFApp* pAdapterNative)
    : m_dwCapability(0), m_pAdapter(pAdapterNative->GetWidgetMgr(this)) {
  ASSERT(m_pAdapter);
  m_mapWidgetItem[nullptr].reset(new CFWL_WidgetMgrItem);
#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
  m_rtScreen.Reset();
#endif
}

CFWL_WidgetMgr::~CFWL_WidgetMgr() {}

IFWL_Widget* CFWL_WidgetMgr::GetParentWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  return pItem && pItem->pParent ? pItem->pParent->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetOwnerWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  return pItem && pItem->pOwner ? pItem->pOwner->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetFirstSiblingWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!pItem)
    return nullptr;

  pItem = pItem->pPrevious;
  while (pItem && pItem->pPrevious)
    pItem = pItem->pPrevious;
  return pItem ? pItem->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetPriorSiblingWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  return pItem && pItem->pPrevious ? pItem->pPrevious->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetNextSiblingWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  return pItem && pItem->pNext ? pItem->pNext->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetFirstChildWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  return pItem && pItem->pChild ? pItem->pChild->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetLastChildWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!pItem)
    return nullptr;

  pItem = pItem->pChild;
  while (pItem && pItem->pNext)
    pItem = pItem->pNext;
  return pItem ? pItem->pWidget : nullptr;
}

IFWL_Widget* CFWL_WidgetMgr::GetSystemFormWidget(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  while (pItem) {
    if (IsAbleNative(pItem->pWidget))
      return pItem->pWidget;
    pItem = pItem->pParent;
  }
  return nullptr;
}

void CFWL_WidgetMgr::SetWidgetIndex(IFWL_Widget* pWidget, int32_t nIndex) {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!pItem)
    return;
  if (!pItem->pParent)
    return;

  CFWL_WidgetMgrItem* pChild = pItem->pParent->pChild;
  int32_t i = 0;
  while (pChild) {
    if (pChild == pItem) {
      if (i == nIndex)
        return;
      if (pChild->pPrevious)
        pChild->pPrevious->pNext = pChild->pNext;
      if (pChild->pNext)
        pChild->pNext->pPrevious = pChild->pPrevious;
      if (pItem->pParent->pChild == pItem)
        pItem->pParent->pChild = pItem->pNext;

      pItem->pNext = nullptr;
      pItem->pPrevious = nullptr;
      break;
    }
    if (!pChild->pNext)
      break;

    pChild = pChild->pNext;
    ++i;
  }

  pChild = pItem->pParent->pChild;
  if (pChild) {
    if (nIndex < 0) {
      while (pChild->pNext)
        pChild = pChild->pNext;

      pChild->pNext = pItem;
      pItem->pPrevious = pChild;
      pItem->pNext = nullptr;
      return;
    }

    i = 0;
    while (i < nIndex && pChild->pNext) {
      pChild = pChild->pNext;
      ++i;
    }
    if (!pChild->pNext) {
      pChild->pNext = pItem;
      pItem->pPrevious = pChild;
      pItem->pNext = nullptr;
      return;
    }
    if (pChild->pPrevious) {
      pItem->pPrevious = pChild->pPrevious;
      pChild->pPrevious->pNext = pItem;
    }
    pChild->pPrevious = pItem;
    pItem->pNext = pChild;
    if (pItem->pParent->pChild == pChild)
      pItem->pParent->pChild = pItem;
  } else {
    pItem->pParent->pChild = pItem;
    pItem->pPrevious = nullptr;
    pItem->pNext = nullptr;
  }
}

void CFWL_WidgetMgr::RepaintWidget(IFWL_Widget* pWidget,
                                   const CFX_RectF* pRect) {
  if (!m_pAdapter)
    return;

  IFWL_Widget* pNative = pWidget;
  CFX_RectF rect(*pRect);
  if (IsFormDisabled()) {
    IFWL_Widget* pOuter = pWidget->GetOuter();
    while (pOuter) {
      CFX_RectF rtTemp;
      pNative->GetWidgetRect(rtTemp);
      rect.left += rtTemp.left;
      rect.top += rtTemp.top;
      pNative = pOuter;
      pOuter = pOuter->GetOuter();
    }
  } else if (!IsAbleNative(pWidget)) {
    pNative = GetSystemFormWidget(pWidget);
    if (!pNative)
      return;

    pWidget->TransformTo(pNative, rect.left, rect.top);
  }
  AddRedrawCounts(pNative);
  m_pAdapter->RepaintWidget(pNative, &rect);
}

void CFWL_WidgetMgr::InsertWidget(IFWL_Widget* pParent,
                                  IFWL_Widget* pChild,
                                  int32_t nIndex) {
  CFWL_WidgetMgrItem* pParentItem = GetWidgetMgrItem(pParent);
  if (!pParentItem) {
    pParentItem = new CFWL_WidgetMgrItem(pParent);
    m_mapWidgetItem[pParent].reset(pParentItem);
    pParentItem->pParent = GetWidgetMgrItem(nullptr);
    SetWidgetIndex(pParent, -1);
  }

  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pChild);
  if (!pItem) {
    pItem = new CFWL_WidgetMgrItem(pChild);
    m_mapWidgetItem[pChild].reset(pItem);
  }
  if (pItem->pParent && pItem->pParent != pParentItem) {
    if (pItem->pPrevious)
      pItem->pPrevious->pNext = pItem->pNext;
    if (pItem->pNext)
      pItem->pNext->pPrevious = pItem->pPrevious;
    if (pItem->pParent->pChild == pItem)
      pItem->pParent->pChild = pItem->pNext;
  }
  pItem->pParent = pParentItem;
  SetWidgetIndex(pChild, nIndex);
}

void CFWL_WidgetMgr::RemoveWidget(IFWL_Widget* pWidget) {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!pItem)
    return;
  if (pItem->pPrevious)
    pItem->pPrevious->pNext = pItem->pNext;
  if (pItem->pNext)
    pItem->pNext->pPrevious = pItem->pPrevious;
  if (pItem->pParent && pItem->pParent->pChild == pItem)
    pItem->pParent->pChild = pItem->pNext;

  CFWL_WidgetMgrItem* pChild = pItem->pChild;
  while (pChild) {
    CFWL_WidgetMgrItem* pNext = pChild->pNext;
    RemoveWidget(pChild->pWidget);
    pChild = pNext;
  }
  m_mapWidgetItem.erase(pWidget);
}

void CFWL_WidgetMgr::SetOwner(IFWL_Widget* pOwner, IFWL_Widget* pOwned) {
  CFWL_WidgetMgrItem* pParentItem = GetWidgetMgrItem(pOwner);
  if (!pParentItem) {
    pParentItem = new CFWL_WidgetMgrItem(pOwner);
    m_mapWidgetItem[pOwner].reset(pParentItem);
    pParentItem->pParent = GetWidgetMgrItem(nullptr);
    SetWidgetIndex(pOwner, -1);
  }

  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pOwned);
  if (!pItem) {
    pItem = new CFWL_WidgetMgrItem(pOwned);
    m_mapWidgetItem[pOwned].reset(pItem);
  }
  pItem->pOwner = pParentItem;
}
void CFWL_WidgetMgr::SetParent(IFWL_Widget* pParent, IFWL_Widget* pChild) {
  CFWL_WidgetMgrItem* pParentItem = GetWidgetMgrItem(pParent);
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pChild);
  if (!pItem)
    return;
  if (pItem->pParent && pItem->pParent != pParentItem) {
    if (pItem->pPrevious)
      pItem->pPrevious->pNext = pItem->pNext;
    if (pItem->pNext)
      pItem->pNext->pPrevious = pItem->pPrevious;
    if (pItem->pParent->pChild == pItem)
      pItem->pParent->pChild = pItem->pNext;

    pItem->pNext = nullptr;
    pItem->pPrevious = nullptr;
  }
  pItem->pParent = pParentItem;
  SetWidgetIndex(pChild, -1);
}

void CFWL_WidgetMgr::SetWidgetRect_Native(IFWL_Widget* pWidget,
                                          const CFX_RectF& rect) {
  if (!FWL_UseOffscreen(pWidget))
    return;

  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  pItem->iRedrawCounter++;
  if (pItem->pOffscreen) {
    CFX_RenderDevice* pDevice = pItem->pOffscreen->GetRenderDevice();
    if (pDevice && pDevice->GetBitmap()) {
      CFX_DIBitmap* pBitmap = pDevice->GetBitmap();
      if (pBitmap->GetWidth() - rect.width > 1 ||
          pBitmap->GetHeight() - rect.height > 1) {
        pItem->pOffscreen.reset();
      }
    }
  }
#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
  pItem->bOutsideChanged = !m_rtScreen.Contains(rect);
#endif
}

IFWL_Widget* CFWL_WidgetMgr::GetWidgetAtPoint(IFWL_Widget* parent,
                                              FX_FLOAT x,
                                              FX_FLOAT y) {
  if (!parent)
    return nullptr;

  FX_FLOAT x1;
  FX_FLOAT y1;
  IFWL_Widget* child = GetLastChildWidget(parent);
  while (child) {
    if ((child->GetStates() & FWL_WGTSTATE_Invisible) == 0) {
      x1 = x;
      y1 = y;
      CFX_Matrix matrixOnParent;
      child->GetMatrix(matrixOnParent);
      CFX_Matrix m;
      m.SetIdentity();
      m.SetReverse(matrixOnParent);
      m.TransformPoint(x1, y1);
      CFX_RectF bounds;
      child->GetWidgetRect(bounds);
      if (bounds.Contains(x1, y1)) {
        x1 -= bounds.left;
        y1 -= bounds.top;
        return GetWidgetAtPoint(child, x1, y1);
      }
    }
    child = GetPriorSiblingWidget(child);
  }
  return parent;
}

void CFWL_WidgetMgr::NotifySizeChanged(IFWL_Widget* pForm,
                                       FX_FLOAT fx,
                                       FX_FLOAT fy) {
  if (FWL_UseOffscreen(pForm))
    GetWidgetMgrItem(pForm)->pOffscreen.reset();
}

IFWL_Widget* CFWL_WidgetMgr::NextTab(IFWL_Widget* parent,
                                     IFWL_Widget* focus,
                                     bool& bFind) {
  CFWL_WidgetMgr* pMgr = parent->GetOwnerApp()->GetWidgetMgr();
  IFWL_Widget* child = pMgr->GetFirstChildWidget(parent);
  while (child) {
    if (focus == child)
      bFind = true;

    if ((child->GetStyles() & FWL_WGTSTYLE_TabStop) &&
        (!focus || (focus != child && bFind))) {
      return child;
    }
    IFWL_Widget* bRet = NextTab(child, focus, bFind);
    if (bRet)
      return bRet;

    child = pMgr->GetNextSiblingWidget(child);
  }
  return nullptr;
}

int32_t CFWL_WidgetMgr::CountRadioButtonGroup(IFWL_Widget* pFirst) const {
  int32_t iRet = 0;
  IFWL_Widget* pChild = pFirst;
  while (pChild) {
    pChild = GetNextSiblingWidget(pChild);
    ++iRet;
  }
  return iRet;
}

IFWL_Widget* CFWL_WidgetMgr::GetRadioButtonGroupHeader(
    IFWL_Widget* pRadioButton) const {
  IFWL_Widget* pNext = pRadioButton;
  if (pNext && (pNext->GetStyles() & FWL_WGTSTYLE_Group))
    return pNext;
  return nullptr;
}

void CFWL_WidgetMgr::GetSameGroupRadioButton(
    IFWL_Widget* pRadioButton,
    CFX_ArrayTemplate<IFWL_Widget*>& group) const {
  IFWL_Widget* pFirst = GetFirstSiblingWidget(pRadioButton);
  if (!pFirst)
    pFirst = pRadioButton;

  int32_t iGroup = CountRadioButtonGroup(pFirst);
  if (iGroup < 2)
    return;
  group.Add(GetRadioButtonGroupHeader(pRadioButton));
}

IFWL_Widget* CFWL_WidgetMgr::GetDefaultButton(IFWL_Widget* pParent) const {
  if ((pParent->GetClassID() == FWL_Type::PushButton) &&
      (pParent->GetStates() & (1 << (FWL_WGTSTATE_MAX + 2)))) {
    return pParent;
  }

  IFWL_Widget* child =
      pParent->GetOwnerApp()->GetWidgetMgr()->GetFirstChildWidget(pParent);
  while (child) {
    if ((child->GetClassID() == FWL_Type::PushButton) &&
        (child->GetStates() & (1 << (FWL_WGTSTATE_MAX + 2)))) {
      return child;
    }
    if (IFWL_Widget* find = GetDefaultButton(child))
      return find;

    child = child->GetOwnerApp()->GetWidgetMgr()->GetNextSiblingWidget(child);
  }
  return nullptr;
}

void CFWL_WidgetMgr::AddRedrawCounts(IFWL_Widget* pWidget) {
  GetWidgetMgrItem(pWidget)->iRedrawCounter++;
}

void CFWL_WidgetMgr::ResetRedrawCounts(IFWL_Widget* pWidget) {
  GetWidgetMgrItem(pWidget)->iRedrawCounter = 0;
}

CFWL_WidgetMgrItem* CFWL_WidgetMgr::GetWidgetMgrItem(
    IFWL_Widget* pWidget) const {
  auto it = m_mapWidgetItem.find(pWidget);
  return it != m_mapWidgetItem.end()
             ? static_cast<CFWL_WidgetMgrItem*>(it->second.get())
             : nullptr;
}

bool CFWL_WidgetMgr::IsAbleNative(IFWL_Widget* pWidget) const {
  if (!pWidget)
    return false;
  if (!pWidget->IsInstance(FX_WSTRC(FWL_CLASS_Form)))
    return false;

  uint32_t dwStyles = pWidget->GetStyles();
  return ((dwStyles & FWL_WGTSTYLE_WindowTypeMask) ==
          FWL_WGTSTYLE_OverLapper) ||
         (dwStyles & FWL_WGTSTYLE_Popup);
}

void CFWL_WidgetMgr::GetAdapterPopupPos(IFWL_Widget* pWidget,
                                        FX_FLOAT fMinHeight,
                                        FX_FLOAT fMaxHeight,
                                        const CFX_RectF& rtAnchor,
                                        CFX_RectF& rtPopup) const {
  m_pAdapter->GetPopupPos(pWidget, fMinHeight, fMaxHeight, rtAnchor, rtPopup);
}

void CFWL_WidgetMgr::OnSetCapability(uint32_t dwCapability) {
  m_dwCapability = dwCapability;
}

void CFWL_WidgetMgr::OnProcessMessageToForm(CFWL_Message* pMessage) {
  if (!pMessage)
    return;
  if (!pMessage->m_pDstTarget)
    return;

  IFWL_Widget* pDstWidget = pMessage->m_pDstTarget;
  const IFWL_App* pApp = pDstWidget->GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pNoteDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pNoteDriver)
    return;

  std::unique_ptr<CFWL_Message> pClonedMessage = pMessage->Clone();
  if (IsFormDisabled())
    pNoteDriver->ProcessMessage(pClonedMessage.get());
  else
    pNoteDriver->QueueMessage(std::move(pClonedMessage));

#if (_FX_OS_ == _FX_MACOSX_)
  CFWL_NoteLoop* pTopLoop = pNoteDriver->GetTopLoop();
  if (pTopLoop)
    pNoteDriver->UnqueueMessage(pTopLoop);
#endif
}

void CFWL_WidgetMgr::OnDrawWidget(IFWL_Widget* pWidget,
                                  CFX_Graphics* pGraphics,
                                  const CFX_Matrix* pMatrix) {
  if (!pWidget || !pGraphics)
    return;

  CFX_Graphics* pTemp = DrawWidgetBefore(pWidget, pGraphics, pMatrix);
  CFX_RectF clipCopy;
  pWidget->GetWidgetRect(clipCopy);
  clipCopy.left = clipCopy.top = 0;

  if (UseOffscreenDirect(pWidget)) {
    DrawWidgetAfter(pWidget, pGraphics, clipCopy, pMatrix);
    return;
  }
  CFX_RectF clipBounds;

#if _FX_OS_ == _FX_WIN32_DESKTOP_ || _FX_OS_ == _FX_WIN64_ || \
    _FX_OS_ == _FX_LINUX_DESKTOP_ || _FX_OS_ == _FX_ANDROID_
  pWidget->GetDelegate()->OnDrawWidget(pTemp, pMatrix);
  pGraphics->GetClipRect(clipBounds);
  clipCopy = clipBounds;
#elif _FX_OS_ == _FX_MACOSX_
  if (IsFormDisabled()) {
    pWidget->GetDelegate()->OnDrawWidget(pTemp, pMatrix);
    pGraphics->GetClipRect(clipBounds);
    clipCopy = clipBounds;
  } else {
    clipBounds.Set(pMatrix->a, pMatrix->b, pMatrix->c, pMatrix->d);
    const_cast<CFX_Matrix*>(pMatrix)->SetIdentity();  // FIXME: const cast.
    pWidget->GetDelegate()->OnDrawWidget(pTemp, pMatrix);
  }
#endif  // _FX_OS_ == _FX_MACOSX_

  if (!IsFormDisabled()) {
    CFX_RectF rtClient;
    pWidget->GetClientRect(rtClient);
    clipBounds.Intersect(rtClient);
  }
  if (!clipBounds.IsEmpty())
    DrawChild(pWidget, clipBounds, pTemp, pMatrix);

  DrawWidgetAfter(pWidget, pGraphics, clipCopy, pMatrix);
  ResetRedrawCounts(pWidget);
}

void CFWL_WidgetMgr::DrawChild(IFWL_Widget* parent,
                               const CFX_RectF& rtClip,
                               CFX_Graphics* pGraphics,
                               const CFX_Matrix* pMatrix) {
  if (!parent)
    return;

  bool bFormDisable = IsFormDisabled();
  IFWL_Widget* pNextChild = GetFirstChildWidget(parent);
  while (pNextChild) {
    IFWL_Widget* child = pNextChild;
    pNextChild = GetNextSiblingWidget(child);
    if (child->GetStates() & FWL_WGTSTATE_Invisible)
      continue;

    CFX_RectF rtWidget;
    child->GetWidgetRect(rtWidget);
    if (rtWidget.IsEmpty())
      continue;

    CFX_Matrix widgetMatrix;
    CFX_RectF clipBounds(rtWidget);
    if (!bFormDisable)
      child->GetMatrix(widgetMatrix, true);
    if (pMatrix)
      widgetMatrix.Concat(*pMatrix);

    if (!bFormDisable) {
      widgetMatrix.TransformPoint(clipBounds.left, clipBounds.top);
      clipBounds.Intersect(rtClip);
      if (clipBounds.IsEmpty())
        continue;

      pGraphics->SaveGraphState();
      pGraphics->SetClipRect(clipBounds);
    }
    widgetMatrix.Translate(rtWidget.left, rtWidget.top, true);

    if (IFWL_WidgetDelegate* pDelegate = child->GetDelegate()) {
      if (IsFormDisabled() || IsNeedRepaint(child, &widgetMatrix, rtClip))
        pDelegate->OnDrawWidget(pGraphics, &widgetMatrix);
    }
    if (!bFormDisable)
      pGraphics->RestoreGraphState();

    DrawChild(child, clipBounds, pGraphics,
              bFormDisable ? &widgetMatrix : pMatrix);
    child = GetNextSiblingWidget(child);
  }
}

CFX_Graphics* CFWL_WidgetMgr::DrawWidgetBefore(IFWL_Widget* pWidget,
                                               CFX_Graphics* pGraphics,
                                               const CFX_Matrix* pMatrix) {
  if (!FWL_UseOffscreen(pWidget))
    return pGraphics;

  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!pItem->pOffscreen) {
    pItem->pOffscreen.reset(new CFX_Graphics);
    CFX_RectF rect;
    pWidget->GetWidgetRect(rect);
    pItem->pOffscreen->Create((int32_t)rect.width, (int32_t)rect.height,
                              FXDIB_Argb);
  }
  CFX_RectF rect;
  pGraphics->GetClipRect(rect);
  pItem->pOffscreen->SetClipRect(rect);
  return pItem->pOffscreen.get();
}

void CFWL_WidgetMgr::DrawWidgetAfter(IFWL_Widget* pWidget,
                                     CFX_Graphics* pGraphics,
                                     CFX_RectF& rtClip,
                                     const CFX_Matrix* pMatrix) {
  if (FWL_UseOffscreen(pWidget)) {
    CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
    pGraphics->Transfer(pItem->pOffscreen.get(), rtClip.left, rtClip.top,
                        rtClip, pMatrix);
#ifdef _WIN32
    pItem->pOffscreen->ClearClip();
#endif
  }
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  pItem->iRedrawCounter = 0;
}

bool CFWL_WidgetMgr::IsNeedRepaint(IFWL_Widget* pWidget,
                                   CFX_Matrix* pMatrix,
                                   const CFX_RectF& rtDirty) {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (pItem && pItem->iRedrawCounter > 0) {
    pItem->iRedrawCounter = 0;
    return true;
  }

  CFX_RectF rtWidget;
  pWidget->GetWidgetRect(rtWidget);
  rtWidget.left = rtWidget.top = 0;
  pMatrix->TransformRect(rtWidget);
  if (!rtWidget.IntersectWith(rtDirty))
    return false;

  IFWL_Widget* pChild =
      pWidget->GetOwnerApp()->GetWidgetMgr()->GetFirstChildWidget(pWidget);
  if (!pChild)
    return true;

  CFX_RectF rtChilds;
  rtChilds.Empty();
  bool bChildIntersectWithDirty = false;
  bool bOrginPtIntersectWidthChild = false;
  bool bOrginPtIntersectWidthDirty =
      rtDirty.Contains(rtWidget.left, rtWidget.top);
  static FWL_NEEDREPAINTHITDATA hitPoint[kNeedRepaintHitPoints];
  FXSYS_memset(hitPoint, 0, sizeof(hitPoint));
  FX_FLOAT fxPiece = rtWidget.width / kNeedRepaintHitPiece;
  FX_FLOAT fyPiece = rtWidget.height / kNeedRepaintHitPiece;
  hitPoint[2].hitPoint.x = hitPoint[6].hitPoint.x = rtWidget.left;
  hitPoint[0].hitPoint.x = hitPoint[3].hitPoint.x = hitPoint[7].hitPoint.x =
      hitPoint[10].hitPoint.x = fxPiece + rtWidget.left;
  hitPoint[1].hitPoint.x = hitPoint[4].hitPoint.x = hitPoint[8].hitPoint.x =
      hitPoint[11].hitPoint.x = fxPiece * 2 + rtWidget.left;
  hitPoint[5].hitPoint.x = hitPoint[9].hitPoint.x =
      rtWidget.width + rtWidget.left;
  hitPoint[0].hitPoint.y = hitPoint[1].hitPoint.y = rtWidget.top;
  hitPoint[2].hitPoint.y = hitPoint[3].hitPoint.y = hitPoint[4].hitPoint.y =
      hitPoint[5].hitPoint.y = fyPiece + rtWidget.top;
  hitPoint[6].hitPoint.y = hitPoint[7].hitPoint.y = hitPoint[8].hitPoint.y =
      hitPoint[9].hitPoint.y = fyPiece * 2 + rtWidget.top;
  hitPoint[10].hitPoint.y = hitPoint[11].hitPoint.y =
      rtWidget.height + rtWidget.top;
  do {
    CFX_RectF rect;
    pChild->GetWidgetRect(rect);
    CFX_RectF r = rect;
    r.left += rtWidget.left;
    r.top += rtWidget.top;
    if (r.IsEmpty())
      continue;
    if (r.Contains(rtDirty))
      return false;
    if (!bChildIntersectWithDirty && r.IntersectWith(rtDirty))
      bChildIntersectWithDirty = true;
    if (bOrginPtIntersectWidthDirty && !bOrginPtIntersectWidthChild)
      bOrginPtIntersectWidthChild = rect.Contains(0, 0);

    if (rtChilds.IsEmpty())
      rtChilds = rect;
    else if (!(pChild->GetStates() & FWL_WGTSTATE_Invisible))
      rtChilds.Union(rect);

    for (int32_t i = 0; i < kNeedRepaintHitPoints; i++) {
      if (hitPoint[i].bNotContainByDirty || hitPoint[i].bNotNeedRepaint)
        continue;
      if (!rtDirty.Contains(hitPoint[i].hitPoint)) {
        hitPoint[i].bNotContainByDirty = true;
        continue;
      }
      if (r.Contains(hitPoint[i].hitPoint))
        hitPoint[i].bNotNeedRepaint = true;
    }
    pChild =
        pChild->GetOwnerApp()->GetWidgetMgr()->GetNextSiblingWidget(pChild);
  } while (pChild);

  if (!bChildIntersectWithDirty)
    return true;
  if (bOrginPtIntersectWidthDirty && !bOrginPtIntersectWidthChild)
    return true;
  if (rtChilds.IsEmpty())
    return true;

  int32_t repaintPoint = kNeedRepaintHitPoints;
  for (int32_t i = 0; i < kNeedRepaintHitPoints; i++) {
    if (hitPoint[i].bNotNeedRepaint)
      repaintPoint--;
  }
  if (repaintPoint > 0)
    return true;

  pMatrix->TransformRect(rtChilds);
  if (rtChilds.Contains(rtDirty) || rtChilds.Contains(rtWidget))
    return false;
  return true;
}

bool CFWL_WidgetMgr::UseOffscreenDirect(IFWL_Widget* pWidget) const {
  CFWL_WidgetMgrItem* pItem = GetWidgetMgrItem(pWidget);
  if (!FWL_UseOffscreen(pWidget) || !(pItem->pOffscreen))
    return false;

#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
  if (pItem->bOutsideChanged) {
    CFX_RectF r;
    pWidget->GetWidgetRect(r);
    CFX_RectF temp(m_rtScreen);
    temp.Deflate(50, 50);
    if (!temp.Contains(r))
      return false;

    pItem->bOutsideChanged = false;
  }
#endif

  return pItem->iRedrawCounter == 0;
}

CFWL_WidgetMgrItem::CFWL_WidgetMgrItem() : CFWL_WidgetMgrItem(nullptr) {}

CFWL_WidgetMgrItem::CFWL_WidgetMgrItem(IFWL_Widget* widget)
    : pParent(nullptr),
      pOwner(nullptr),
      pChild(nullptr),
      pPrevious(nullptr),
      pNext(nullptr),
      pWidget(widget),
      iRedrawCounter(0)
#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
      ,
      bOutsideChanged(false)
#endif
{
}

CFWL_WidgetMgrItem::~CFWL_WidgetMgrItem() {}
