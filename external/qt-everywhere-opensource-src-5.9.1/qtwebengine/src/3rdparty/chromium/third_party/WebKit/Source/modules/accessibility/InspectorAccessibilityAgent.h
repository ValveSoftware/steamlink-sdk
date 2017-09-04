// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAccessibilityAgent_h
#define InspectorAccessibilityAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Accessibility.h"
#include "modules/ModulesExport.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class InspectorDOMAgent;
class Page;

using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAccessibilityAgent);

 public:
  InspectorAccessibilityAgent(Page*, InspectorDOMAgent*);

  // Base agent methods.
  DECLARE_VIRTUAL_TRACE();

  // Protocol methods.
  Response getPartialAXTree(
      int domNodeId,
      Maybe<bool> fetchRelatives,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;

 private:
  std::unique_ptr<AXNode> buildObjectForIgnoredNode(
      Node* domNode,
      AXObject*,
      bool fetchRelatives,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void populateDOMNodeRelatives(Node& inspectedDOMNode,
                                AXNode&,
                                std::unique_ptr<protocol::Array<AXNode>>& nodes,
                                AXObjectCacheImpl&) const;
  void findDOMNodeChildren(std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
                           Node& parentNode,
                           Node& inspectedDOMNode,
                           std::unique_ptr<protocol::Array<AXNode>>& nodes,
                           AXObjectCacheImpl&) const;
  std::unique_ptr<AXNode> buildProtocolAXObject(
      AXObject&,
      AXObject* inspectedAXObject,
      bool fetchRelatives,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void fillCoreProperties(AXObject&,
                          AXObject* inspectedAXObject,
                          bool fetchRelatives,
                          AXNode&,
                          std::unique_ptr<protocol::Array<AXNode>>& nodes,
                          AXObjectCacheImpl&) const;
  void addAncestors(AXObject& firstAncestor,
                    AXObject* inspectedAXObject,
                    std::unique_ptr<protocol::Array<AXNode>>& nodes,
                    AXObjectCacheImpl&) const;
  void populateRelatives(AXObject&,
                         AXObject* inspectedAXObject,
                         AXNode&,
                         std::unique_ptr<protocol::Array<AXNode>>& nodes,
                         AXObjectCacheImpl&) const;
  void addSiblingsOfIgnored(
      std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
      AXObject& parentAXObject,
      AXObject* inspectedAXObject,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void addChild(std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
                AXObject& childAXObject,
                AXObject* inspectedAXObject,
                std::unique_ptr<protocol::Array<AXNode>>& nodes,
                AXObjectCacheImpl&) const;
  void addChildren(AXObject&,
                   AXObject* inspectedAXObject,
                   std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
                   std::unique_ptr<protocol::Array<AXNode>>& nodes,
                   AXObjectCacheImpl&) const;

  Member<Page> m_page;
  Member<InspectorDOMAgent> m_domAgent;
};

}  // namespace blink

#endif  // InspectorAccessibilityAgent_h
