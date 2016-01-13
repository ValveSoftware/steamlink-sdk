/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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

#ifndef FrameLoader_h
#define FrameLoader_h

#include "core/dom/IconURL.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/SecurityContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/MixedContentChecker.h"
#include "platform/Timer.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class Resource;
class Chrome;
class DOMWrapperWorld;
class DocumentLoader;
class Event;
class FetchContext;
class FormState;
class FormSubmission;
class FrameLoaderClient;
class IconController;
class NavigationAction;
class Page;
class ProgressTracker;
class ResourceError;
class ResourceResponse;
class SecurityOrigin;
class SerializedScriptValue;
class SubstituteData;

struct FrameLoadRequest;
struct WindowFeatures;

bool isBackForwardLoadType(FrameLoadType);

class FrameLoader {
    WTF_MAKE_NONCOPYABLE(FrameLoader);
public:
    FrameLoader(LocalFrame*);
    ~FrameLoader();

    void init();

    LocalFrame* frame() const { return m_frame; }

    MixedContentChecker* mixedContentChecker() const { return &m_mixedContentChecker; }
    ProgressTracker& progress() const { return *m_progressTracker; }

    // These functions start a load. All eventually call into loadWithNavigationAction() or loadInSameDocument().
    void load(const FrameLoadRequest&); // The entry point for non-reload, non-history loads.
    void reload(ReloadPolicy = NormalReload, const KURL& overrideURL = KURL(), const AtomicString& overrideEncoding = nullAtom);
    void loadHistoryItem(HistoryItem*, HistoryLoadType = HistoryDifferentDocumentLoad, ResourceRequestCachePolicy = UseProtocolCachePolicy); // The entry point for all back/forward loads

    static void reportLocalLoadFailed(LocalFrame*, const String& url);

    // FIXME: These are all functions which stop loads. We have too many.
    // Warning: stopAllLoaders can and will detach the LocalFrame out from under you. All callers need to either protect the LocalFrame
    // or guarantee they won't in any way access the LocalFrame after stopAllLoaders returns.
    void stopAllLoaders();
    void stopLoading();
    bool closeURL();
    // FIXME: clear() is trying to do too many things. We should break it down into smaller functions.
    void clear();

    // Sets a timer to notify the client that the initial empty document has
    // been accessed, and thus it is no longer safe to show a provisional URL
    // above the document without risking a URL spoof.
    void didAccessInitialDocument();

    // If the initial empty document is showing and has been accessed, this
    // cancels the timer and immediately notifies the client in cases that
    // waiting to notify would allow a URL spoof.
    void notifyIfInitialDocumentAccessed();

    bool isLoading() const;

    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }
    DocumentLoader* policyDocumentLoader() const { return m_policyDocumentLoader.get(); }
    DocumentLoader* provisionalDocumentLoader() const { return m_provisionalDocumentLoader.get(); }
    FrameState state() const { return m_state; }
    FetchContext& fetchContext() const { return *m_fetchContext; }

    void receivedMainResourceError(const ResourceError&);

    bool isLoadingMainFrame() const;

    bool shouldTreatURLAsSameAsCurrent(const KURL&) const;
    bool shouldTreatURLAsSrcdocDocument(const KURL&) const;

    FrameLoadType loadType() const;
    void setLoadType(FrameLoadType loadType) { m_loadType = loadType; }

    void checkLoadComplete();

    static void addHTTPOriginIfNeeded(ResourceRequest&, const AtomicString& origin);

    FrameLoaderClient* client() const;

    void setDefersLoading(bool);

    void didExplicitOpen();

    // Callbacks from DocumentWriter
    void didBeginDocument(bool dispatchWindowObjectAvailable);

    void receivedFirstData();

    String userAgent(const KURL&) const;

    void dispatchDidClearWindowObjectInMainWorld();
    void dispatchDidClearDocumentOfWindowObject();
    void dispatchDocumentElementAvailable();

    // The following sandbox flags will be forced, regardless of changes to
    // the sandbox attribute of any parent frames.
    void forceSandboxFlags(SandboxFlags flags) { m_forcedSandboxFlags |= flags; }
    SandboxFlags effectiveSandboxFlags() const;

    LocalFrame* opener();
    void setOpener(LocalFrame*);

    void frameDetached();

    void loadDone();
    void finishedParsing();
    void checkCompleted();

    void commitProvisionalLoad();

    FrameLoaderStateMachine* stateMachine() const { return &m_stateMachine; }

    LocalFrame* findFrameForNavigation(const AtomicString& name, Document* activeDocument);

    void applyUserAgent(ResourceRequest&);

    bool shouldInterruptLoadForXFrameOptions(const String&, const KURL&, unsigned long requestIdentifier);

    bool allAncestorsAreComplete() const; // including this

    bool shouldClose();

    void started();

    bool allowPlugins(ReasonForCallingAllowPlugins);

    void updateForSameDocumentNavigation(const KURL&, SameDocumentNavigationSource, PassRefPtr<SerializedScriptValue>, FrameLoadType);

    HistoryItem* currentItem() const { return m_currentItem.get(); }
    void saveScrollState();
    void clearScrollPositionAndViewState();

    void restoreScrollPositionAndViewState();

private:
    bool allChildrenAreComplete() const; // immediate children, not all descendants

    void completed();

    void checkTimerFired(Timer<FrameLoader>*);
    void didAccessInitialDocumentTimerFired(Timer<FrameLoader>*);

    bool prepareRequestForThisFrame(FrameLoadRequest&);
    void setReferrerForFrameRequest(ResourceRequest&, ShouldSendReferrer, Document*);
    FrameLoadType determineFrameLoadType(const FrameLoadRequest&);
    bool isScriptTriggeredFormSubmissionInChildFrame(const FrameLoadRequest&) const;

    SubstituteData defaultSubstituteDataForURL(const KURL&);

    bool shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType, const KURL&);
    void scrollToFragmentWithParentBoundary(const KURL&);

    bool checkLoadCompleteForThisFrame();

    // Calls continueLoadAfterNavigationPolicy
    void loadWithNavigationAction(const NavigationAction&, FrameLoadType, PassRefPtrWillBeRawPtr<FormState>,
        const SubstituteData&, ClientRedirectPolicy = NotClientRedirect, const AtomicString& overrideEncoding = nullAtom);

    void detachFromParent();
    void detachChildren();
    void detachClient();

    void setHistoryItemStateForCommit(HistoryCommitType, bool isPushOrReplaceState = false, PassRefPtr<SerializedScriptValue> = nullptr);

    void loadInSameDocument(const KURL&, PassRefPtr<SerializedScriptValue> stateObject, FrameLoadType, ClientRedirectPolicy);

    void scheduleCheckCompleted();
    void startCheckCompleteTimer();

    LocalFrame* m_frame;

    // FIXME: These should be OwnPtr<T> to reduce build times and simplify
    // header dependencies unless performance testing proves otherwise.
    // Some of these could be lazily created for memory savings on devices.
    mutable FrameLoaderStateMachine m_stateMachine;
    mutable MixedContentChecker m_mixedContentChecker;

    OwnPtr<ProgressTracker> m_progressTracker;

    FrameState m_state;
    FrameLoadType m_loadType;

    // Document loaders for the three phases of frame loading. Note that while
    // a new request is being loaded, the old document loader may still be referenced.
    // E.g. while a new request is in the "policy" state, the old document loader may
    // be consulted in particular as it makes sense to imply certain settings on the new loader.
    RefPtr<DocumentLoader> m_documentLoader;
    RefPtr<DocumentLoader> m_provisionalDocumentLoader;
    RefPtr<DocumentLoader> m_policyDocumentLoader;
    OwnPtr<FetchContext> m_fetchContext;

    RefPtr<HistoryItem> m_currentItem;
    RefPtr<HistoryItem> m_provisionalItem;
    struct DeferredHistoryLoad {
        DeferredHistoryLoad(HistoryItem* item, HistoryLoadType type, ResourceRequestCachePolicy cachePolicy)
            : m_item(item)
            , m_type(type)
            , m_cachePolicy(cachePolicy)
        {
        }

        DeferredHistoryLoad() { }

        bool isValid() { return m_item; }

        RefPtr<HistoryItem> m_item;
        HistoryLoadType m_type;
        ResourceRequestCachePolicy m_cachePolicy;
    };
    DeferredHistoryLoad m_deferredHistoryLoad;

    bool m_inStopAllLoaders;

    // FIXME: This is only used in checkCompleted(). Figure out a way to disentangle it.
    bool m_isComplete;

    Timer<FrameLoader> m_checkTimer;
    bool m_shouldCallCheckCompleted;

    bool m_didAccessInitialDocument;
    Timer<FrameLoader> m_didAccessInitialDocumentTimer;

    SandboxFlags m_forcedSandboxFlags;

    bool m_willDetachClient;
};

} // namespace WebCore

#endif // FrameLoader_h
