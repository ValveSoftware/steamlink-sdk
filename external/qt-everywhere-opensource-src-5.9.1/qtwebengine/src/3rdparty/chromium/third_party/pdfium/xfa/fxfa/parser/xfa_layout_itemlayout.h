// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_LAYOUT_ITEMLAYOUT_H_
#define XFA_FXFA_PARSER_XFA_LAYOUT_ITEMLAYOUT_H_

#include <float.h>

#include <list>
#include <map>

#include "core/fxcrt/fx_basic.h"
#include "xfa/fxfa/fxfa_basic.h"

#define XFA_LAYOUT_INVALIDNODE ((CXFA_Node*)(intptr_t)-1)
#define XFA_LAYOUT_FLOAT_PERCISION (0.0005f)
#define XFA_LAYOUT_FLOAT_MAX FLT_MAX

class CXFA_ContainerLayoutItem;
class CXFA_ContentLayoutItem;
class CXFA_ItemLayoutProcessor;
class CXFA_LayoutPageMgr;
class CXFA_LayoutProcessor;
class CXFA_Node;

enum XFA_ItemLayoutProcessorResult {
  XFA_ItemLayoutProcessorResult_Done,
  XFA_ItemLayoutProcessorResult_PageFullBreak,
  XFA_ItemLayoutProcessorResult_RowFullBreak,
  XFA_ItemLayoutProcessorResult_ManualBreak,
};

enum XFA_ItemLayoutProcessorStages {
  XFA_ItemLayoutProcessorStages_None,
  XFA_ItemLayoutProcessorStages_BookendLeader,
  XFA_ItemLayoutProcessorStages_BreakBefore,
  XFA_ItemLayoutProcessorStages_Keep,
  XFA_ItemLayoutProcessorStages_Container,
  XFA_ItemLayoutProcessorStages_BreakAfter,
  XFA_ItemLayoutProcessorStages_BookendTrailer,
  XFA_ItemLayoutProcessorStages_Done,
};

class CXFA_LayoutContext {
 public:
  CXFA_LayoutContext()
      : m_prgSpecifiedColumnWidths(nullptr),
        m_fCurColumnWidth(0),
        m_bCurColumnWidthAvaiable(false),
        m_pOverflowProcessor(nullptr),
        m_pOverflowNode(nullptr) {}
  ~CXFA_LayoutContext() { m_pOverflowProcessor = nullptr; }
  CFX_ArrayTemplate<FX_FLOAT>* m_prgSpecifiedColumnWidths;
  FX_FLOAT m_fCurColumnWidth;
  bool m_bCurColumnWidthAvaiable;
  CXFA_ItemLayoutProcessor* m_pOverflowProcessor;
  CXFA_Node* m_pOverflowNode;
};

class CXFA_ItemLayoutProcessor {
 public:
  CXFA_ItemLayoutProcessor(CXFA_Node* pNode, CXFA_LayoutPageMgr* pPageMgr);
  ~CXFA_ItemLayoutProcessor();

  XFA_ItemLayoutProcessorResult DoLayout(
      bool bUseBreakControl,
      FX_FLOAT fHeightLimit,
      FX_FLOAT fRealHeight = XFA_LAYOUT_FLOAT_MAX,
      CXFA_LayoutContext* pContext = nullptr);

  void GetCurrentComponentPos(FX_FLOAT& fAbsoluteX, FX_FLOAT& fAbsoluteY);

  void GetCurrentComponentSize(FX_FLOAT& fWidth, FX_FLOAT& fHeight);

  void SetCurrentComponentPos(FX_FLOAT fAbsoluteX, FX_FLOAT fAbsoluteY);

  void SetCurrentComponentSize(FX_FLOAT fWidth, FX_FLOAT fHeight);
  CXFA_Node* GetFormNode() { return m_pFormNode; }
  bool HasLayoutItem() { return !!m_pLayoutItem; }
  CXFA_ContentLayoutItem* ExtractLayoutItem();

  static bool IncrementRelayoutNode(CXFA_LayoutProcessor* pLayoutProcessor,
                                    CXFA_Node* pNode,
                                    CXFA_Node* pParentNode);
  static void CalculatePositionedContainerPos(CXFA_Node* pNode,
                                              FX_FLOAT fWidth,
                                              FX_FLOAT fHeight,
                                              FX_FLOAT& fAbsoluteX,
                                              FX_FLOAT& fAbsoluteY);
  static bool FindLayoutItemSplitPos(CXFA_ContentLayoutItem* pLayoutItem,
                                     FX_FLOAT fCurVerticalOffset,
                                     FX_FLOAT& fProposedSplitPos,
                                     bool& bAppChange,
                                     bool bCalculateMargin = true);
  FX_FLOAT FindSplitPos(FX_FLOAT fProposedSplitPos);
  void SplitLayoutItem(CXFA_ContentLayoutItem* pLayoutItem,
                       CXFA_ContentLayoutItem* pSecondParent,
                       FX_FLOAT fSplitPos);
  void SplitLayoutItem(FX_FLOAT fSplitPos);
  bool JudgePutNextPage(CXFA_ContentLayoutItem* pParentLayoutItem,
                        FX_FLOAT fChildHeight,
                        CFX_ArrayTemplate<CXFA_ContentLayoutItem*>& pKeepItems);
  bool ProcessKeepForSplite(
      CXFA_ItemLayoutProcessor* pParentProcessor,
      CXFA_ItemLayoutProcessor* pChildProcessor,
      XFA_ItemLayoutProcessorResult eRetValue,
      CFX_ArrayTemplate<CXFA_ContentLayoutItem*>& rgCurLineLayoutItem,
      FX_FLOAT& fContentCurRowAvailWidth,
      FX_FLOAT& fContentCurRowHeight,
      FX_FLOAT& fContentCurRowY,
      bool& bAddedItemInRow,
      bool& bForceEndPage,
      XFA_ItemLayoutProcessorResult& result);
  FX_FLOAT InsertKeepLayoutItems();
  void DoLayoutPageArea(CXFA_ContainerLayoutItem* pPageAreaLayoutItem);
  bool CalculateRowChildPosition(
      CFX_ArrayTemplate<CXFA_ContentLayoutItem*> (&rgCurLineLayoutItems)[3],
      XFA_ATTRIBUTEENUM eFlowStrategy,
      bool bContainerHeightAutoSize,
      bool bContainerWidthAutoSize,
      FX_FLOAT& fContentCalculatedWidth,
      FX_FLOAT& fContentCalculatedHeight,
      FX_FLOAT& fContentCurRowY,
      FX_FLOAT fContentCurRowHeight,
      FX_FLOAT fContentWidthLimit,
      bool bRootForceTb = false);

  void ProcessUnUseOverFlow(CXFA_Node* pLeaderNode,
                            CXFA_Node* pTrailerNode,
                            CXFA_ContentLayoutItem* pTrailerItem,
                            CXFA_Node* pFormNode);
  void ProcessUnUseBinds(CXFA_Node* pFormNode);
  bool IsAddNewRowForTrailer(CXFA_ContentLayoutItem* pTrailerItem);
  bool JudgeLeaderOrTrailerForOccur(CXFA_Node* pFormNode);
  CXFA_ContentLayoutItem* CreateContentLayoutItem(CXFA_Node* pFormNode);

 protected:
  void DoLayoutPositionedContainer(CXFA_LayoutContext* pContext = nullptr);
  void DoLayoutTableContainer(CXFA_Node* pLayoutNode);
  XFA_ItemLayoutProcessorResult DoLayoutFlowedContainer(
      bool bUseBreakControl,
      XFA_ATTRIBUTEENUM eFlowStrategy,
      FX_FLOAT fHeightLimit,
      FX_FLOAT fRealHeight,
      CXFA_LayoutContext* pContext = nullptr,
      bool bRootForceTb = false);
  void DoLayoutField();
  void XFA_ItemLayoutProcessor_GotoNextContainerNode(
      CXFA_Node*& pCurActionNode,
      XFA_ItemLayoutProcessorStages& nCurStage,
      CXFA_Node* pParentContainer,
      bool bUsePageBreak);

  bool ProcessKeepNodesForCheckNext(CXFA_Node*& pCurActionNode,
                                    XFA_ItemLayoutProcessorStages& nCurStage,
                                    CXFA_Node*& pNextContainer,
                                    bool& bLastKeepNode);

  bool ProcessKeepNodesForBreakBefore(CXFA_Node*& pCurActionNode,
                                      XFA_ItemLayoutProcessorStages& nCurStage,
                                      CXFA_Node* pContainerNode);

  CXFA_Node* GetSubformSetParent(CXFA_Node* pSubformSet);

 public:
  bool m_bKeepBreakFinish;
  bool m_bIsProcessKeep;
  CXFA_Node* m_pKeepHeadNode;
  CXFA_Node* m_pKeepTailNode;
  CXFA_Node* m_pFormNode;
  CXFA_ContentLayoutItem* m_pLayoutItem;
  CXFA_ContentLayoutItem* m_pOldLayoutItem;
  CXFA_Node* m_pCurChildNode;
  CXFA_ItemLayoutProcessor* m_pCurChildPreprocessor;
  XFA_ItemLayoutProcessorStages m_nCurChildNodeStage;
  FX_FLOAT m_fUsedSize;
  CXFA_LayoutPageMgr* m_pPageMgr;
  std::list<CXFA_Node*> m_PendingNodes;
  bool m_bBreakPending;
  CFX_ArrayTemplate<FX_FLOAT> m_rgSpecifiedColumnWidths;
  CFX_ArrayTemplate<CXFA_ContentLayoutItem*> m_arrayKeepItems;
  std::map<CXFA_Node*, int32_t> m_PendingNodesCount;
  FX_FLOAT m_fLastRowWidth;
  FX_FLOAT m_fLastRowY;
  FX_FLOAT m_fWidthLimite;
  bool m_bUseInheriated;
  XFA_ItemLayoutProcessorResult m_ePreProcessRs;
  bool m_bHasAvailHeight;
};
bool XFA_ItemLayoutProcessor_IsTakingSpace(CXFA_Node* pNode);

#endif  // XFA_FXFA_PARSER_XFA_LAYOUT_ITEMLAYOUT_H_
