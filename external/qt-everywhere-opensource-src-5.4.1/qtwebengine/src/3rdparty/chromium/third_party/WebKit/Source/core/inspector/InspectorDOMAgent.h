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

#include "core/InspectorFrontend.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/rendering/RenderLayer.h"
#include "platform/JSONValues.h"

#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class CharacterData;
class DOMEditor;
class Document;
class Element;
class EventTarget;
class ExceptionState;
class InspectorFrontend;
class InspectorHistory;
class InspectorOverlay;
class InspectorPageAgent;
class InspectorState;
class InstrumentingAgents;
class Node;
class PlatformGestureEvent;
class PlatformTouchEvent;
class RevalidateStyleAttributeTask;
class ShadowRoot;

struct HighlightConfig;

typedef String ErrorString;


struct EventListenerInfo {
    EventListenerInfo(EventTarget* eventTarget, const AtomicString& eventType, const EventListenerVector& eventListenerVector)
        : eventTarget(eventTarget)
        , eventType(eventType)
        , eventListenerVector(eventListenerVector)
    {
    }

    EventTarget* eventTarget;
    const AtomicString eventType;
    const EventListenerVector eventListenerVector;
};

class InspectorDOMAgent FINAL : public InspectorBaseAgent<InspectorDOMAgent>, public InspectorBackendDispatcher::DOMCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);
public:
    struct DOMListener {
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    static PassOwnPtr<InspectorDOMAgent> create(InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager, InspectorOverlay* overlay)
    {
        return adoptPtr(new InspectorDOMAgent(pageAgent, injectedScriptManager, overlay));
    }

    static String toErrorString(ExceptionState&);

    virtual ~InspectorDOMAgent();

    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;

    Vector<Document*> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void querySelector(ErrorString*, int nodeId, const String& selectors, int* elementId) OVERRIDE;
    virtual void querySelectorAll(ErrorString*, int nodeId, const String& selectors, RefPtr<TypeBuilder::Array<int> >& result) OVERRIDE;
    virtual void getDocument(ErrorString*, RefPtr<TypeBuilder::DOM::Node>& root) OVERRIDE;
    virtual void requestChildNodes(ErrorString*, int nodeId, const int* depth) OVERRIDE;
    virtual void setAttributeValue(ErrorString*, int elementId, const String& name, const String& value) OVERRIDE;
    virtual void setAttributesAsText(ErrorString*, int elementId, const String& text, const String* name) OVERRIDE;
    virtual void removeAttribute(ErrorString*, int elementId, const String& name) OVERRIDE;
    virtual void removeNode(ErrorString*, int nodeId) OVERRIDE;
    virtual void setNodeName(ErrorString*, int nodeId, const String& name, int* newId) OVERRIDE;
    virtual void getOuterHTML(ErrorString*, int nodeId, WTF::String* outerHTML) OVERRIDE;
    virtual void setOuterHTML(ErrorString*, int nodeId, const String& outerHTML) OVERRIDE;
    virtual void setNodeValue(ErrorString*, int nodeId, const String& value) OVERRIDE;
    virtual void getEventListenersForNode(ErrorString*, int nodeId, const WTF::String* objectGroup, RefPtr<TypeBuilder::Array<TypeBuilder::DOM::EventListener> >& listenersArray) OVERRIDE;
    virtual void performSearch(ErrorString*, const String& whitespaceTrimmedQuery, String* searchId, int* resultCount) OVERRIDE;
    virtual void getSearchResults(ErrorString*, const String& searchId, int fromIndex, int toIndex, RefPtr<TypeBuilder::Array<int> >&) OVERRIDE;
    virtual void discardSearchResults(ErrorString*, const String& searchId) OVERRIDE;
    virtual void resolveNode(ErrorString*, int nodeId, const String* objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& result) OVERRIDE;
    virtual void getAttributes(ErrorString*, int nodeId, RefPtr<TypeBuilder::Array<String> >& result) OVERRIDE;
    virtual void setInspectModeEnabled(ErrorString*, bool enabled, const bool* inspectUAShadowDOM, const RefPtr<JSONObject>* highlightConfig) OVERRIDE;
    virtual void requestNode(ErrorString*, const String& objectId, int* nodeId) OVERRIDE;
    virtual void pushNodeByPathToFrontend(ErrorString*, const String& path, int* nodeId) OVERRIDE;
    virtual void pushNodesByBackendIdsToFrontend(ErrorString*, const RefPtr<JSONArray>& nodeIds, RefPtr<TypeBuilder::Array<int> >&) OVERRIDE;
    virtual void hideHighlight(ErrorString*) OVERRIDE;
    virtual void highlightRect(ErrorString*, int x, int y, int width, int height, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) OVERRIDE;
    virtual void highlightQuad(ErrorString*, const RefPtr<JSONArray>& quad, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) OVERRIDE;
    virtual void highlightNode(ErrorString*, const RefPtr<JSONObject>& highlightConfig, const int* nodeId, const String* objectId) OVERRIDE;
    virtual void highlightFrame(ErrorString*, const String& frameId, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) OVERRIDE;

    virtual void moveTo(ErrorString*, int nodeId, int targetNodeId, const int* anchorNodeId, int* newNodeId) OVERRIDE;
    virtual void undo(ErrorString*) OVERRIDE;
    virtual void redo(ErrorString*) OVERRIDE;
    virtual void markUndoableState(ErrorString*) OVERRIDE;
    virtual void focus(ErrorString*, int nodeId) OVERRIDE;
    virtual void setFileInputFiles(ErrorString*, int nodeId, const RefPtr<JSONArray>& files) OVERRIDE;
    virtual void getBoxModel(ErrorString*, int nodeId, RefPtr<TypeBuilder::DOM::BoxModel>&) OVERRIDE;
    virtual void getNodeForLocation(ErrorString*, int x, int y, int* nodeId) OVERRIDE;
    virtual void getRelayoutBoundary(ErrorString*, int nodeId, int* relayoutBoundaryNodeId) OVERRIDE;

    static void getEventListeners(EventTarget*, Vector<EventListenerInfo>& listenersArray, bool includeAncestors);

    class Listener {
    public:
        virtual ~Listener() { }
        virtual void domAgentWasEnabled() = 0;
        virtual void domAgentWasDisabled() = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

    bool enabled() const;

    // Methods called from the InspectorInstrumentation.
    void setDocument(Document*);
    void releaseDanglingNodes();

    void domContentLoadedEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);

    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyDOMAttr(Element*, const AtomicString& name, const AtomicString& value);
    void didRemoveDOMAttr(Element*, const AtomicString& name);
    void styleAttributeInvalidated(const WillBeHeapVector<RawPtrWillBeMember<Element> >& elements);
    void characterDataModified(CharacterData*);
    void didInvalidateStyleAttr(Node*);
    void didPushShadowRoot(Element* host, ShadowRoot*);
    void willPopShadowRoot(Element* host, ShadowRoot*);
    void frameDocumentUpdated(LocalFrame*);
    void pseudoElementCreated(PseudoElement*);
    void pseudoElementDestroyed(PseudoElement*);

    Node* nodeForId(int nodeId);
    int boundNodeId(Node*);
    void setDOMListener(DOMListener*);

    static String documentURLString(Document*);

    PassRefPtr<TypeBuilder::Runtime::RemoteObject> resolveNode(Node*, const String& objectGroup);
    bool handleMousePress();
    bool handleGestureEvent(LocalFrame*, const PlatformGestureEvent&);
    bool handleTouchEvent(LocalFrame*, const PlatformTouchEvent&);
    void handleMouseMove(LocalFrame*, const PlatformMouseEvent&);

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
    enum SearchMode { NotSearching, SearchingForNormal, SearchingForUAShadow };

    InspectorDOMAgent(InspectorPageAgent*, InjectedScriptManager*, InspectorOverlay*);

    void setSearchingForNode(ErrorString*, SearchMode, JSONObject* highlightConfig);
    PassOwnPtr<HighlightConfig> highlightConfigFromInspectorObject(ErrorString*, JSONObject* highlightInspectorObject);

    // Node-related methods.
    typedef WillBeHeapHashMap<RefPtrWillBeMember<Node>, int> NodeToIdMap;
    int bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(ErrorString*, int nodeId);
    Element* assertEditableElement(ErrorString*, int nodeId);

    void inspect(Node*);

    int pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(int nodeId, int depth = 1);

    void invalidateFrameOwnerElement(LocalFrame*);

    bool hasBreakpoint(Node*, int type);
    void updateSubtreeBreakpoints(Node* root, uint32_t rootMask, bool value);
    void descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, PassRefPtr<JSONObject> description);

    PassRefPtr<TypeBuilder::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    PassRefPtr<TypeBuilder::Array<String> > buildArrayForElementAttributes(Element*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    PassRefPtr<TypeBuilder::DOM::EventListener> buildObjectForEventListener(const RegisteredEventListener&, const AtomicString& eventType, Node*, const String* objectGroupId);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::DOM::Node> > buildArrayForPseudoElements(Element*, NodeToIdMap* nodesMap);

    Node* nodeForPath(const String& path);

    void discardFrontendBindings();

    void innerHighlightQuad(PassOwnPtr<FloatQuad>, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor);

    bool pushDocumentUponHandlelessOperation(ErrorString*);

    InspectorPageAgent* m_pageAgent;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorOverlay* m_overlay;
    InspectorFrontend::DOM* m_frontend;
    DOMListener* m_domListener;
    OwnPtrWillBePersistent<NodeToIdMap> m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    WillBePersistentHeapVector<OwnPtrWillBeMember<NodeToIdMap> > m_danglingNodeToIdMaps;
    WillBePersistentHeapHashMap<int, RawPtrWillBeMember<Node> > m_idToNode;
    WillBePersistentHeapHashMap<int, RawPtrWillBeMember<NodeToIdMap> > m_idToNodesMap;
    HashSet<int> m_childrenRequested;
    HashMap<int, int> m_cachedChildCount;
    int m_lastNodeId;
    RefPtrWillBePersistent<Document> m_document;
    typedef WillBePersistentHeapHashMap<String, WillBeHeapVector<RefPtrWillBeMember<Node> > > SearchResults;
    SearchResults m_searchResults;
    OwnPtr<RevalidateStyleAttributeTask> m_revalidateStyleAttrTask;
    SearchMode m_searchingForNode;
    OwnPtr<HighlightConfig> m_inspectModeHighlightConfig;
    OwnPtrWillBePersistent<InspectorHistory> m_history;
    OwnPtrWillBePersistent<DOMEditor> m_domEditor;
    bool m_suppressAttributeModifiedEvent;
    Listener* m_listener;
};


} // namespace WebCore

#endif // !defined(InspectorDOMAgent_h)
