// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_LAYOUTPROCESSOR_H_
#define XFA_FXFA_PARSER_CXFA_LAYOUTPROCESSOR_H_

#include "core/fxcrt/fx_system.h"
#include "xfa/fxfa/parser/xfa_object.h"

class CXFA_ContainerLayoutItem;
class CXFA_Document;
class CXFA_ItemLayoutProcessor;
class CXFA_LayoutItem;
class CXFA_LayoutPageMgr;
class CXFA_Node;
class IFX_Pause;

class CXFA_LayoutProcessor {
 public:
  CXFA_LayoutProcessor(CXFA_Document* pDocument);
  ~CXFA_LayoutProcessor();

  CXFA_Document* GetDocument() const;
  int32_t StartLayout(bool bForceRestart = false);
  int32_t DoLayout(IFX_Pause* pPause = nullptr);
  bool IncrementLayout();
  int32_t CountPages() const;
  CXFA_ContainerLayoutItem* GetPage(int32_t index) const;
  CXFA_LayoutItem* GetLayoutItem(CXFA_Node* pFormItem);

  void AddChangedContainer(CXFA_Node* pContainer);
  void SetForceReLayout(bool bForceRestart) { m_bNeeLayout = bForceRestart; }
  CXFA_ContainerLayoutItem* GetRootLayoutItem() const;
  CXFA_ItemLayoutProcessor* GetRootRootItemLayoutProcessor() {
    return m_pRootItemLayoutProcessor;
  }
  CXFA_LayoutPageMgr* GetLayoutPageMgr() { return m_pLayoutPageMgr; }

 private:
  void ClearLayoutData();

  bool IsNeedLayout();

  CXFA_Document* m_pDocument;
  CXFA_ItemLayoutProcessor* m_pRootItemLayoutProcessor;
  CXFA_LayoutPageMgr* m_pLayoutPageMgr;
  CXFA_NodeArray m_rgChangedContainers;
  uint32_t m_nProgressCounter;
  bool m_bNeeLayout;
};

#endif  // XFA_FXFA_PARSER_CXFA_LAYOUTPROCESSOR_H_
