// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_layoutprocessor.h"

#include "xfa/fxfa/parser/cxfa_contentlayoutitem.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_layoutpagemgr.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/xfa_document_datamerger_imp.h"
#include "xfa/fxfa/parser/xfa_layout_itemlayout.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CXFA_LayoutProcessor* CXFA_Document::GetLayoutProcessor() {
  if (!m_pLayoutProcessor) {
    m_pLayoutProcessor = new CXFA_LayoutProcessor(this);
    ASSERT(m_pLayoutProcessor);
  }
  return m_pLayoutProcessor;
}

CXFA_LayoutProcessor* CXFA_Document::GetDocLayout() {
  return GetLayoutProcessor();
}

CXFA_LayoutProcessor::CXFA_LayoutProcessor(CXFA_Document* pDocument)
    : m_pDocument(pDocument),
      m_pRootItemLayoutProcessor(nullptr),
      m_pLayoutPageMgr(nullptr),
      m_nProgressCounter(0),
      m_bNeeLayout(true) {}

CXFA_LayoutProcessor::~CXFA_LayoutProcessor() {
  ClearLayoutData();
}

CXFA_Document* CXFA_LayoutProcessor::GetDocument() const {
  return m_pDocument;
}

int32_t CXFA_LayoutProcessor::StartLayout(bool bForceRestart) {
  if (!bForceRestart && !IsNeedLayout())
    return 100;

  delete m_pRootItemLayoutProcessor;
  m_pRootItemLayoutProcessor = nullptr;
  m_nProgressCounter = 0;
  CXFA_Node* pFormPacketNode =
      ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Form));
  if (!pFormPacketNode)
    return -1;

  CXFA_Node* pFormRoot =
      pFormPacketNode->GetFirstChildByClass(XFA_Element::Subform);
  if (!pFormRoot)
    return -1;
  if (!m_pLayoutPageMgr)
    m_pLayoutPageMgr = new CXFA_LayoutPageMgr(this);
  if (!m_pLayoutPageMgr->InitLayoutPage(pFormRoot))
    return -1;
  if (!m_pLayoutPageMgr->PrepareFirstPage(pFormRoot))
    return -1;
  m_pRootItemLayoutProcessor =
      new CXFA_ItemLayoutProcessor(pFormRoot, m_pLayoutPageMgr);
  m_nProgressCounter = 1;
  return 0;
}

int32_t CXFA_LayoutProcessor::DoLayout(IFX_Pause* pPause) {
  if (m_nProgressCounter < 1)
    return -1;

  XFA_ItemLayoutProcessorResult eStatus;
  CXFA_Node* pFormNode = m_pRootItemLayoutProcessor->GetFormNode();
  FX_FLOAT fPosX = pFormNode->GetMeasure(XFA_ATTRIBUTE_X).ToUnit(XFA_UNIT_Pt);
  FX_FLOAT fPosY = pFormNode->GetMeasure(XFA_ATTRIBUTE_Y).ToUnit(XFA_UNIT_Pt);
  do {
    FX_FLOAT fAvailHeight = m_pLayoutPageMgr->GetAvailHeight();
    eStatus =
        m_pRootItemLayoutProcessor->DoLayout(true, fAvailHeight, fAvailHeight);
    if (eStatus != XFA_ItemLayoutProcessorResult_Done)
      m_nProgressCounter++;

    CXFA_ContentLayoutItem* pLayoutItem =
        m_pRootItemLayoutProcessor->ExtractLayoutItem();
    if (pLayoutItem)
      pLayoutItem->m_sPos = CFX_PointF(fPosX, fPosY);

    m_pLayoutPageMgr->SubmitContentItem(pLayoutItem, eStatus);
  } while (eStatus != XFA_ItemLayoutProcessorResult_Done &&
           (!pPause || !pPause->NeedToPauseNow()));

  if (eStatus == XFA_ItemLayoutProcessorResult_Done) {
    m_pLayoutPageMgr->FinishPaginatedPageSets();
    m_pLayoutPageMgr->SyncLayoutData();
    m_bNeeLayout = false;
    m_rgChangedContainers.RemoveAll();
  }
  return 100 * (eStatus == XFA_ItemLayoutProcessorResult_Done
                    ? m_nProgressCounter
                    : m_nProgressCounter - 1) /
         m_nProgressCounter;
}

bool CXFA_LayoutProcessor::IncrementLayout() {
  if (m_bNeeLayout) {
    StartLayout(true);
    return DoLayout(nullptr) == 100;
  }

  for (int32_t i = 0, c = m_rgChangedContainers.GetSize(); i < c; i++) {
    CXFA_Node* pNode = m_rgChangedContainers[i];
    CXFA_Node* pParentNode =
        pNode->GetNodeItem(XFA_NODEITEM_Parent, XFA_ObjectType::ContainerNode);
    if (!pParentNode)
      return false;
    if (!CXFA_ItemLayoutProcessor::IncrementRelayoutNode(this, pNode,
                                                         pParentNode)) {
      return false;
    }
  }
  m_rgChangedContainers.RemoveAll();
  return true;
}

int32_t CXFA_LayoutProcessor::CountPages() const {
  return m_pLayoutPageMgr ? m_pLayoutPageMgr->GetPageCount() : 0;
}

CXFA_ContainerLayoutItem* CXFA_LayoutProcessor::GetPage(int32_t index) const {
  return m_pLayoutPageMgr ? m_pLayoutPageMgr->GetPage(index) : nullptr;
}

CXFA_LayoutItem* CXFA_LayoutProcessor::GetLayoutItem(CXFA_Node* pFormItem) {
  return static_cast<CXFA_LayoutItem*>(
      pFormItem->GetUserData(XFA_LAYOUTITEMKEY));
}

void CXFA_LayoutProcessor::AddChangedContainer(CXFA_Node* pContainer) {
  if (m_rgChangedContainers.Find(pContainer) < 0)
    m_rgChangedContainers.Add(pContainer);
}

CXFA_ContainerLayoutItem* CXFA_LayoutProcessor::GetRootLayoutItem() const {
  return m_pLayoutPageMgr ? m_pLayoutPageMgr->GetRootLayoutItem() : nullptr;
}

void CXFA_LayoutProcessor::ClearLayoutData() {
  delete m_pLayoutPageMgr;
  m_pLayoutPageMgr = nullptr;
  delete m_pRootItemLayoutProcessor;
  m_pRootItemLayoutProcessor = nullptr;
  m_nProgressCounter = 0;
}

bool CXFA_LayoutProcessor::IsNeedLayout() {
  return m_bNeeLayout || m_rgChangedContainers.GetSize() > 0;
}
