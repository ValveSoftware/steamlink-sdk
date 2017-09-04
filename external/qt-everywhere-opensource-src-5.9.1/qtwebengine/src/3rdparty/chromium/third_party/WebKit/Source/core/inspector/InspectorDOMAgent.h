/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMAgent_h
#define InspectorDOMAgent_h

#include "core/CoreExport.h"
#include "core/events/EventListenerMap.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/inspector/protocol/DOM.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/FloatQuad.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include <memory>
#include <v8-inspector.h>

namespace blink {

class CharacterData;
class DOMEditor;
class Document;
class DocumentLoader;
class Element;
class ExceptionState;
class FloatQuad;
class HTMLSlotElement;
class InsertionPoint;
class InspectedFrames;
class InspectorHistory;
class Node;
class QualifiedName;
class PseudoElement;
class InspectorRevalidateDOMTask;
class ShadowRoot;

class CORE_EXPORT InspectorDOMAgent final
    : public InspectorBaseAgent<protocol::DOM::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);

 public:
  struct CORE_EXPORT DOMListener : public GarbageCollectedMixin {
    virtual ~DOMListener() {}
    virtual void didAddDocument(Document*) = 0;
    virtual void didRemoveDocument(Document*) = 0;
    virtual void didRemoveDOMNode(Node*) = 0;
    virtual void didModifyDOMAttr(Element*) = 0;
  };

  enum SearchMode {
    NotSearching,
    SearchingForNormal,
    SearchingForUAShadow,
    ShowLayoutEditor
  };

  class Client {
   public:
    virtual ~Client() {}
    virtual void hideHighlight() {}
    virtual void highlightNode(Node*,
                               const InspectorHighlightConfig&,
                               bool omitTooltip) {}
    virtual void highlightQuad(std::unique_ptr<FloatQuad>,
                               const InspectorHighlightConfig&) {}
    virtual void setInspectMode(SearchMode searchMode,
                                std::unique_ptr<InspectorHighlightConfig>) {}
    virtual void setInspectedNode(Node*) {}
  };

  static Response toResponse(ExceptionState&);
  static bool getPseudoElementType(PseudoId, String*);
  static ShadowRoot* userAgentShadowRoot(Node*);

  InspectorDOMAgent(v8::Isolate*,
                    InspectedFrames*,
                    v8_inspector::V8InspectorSession*,
                    Client*);
  ~InspectorDOMAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void restore() override;

  HeapVector<Member<Document>> documents();
  void reset();

  // Methods called from the frontend for DOM nodes inspection.
  Response enable() override;
  Response disable() override;
  Response getDocument(Maybe<int> depth,
                       Maybe<bool> traverseFrames,
                       std::unique_ptr<protocol::DOM::Node>* root) override;
  Response collectClassNamesFromSubtree(
      int nodeId,
      std::unique_ptr<protocol::Array<String>>* classNames) override;
  Response requestChildNodes(int nodeId,
                             Maybe<int> depth,
                             Maybe<bool> traverseFrames) override;
  Response querySelector(int nodeId,
                         const String& selector,
                         int* outNodeId) override;
  Response querySelectorAll(
      int nodeId,
      const String& selector,
      std::unique_ptr<protocol::Array<int>>* nodeIds) override;
  Response setNodeName(int nodeId, const String& name, int* outNodeId) override;
  Response setNodeValue(int nodeId, const String& value) override;
  Response removeNode(int nodeId) override;
  Response setAttributeValue(int nodeId,
                             const String& name,
                             const String& value) override;
  Response setAttributesAsText(int nodeId,
                               const String& text,
                               Maybe<String> name) override;
  Response removeAttribute(int nodeId, const String& name) override;
  Response getOuterHTML(int nodeId, String* outerHTML) override;
  Response setOuterHTML(int nodeId, const String& outerHTML) override;
  Response performSearch(const String& query,
                         Maybe<bool> includeUserAgentShadowDOM,
                         String* searchId,
                         int* resultCount) override;
  Response getSearchResults(
      const String& searchId,
      int fromIndex,
      int toIndex,
      std::unique_ptr<protocol::Array<int>>* nodeIds) override;
  Response discardSearchResults(const String& searchId) override;
  Response requestNode(const String& objectId, int* outNodeId) override;
  Response setInspectMode(const String& mode,
                          Maybe<protocol::DOM::HighlightConfig>) override;
  Response highlightRect(int x,
                         int y,
                         int width,
                         int height,
                         Maybe<protocol::DOM::RGBA> color,
                         Maybe<protocol::DOM::RGBA> outlineColor) override;
  Response highlightQuad(std::unique_ptr<protocol::Array<double>> quad,
                         Maybe<protocol::DOM::RGBA> color,
                         Maybe<protocol::DOM::RGBA> outlineColor) override;
  Response highlightNode(std::unique_ptr<protocol::DOM::HighlightConfig>,
                         Maybe<int> nodeId,
                         Maybe<int> backendNodeId,
                         Maybe<String> objectId) override;
  Response hideHighlight() override;
  Response highlightFrame(
      const String& frameId,
      Maybe<protocol::DOM::RGBA> contentColor,
      Maybe<protocol::DOM::RGBA> contentOutlineColor) override;
  Response pushNodeByPathToFrontend(const String& path,
                                    int* outNodeId) override;
  Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backendNodeIds,
      std::unique_ptr<protocol::Array<int>>* nodeIds) override;
  Response setInspectedNode(int nodeId) override;
  Response resolveNode(
      int nodeId,
      Maybe<String> objectGroup,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;
  Response getAttributes(
      int nodeId,
      std::unique_ptr<protocol::Array<String>>* attributes) override;
  Response copyTo(int nodeId,
                  int targetNodeId,
                  Maybe<int> insertBeforeNodeId,
                  int* outNodeId) override;
  Response moveTo(int nodeId,
                  int targetNodeId,
                  Maybe<int> insertBeforeNodeId,
                  int* outNodeId) override;
  Response undo() override;
  Response redo() override;
  Response markUndoableState() override;
  Response focus(int nodeId) override;
  Response setFileInputFiles(
      int nodeId,
      std::unique_ptr<protocol::Array<String>> files) override;
  Response getBoxModel(int nodeId,
                       std::unique_ptr<protocol::DOM::BoxModel>*) override;
  Response getNodeForLocation(int x, int y, int* outNodeId) override;
  Response getRelayoutBoundary(int nodeId, int* outNodeId) override;
  Response getHighlightObjectForTest(
      int nodeId,
      std::unique_ptr<protocol::DictionaryValue>* highlight) override;

  bool enabled() const;
  void releaseDanglingNodes();

  // Methods called from the InspectorInstrumentation.
  void domContentLoadedEventFired(LocalFrame*);
  void didCommitLoad(LocalFrame*, DocumentLoader*);
  void didInsertDOMNode(Node*);
  void willRemoveDOMNode(Node*);
  void willModifyDOMAttr(Element*,
                         const AtomicString& oldValue,
                         const AtomicString& newValue);
  void didModifyDOMAttr(Element*,
                        const QualifiedName&,
                        const AtomicString& value);
  void didRemoveDOMAttr(Element*, const QualifiedName&);
  void styleAttributeInvalidated(const HeapVector<Member<Element>>& elements);
  void characterDataModified(CharacterData*);
  void didInvalidateStyleAttr(Node*);
  void didPushShadowRoot(Element* host, ShadowRoot*);
  void willPopShadowRoot(Element* host, ShadowRoot*);
  void didPerformElementShadowDistribution(Element*);
  void didPerformSlotDistribution(HTMLSlotElement*);
  void frameDocumentUpdated(LocalFrame*);
  void pseudoElementCreated(PseudoElement*);
  void pseudoElementDestroyed(PseudoElement*);

  Node* nodeForId(int nodeId);
  int boundNodeId(Node*);
  void setDOMListener(DOMListener*);
  void inspect(Node*);
  void nodeHighlightedInOverlay(Node*);
  int pushNodePathToFrontend(Node*);

  static String documentURLString(Document*);

  std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
  resolveNode(Node*, const String& objectGroup);

  InspectorHistory* history() { return m_history.get(); }

  // We represent embedded doms as a part of the same hierarchy. Hence we treat
  // children of frame owners differently.  We also skip whitespace text nodes
  // conditionally. Following methods encapsulate these specifics.
  static Node* innerFirstChild(Node*);
  static Node* innerNextSibling(Node*);
  static Node* innerPreviousSibling(Node*);
  static unsigned innerChildNodeCount(Node*);
  static Node* innerParentNode(Node*);
  static bool isWhitespace(Node*);

  Response assertNode(int nodeId, Node*&);
  Response assertElement(int nodeId, Element*&);
  Document* document() const { return m_document.get(); }

 private:
  void setDocument(Document*);
  void innerEnable();

  Response setSearchingForNode(SearchMode,
                               Maybe<protocol::DOM::HighlightConfig>);
  Response highlightConfigFromInspectorObject(
      Maybe<protocol::DOM::HighlightConfig> highlightInspectorObject,
      std::unique_ptr<InspectorHighlightConfig>*);

  // Node-related methods.
  typedef HeapHashMap<Member<Node>, int> NodeToIdMap;
  int bind(Node*, NodeToIdMap*);
  void unbind(Node*, NodeToIdMap*);

  Response assertEditableNode(int nodeId, Node*&);
  Response assertEditableChildNode(Element* parentElement, int nodeId, Node*&);
  Response assertEditableElement(int nodeId, Element*&);

  int pushNodePathToFrontend(Node*, NodeToIdMap* nodeMap);
  void pushChildNodesToFrontend(int nodeId,
                                int depth = 1,
                                bool traverseFrames = false);

  void invalidateFrameOwnerElement(LocalFrame*);

  std::unique_ptr<protocol::DOM::Node> buildObjectForNode(Node*,
                                                          int depth,
                                                          bool traverseFrames,
                                                          NodeToIdMap*);
  std::unique_ptr<protocol::Array<String>> buildArrayForElementAttributes(
      Element*);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  buildArrayForContainerChildren(Node* container,
                                 int depth,
                                 bool traverseFrames,
                                 NodeToIdMap* nodesMap);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  buildArrayForPseudoElements(Element*, NodeToIdMap* nodesMap);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  buildArrayForDistributedNodes(InsertionPoint*);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  buildDistributedNodesForSlot(HTMLSlotElement*);

  Node* nodeForPath(const String& path);
  Response nodeForRemoteId(const String& id, Node*&);

  void discardFrontendBindings();

  void innerHighlightQuad(std::unique_ptr<FloatQuad>,
                          Maybe<protocol::DOM::RGBA> color,
                          Maybe<protocol::DOM::RGBA> outlineColor);

  Response pushDocumentUponHandlelessOperation();

  Member<InspectorRevalidateDOMTask> revalidateTask();

  v8::Isolate* m_isolate;
  Member<InspectedFrames> m_inspectedFrames;
  v8_inspector::V8InspectorSession* m_v8Session;
  Client* m_client;
  Member<DOMListener> m_domListener;
  Member<NodeToIdMap> m_documentNodeToIdMap;
  // Owns node mappings for dangling nodes.
  HeapVector<Member<NodeToIdMap>> m_danglingNodeToIdMaps;
  HeapHashMap<int, Member<Node>> m_idToNode;
  HeapHashMap<int, Member<NodeToIdMap>> m_idToNodesMap;
  HashSet<int> m_childrenRequested;
  HashSet<int> m_distributedNodesRequested;
  HashMap<int, int> m_cachedChildCount;
  int m_lastNodeId;
  Member<Document> m_document;
  typedef HeapHashMap<String, HeapVector<Member<Node>>> SearchResults;
  SearchResults m_searchResults;
  Member<InspectorRevalidateDOMTask> m_revalidateTask;
  Member<InspectorHistory> m_history;
  Member<DOMEditor> m_domEditor;
  bool m_suppressAttributeModifiedEvent;
  int m_backendNodeIdToInspect;
};

}  // namespace blink

#endif  // !defined(InspectorDOMAgent_h)
