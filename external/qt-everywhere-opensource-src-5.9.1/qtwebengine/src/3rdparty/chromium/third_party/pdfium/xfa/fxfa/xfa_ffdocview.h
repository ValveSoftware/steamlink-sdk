// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_XFA_FFDOCVIEW_H_
#define XFA_FXFA_XFA_FFDOCVIEW_H_

#include <map>
#include <memory>

#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/xfa_ffdoc.h"

class CXFA_FFWidgetHandler;
class CXFA_FFDoc;
class CXFA_FFWidget;
class CXFA_WidgetAccIterator;

extern const XFA_ATTRIBUTEENUM gs_EventActivity[];
enum XFA_DOCVIEW_LAYOUTSTATUS {
  XFA_DOCVIEW_LAYOUTSTATUS_None,
  XFA_DOCVIEW_LAYOUTSTATUS_Start,
  XFA_DOCVIEW_LAYOUTSTATUS_FormInitialize,
  XFA_DOCVIEW_LAYOUTSTATUS_FormInitCalculate,
  XFA_DOCVIEW_LAYOUTSTATUS_FormInitValidate,
  XFA_DOCVIEW_LAYOUTSTATUS_FormFormReady,
  XFA_DOCVIEW_LAYOUTSTATUS_Doing,
  XFA_DOCVIEW_LAYOUTSTATUS_PagesetInitialize,
  XFA_DOCVIEW_LAYOUTSTATUS_PagesetInitCalculate,
  XFA_DOCVIEW_LAYOUTSTATUS_PagesetInitValidate,
  XFA_DOCVIEW_LAYOUTSTATUS_PagesetFormReady,
  XFA_DOCVIEW_LAYOUTSTATUS_LayoutReady,
  XFA_DOCVIEW_LAYOUTSTATUS_DocReady,
  XFA_DOCVIEW_LAYOUTSTATUS_End
};
class CXFA_FFDocView {
 public:
  CXFA_FFDocView(CXFA_FFDoc* pDoc);
  ~CXFA_FFDocView();

  CXFA_FFDoc* GetDoc() { return m_pDoc; }
  int32_t StartLayout(int32_t iStartPage = 0);
  int32_t DoLayout(IFX_Pause* pPause);
  void StopLayout();
  int32_t GetLayoutStatus();
  void UpdateDocView();
  int32_t CountPageViews();
  CXFA_FFPageView* GetPageView(int32_t nIndex);

  void ResetWidgetData(CXFA_WidgetAcc* pWidgetAcc);
  int32_t ProcessWidgetEvent(CXFA_EventParam* pParam,
                             CXFA_WidgetAcc* pWidgetAcc);
  CXFA_FFWidgetHandler* GetWidgetHandler();
  CXFA_WidgetAccIterator* CreateWidgetAccIterator(
      XFA_WIDGETORDER eOrder = XFA_WIDGETORDER_PreOrder);
  CXFA_FFWidget* GetFocusWidget();
  void KillFocus();
  bool SetFocus(CXFA_FFWidget* hWidget);
  CXFA_FFWidget* GetWidgetByName(const CFX_WideString& wsName,
                                 CXFA_FFWidget* pRefWidget);
  CXFA_WidgetAcc* GetWidgetAccByName(const CFX_WideString& wsName,
                                     CXFA_WidgetAcc* pRefWidgetAcc);
  CXFA_LayoutProcessor* GetXFALayout() const;
  void OnPageEvent(CXFA_ContainerLayoutItem* pSender, uint32_t dwEvent);
  void LockUpdate();
  void UnlockUpdate();
  bool IsUpdateLocked();
  void ClearInvalidateList();
  void AddInvalidateRect(CXFA_FFWidget* pWidget, const CFX_RectF& rtInvalidate);
  void AddInvalidateRect(CXFA_FFPageView* pPageView,
                         const CFX_RectF& rtInvalidate);
  void RunInvalidate();
  void RunDocClose();
  void DestroyDocView();

  bool InitValidate(CXFA_Node* pNode);
  bool RunValidate();

  void SetChangeMark();

  void AddValidateWidget(CXFA_WidgetAcc* pWidget);
  void AddCalculateNodeNotify(CXFA_Node* pNodeChange);
  void AddCalculateWidgetAcc(CXFA_WidgetAcc* pWidgetAcc);
  int32_t RunCalculateWidgets();
  bool IsStaticNotify();
  bool RunLayout();
  void RunSubformIndexChange();
  void AddNewFormNode(CXFA_Node* pNode);
  void AddIndexChangedSubform(CXFA_Node* pNode);
  CXFA_WidgetAcc* GetFocusWidgetAcc();
  void SetFocusWidgetAcc(CXFA_WidgetAcc* pWidgetAcc);
  void DeleteLayoutItem(CXFA_FFWidget* pWidget);
  int32_t ExecEventActivityByDeepFirst(CXFA_Node* pFormNode,
                                       XFA_EVENTTYPE eEventType,
                                       bool bIsFormReady,
                                       bool bRecursive,
                                       CXFA_Node* pExclude);
  bool m_bLayoutEvent;
  CFX_WideStringArray m_arrNullTestMsg;
  CXFA_FFWidget* m_pListFocusWidget;
  bool m_bInLayoutStatus;

 protected:
  bool RunEventLayoutReady();
  void RunBindItems();
  bool InitCalculate(CXFA_Node* pNode);
  void InitLayout(CXFA_Node* pNode);
  void RunCalculateRecursive(int32_t& iIndex);
  void ShowNullTestMsg();
  bool ResetSingleWidgetAccData(CXFA_WidgetAcc* pWidgetAcc);
  CXFA_Node* GetRootSubform();

  CXFA_FFDoc* const m_pDoc;
  std::unique_ptr<CXFA_FFWidgetHandler> m_pWidgetHandler;
  CXFA_LayoutProcessor* m_pXFADocLayout;  // not owned.
  CXFA_WidgetAcc* m_pFocusAcc;            // not owned.
  CXFA_FFWidget* m_pFocusWidget;          // not owned.
  CXFA_FFWidget* m_pOldFocusWidget;       // not owned.
  std::map<CXFA_FFPageView*, std::unique_ptr<CFX_RectF>> m_mapPageInvalidate;
  CFX_ArrayTemplate<CXFA_WidgetAcc*> m_ValidateAccs;
  CFX_ArrayTemplate<CXFA_WidgetAcc*> m_CalculateAccs;
  CFX_ArrayTemplate<CXFA_Node*> m_BindItems;
  CFX_ArrayTemplate<CXFA_Node*> m_NewAddedNodes;
  CFX_ArrayTemplate<CXFA_Node*> m_IndexChangedSubforms;
  XFA_DOCVIEW_LAYOUTSTATUS m_iStatus;
  int32_t m_iLock;
  friend class CXFA_FFNotify;
};

class CXFA_WidgetAccIterator {
 public:
  CXFA_WidgetAccIterator(CXFA_FFDocView* pDocView, CXFA_Node* pTravelRoot);
  ~CXFA_WidgetAccIterator();

  void Reset();
  CXFA_WidgetAcc* MoveToFirst();
  CXFA_WidgetAcc* MoveToLast();
  CXFA_WidgetAcc* MoveToNext();
  CXFA_WidgetAcc* MoveToPrevious();
  CXFA_WidgetAcc* GetCurrentWidgetAcc();
  bool SetCurrentWidgetAcc(CXFA_WidgetAcc* hWidget);
  void SkipTree();

 protected:
  CXFA_ContainerIterator m_ContentIterator;
  CXFA_FFDocView* const m_pDocView;
  CXFA_WidgetAcc* m_pCurWidgetAcc;  // not owned.
};

#endif  // XFA_FXFA_XFA_FFDOCVIEW_H_
