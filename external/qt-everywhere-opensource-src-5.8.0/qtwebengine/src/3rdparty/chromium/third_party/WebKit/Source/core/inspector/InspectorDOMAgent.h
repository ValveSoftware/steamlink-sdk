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
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class CharacterData;
class DOMEditor;
class Document;
class DocumentLoader;
class Element;
class EventTarget;
class ExceptionState;
class FloatQuad;
class InsertionPoint;
class InspectedFrames;
class InspectorHistory;
class Node;
class QualifiedName;
class PseudoElement;
class PlatformGestureEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class InspectorRevalidateDOMTask;
class ShadowRoot;

class CORE_EXPORT InspectorDOMAgent final : public InspectorBaseAgent<protocol::DOM::Metainfo> {
    WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);
public:
    struct CORE_EXPORT DOMListener : public GarbageCollectedMixin {
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    enum SearchMode { NotSearching, SearchingForNormal, SearchingForUAShadow, ShowLayoutEditor };

    class Client {
    public:
        virtual ~Client() { }
        virtual void hideHighlight() { }
        virtual void highlightNode(Node*, const InspectorHighlightConfig&, bool omitTooltip) { }
        virtual void highlightQuad(std::unique_ptr<FloatQuad>, const InspectorHighlightConfig&) { }
        virtual void setInspectMode(SearchMode searchMode, std::unique_ptr<InspectorHighlightConfig>) { }
        virtual void setInspectedNode(Node*) { }
    };

    static String toErrorString(ExceptionState&);
    static bool getPseudoElementType(PseudoId, String*);
    static ShadowRoot* userAgentShadowRoot(Node*);

    InspectorDOMAgent(v8::Isolate*, InspectedFrames*, V8InspectorSession*, Client*);
    ~InspectorDOMAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void restore() override;

    HeapVector<Member<Document>> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void getDocument(ErrorString*, std::unique_ptr<protocol::DOM::Node>* root) override;
    void requestChildNodes(ErrorString*, int nodeId, const Maybe<int>& depth) override;
    void querySelector(ErrorString*, int nodeId, const String& selector, int* outNodeId) override;
    void querySelectorAll(ErrorString*, int nodeId, const String& selector, std::unique_ptr<protocol::Array<int>>* nodeIds) override;
    void setNodeName(ErrorString*, int nodeId, const String& name, int* outNodeId) override;
    void setNodeValue(ErrorString*, int nodeId, const String& value) override;
    void removeNode(ErrorString*, int nodeId) override;
    void setAttributeValue(ErrorString*, int nodeId, const String& name, const String& value) override;
    void setAttributesAsText(ErrorString*, int nodeId, const String& text, const Maybe<String>& name) override;
    void removeAttribute(ErrorString*, int nodeId, const String& name) override;
    void getOuterHTML(ErrorString*, int nodeId, String* outerHTML) override;
    void setOuterHTML(ErrorString*, int nodeId, const String& outerHTML) override;
    void performSearch(ErrorString*, const String& query, const Maybe<bool>& includeUserAgentShadowDOM, String* searchId, int* resultCount) override;
    void getSearchResults(ErrorString*, const String& searchId, int fromIndex, int toIndex, std::unique_ptr<protocol::Array<int>>* nodeIds) override;
    void discardSearchResults(ErrorString*, const String& searchId) override;
    void requestNode(ErrorString*, const String& objectId, int* outNodeId) override;
    void setInspectMode(ErrorString*, const String& mode, const Maybe<protocol::DOM::HighlightConfig>&) override;
    void highlightRect(ErrorString*, int x, int y, int width, int height, const Maybe<protocol::DOM::RGBA>& color, const Maybe<protocol::DOM::RGBA>& outlineColor) override;
    void highlightQuad(ErrorString*, std::unique_ptr<protocol::Array<double>> quad, const Maybe<protocol::DOM::RGBA>& color, const Maybe<protocol::DOM::RGBA>& outlineColor) override;
    void highlightNode(ErrorString*, std::unique_ptr<protocol::DOM::HighlightConfig>, const Maybe<int>& nodeId, const Maybe<int>& backendNodeId, const Maybe<String>& objectId) override;
    void hideHighlight(ErrorString*) override;
    void highlightFrame(ErrorString*, const String& frameId, const Maybe<protocol::DOM::RGBA>& contentColor, const Maybe<protocol::DOM::RGBA>& contentOutlineColor) override;
    void pushNodeByPathToFrontend(ErrorString*, const String& path, int* outNodeId) override;
    void pushNodesByBackendIdsToFrontend(ErrorString*, std::unique_ptr<protocol::Array<int>> backendNodeIds, std::unique_ptr<protocol::Array<int>>* nodeIds) override;
    void setInspectedNode(ErrorString*, int nodeId) override;
    void resolveNode(ErrorString*, int nodeId, const Maybe<String>& objectGroup, std::unique_ptr<protocol::Runtime::RemoteObject>*) override;
    void getAttributes(ErrorString*, int nodeId, std::unique_ptr<protocol::Array<String>>* attributes) override;
    void copyTo(ErrorString*, int nodeId, int targetNodeId, const Maybe<int>& insertBeforeNodeId, int* outNodeId) override;
    void moveTo(ErrorString*, int nodeId, int targetNodeId, const Maybe<int>& insertBeforeNodeId, int* outNodeId) override;
    void undo(ErrorString*) override;
    void redo(ErrorString*) override;
    void markUndoableState(ErrorString*) override;
    void focus(ErrorString*, int nodeId) override;
    void setFileInputFiles(ErrorString*, int nodeId, std::unique_ptr<protocol::Array<String>> files) override;
    void getBoxModel(ErrorString*, int nodeId, std::unique_ptr<protocol::DOM::BoxModel>*) override;
    void getNodeForLocation(ErrorString*, int x, int y, int* outNodeId) override;
    void getRelayoutBoundary(ErrorString*, int nodeId, int* outNodeId) override;
    void getHighlightObjectForTest(ErrorString*, int nodeId, std::unique_ptr<protocol::DictionaryValue>* highlight) override;

    bool enabled() const;
    void releaseDanglingNodes();

    // Methods called from the InspectorInstrumentation.
    void domContentLoadedEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyDOMAttr(Element*, const QualifiedName&, const AtomicString& value);
    void didRemoveDOMAttr(Element*, const QualifiedName&);
    void styleAttributeInvalidated(const HeapVector<Member<Element>>& elements);
    void characterDataModified(CharacterData*);
    void didInvalidateStyleAttr(Node*);
    void didPushShadowRoot(Element* host, ShadowRoot*);
    void willPopShadowRoot(Element* host, ShadowRoot*);
    void didPerformElementShadowDistribution(Element*);
    void frameDocumentUpdated(LocalFrame*);
    void pseudoElementCreated(PseudoElement*);
    void pseudoElementDestroyed(PseudoElement*);

    Node* nodeForId(int nodeId);
    int boundNodeId(Node*);
    void setDOMListener(DOMListener*);
    void inspect(Node*);
    void nodeHighlightedInOverlay(Node*);

    static String documentURLString(Document*);

    std::unique_ptr<protocol::Runtime::RemoteObject> resolveNode(Node*, const String& objectGroup);

    InspectorHistory* history() { return m_history.get(); }

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);
    static bool isWhitespace(Node*);

    Node* assertNode(ErrorString*, int nodeId);
    Element* assertElement(ErrorString*, int nodeId);
    Document* assertDocument(ErrorString*, int nodeId);

private:
    void setDocument(Document*);
    void innerEnable();

    void setSearchingForNode(ErrorString*, SearchMode, const Maybe<protocol::DOM::HighlightConfig>&);
    std::unique_ptr<InspectorHighlightConfig> highlightConfigFromInspectorObject(ErrorString*, const Maybe<protocol::DOM::HighlightConfig>& highlightInspectorObject);

    // Node-related methods.
    typedef HeapHashMap<Member<Node>, int> NodeToIdMap;
    int bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(ErrorString*, int nodeId);
    Node* assertEditableChildNode(ErrorString*, Element* parentElement, int nodeId);
    Element* assertEditableElement(ErrorString*, int nodeId);

    int pushNodePathToFrontend(Node*, NodeToIdMap* nodeMap);
    int pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(int nodeId, int depth = 1);

    void invalidateFrameOwnerElement(LocalFrame*);

    std::unique_ptr<protocol::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    std::unique_ptr<protocol::Array<String>> buildArrayForElementAttributes(Element*);
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> buildArrayForPseudoElements(Element*, NodeToIdMap* nodesMap);
    std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>> buildArrayForDistributedNodes(InsertionPoint*);

    Node* nodeForPath(const String& path);
    Node* nodeForRemoteId(ErrorString*, const String& id);

    void discardFrontendBindings();

    void innerHighlightQuad(std::unique_ptr<FloatQuad>, const Maybe<protocol::DOM::RGBA>& color, const Maybe<protocol::DOM::RGBA>& outlineColor);

    bool pushDocumentUponHandlelessOperation(ErrorString*);

    Member<InspectorRevalidateDOMTask> revalidateTask();

    v8::Isolate* m_isolate;
    Member<InspectedFrames> m_inspectedFrames;
    V8InspectorSession* m_v8Session;
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


} // namespace blink

#endif // !defined(InspectorDOMAgent_h)
