// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_WIDGETMGR_H_
#define XFA_FWL_CORE_CFWL_WIDGETMGR_H_

#include <map>
#include <memory>

#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/fwl_error.h"
#include "xfa/fxgraphics/cfx_graphics.h"

#define FWL_WGTMGR_DisableForm 0x00000002

class CFWL_Message;
class CXFA_FFApp;
class CXFA_FWLAdapterWidgetMgr;
class CFX_Graphics;
class CFX_Matrix;
class IFWL_Widget;

class CFWL_WidgetMgrItem {
 public:
  CFWL_WidgetMgrItem();
  explicit CFWL_WidgetMgrItem(IFWL_Widget* widget);
  ~CFWL_WidgetMgrItem();

  CFWL_WidgetMgrItem* pParent;
  CFWL_WidgetMgrItem* pOwner;
  CFWL_WidgetMgrItem* pChild;
  CFWL_WidgetMgrItem* pPrevious;
  CFWL_WidgetMgrItem* pNext;
  IFWL_Widget* const pWidget;
  std::unique_ptr<CFX_Graphics> pOffscreen;
  int32_t iRedrawCounter;
#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
  bool bOutsideChanged;
#endif
};

class IFWL_WidgetMgrDelegate {
 public:
  virtual void OnSetCapability(uint32_t dwCapability) = 0;
  virtual void OnProcessMessageToForm(CFWL_Message* pMessage) = 0;
  virtual void OnDrawWidget(IFWL_Widget* pWidget,
                            CFX_Graphics* pGraphics,
                            const CFX_Matrix* pMatrix) = 0;
};

class CFWL_WidgetMgr : public IFWL_WidgetMgrDelegate {
 public:
  explicit CFWL_WidgetMgr(CXFA_FFApp* pAdapterNative);
  ~CFWL_WidgetMgr();

  // IFWL_WidgetMgrDelegate
  void OnSetCapability(uint32_t dwCapability) override;
  void OnProcessMessageToForm(CFWL_Message* pMessage) override;
  void OnDrawWidget(IFWL_Widget* pWidget,
                    CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  IFWL_Widget* GetParentWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetOwnerWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetNextSiblingWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetFirstChildWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetSystemFormWidget(IFWL_Widget* pWidget) const;

  void RepaintWidget(IFWL_Widget* pWidget, const CFX_RectF* pRect = nullptr);

  void InsertWidget(IFWL_Widget* pParent,
                    IFWL_Widget* pChild,
                    int32_t nIndex = -1);
  void RemoveWidget(IFWL_Widget* pWidget);
  void SetOwner(IFWL_Widget* pOwner, IFWL_Widget* pOwned);
  void SetParent(IFWL_Widget* pParent, IFWL_Widget* pChild);

  void SetWidgetRect_Native(IFWL_Widget* pWidget, const CFX_RectF& rect);

  IFWL_Widget* GetWidgetAtPoint(IFWL_Widget* pParent, FX_FLOAT fx, FX_FLOAT fy);

  void NotifySizeChanged(IFWL_Widget* pForm, FX_FLOAT fx, FX_FLOAT fy);

  IFWL_Widget* NextTab(IFWL_Widget* parent, IFWL_Widget* focus, bool& bFind);

  void GetSameGroupRadioButton(IFWL_Widget* pRadioButton,
                               CFX_ArrayTemplate<IFWL_Widget*>& group) const;
  IFWL_Widget* GetDefaultButton(IFWL_Widget* pParent) const;
  void AddRedrawCounts(IFWL_Widget* pWidget);

  bool IsFormDisabled() const {
    return !!(m_dwCapability & FWL_WGTMGR_DisableForm);
  }

  void GetAdapterPopupPos(IFWL_Widget* pWidget,
                          FX_FLOAT fMinHeight,
                          FX_FLOAT fMaxHeight,
                          const CFX_RectF& rtAnchor,
                          CFX_RectF& rtPopup) const;

 private:
  IFWL_Widget* GetFirstSiblingWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetPriorSiblingWidget(IFWL_Widget* pWidget) const;
  IFWL_Widget* GetLastChildWidget(IFWL_Widget* pWidget) const;
  CFWL_WidgetMgrItem* GetWidgetMgrItem(IFWL_Widget* pWidget) const;

  void SetWidgetIndex(IFWL_Widget* pWidget, int32_t nIndex);

  int32_t CountRadioButtonGroup(IFWL_Widget* pFirst) const;
  IFWL_Widget* GetRadioButtonGroupHeader(IFWL_Widget* pRadioButton) const;

  void ResetRedrawCounts(IFWL_Widget* pWidget);

  void DrawChild(IFWL_Widget* pParent,
                 const CFX_RectF& rtClip,
                 CFX_Graphics* pGraphics,
                 const CFX_Matrix* pMatrix);
  CFX_Graphics* DrawWidgetBefore(IFWL_Widget* pWidget,
                                 CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix);
  void DrawWidgetAfter(IFWL_Widget* pWidget,
                       CFX_Graphics* pGraphics,
                       CFX_RectF& rtClip,
                       const CFX_Matrix* pMatrix);
  bool IsNeedRepaint(IFWL_Widget* pWidget,
                     CFX_Matrix* pMatrix,
                     const CFX_RectF& rtDirty);
  bool UseOffscreenDirect(IFWL_Widget* pWidget) const;

  bool IsAbleNative(IFWL_Widget* pWidget) const;

  uint32_t m_dwCapability;
  std::map<IFWL_Widget*, std::unique_ptr<CFWL_WidgetMgrItem>> m_mapWidgetItem;
  CXFA_FWLAdapterWidgetMgr* const m_pAdapter;
#if (_FX_OS_ == _FX_WIN32_DESKTOP_) || (_FX_OS_ == _FX_WIN64_)
  CFX_RectF m_rtScreen;
#endif
};

#endif  // XFA_FWL_CORE_CFWL_WIDGETMGR_H_
