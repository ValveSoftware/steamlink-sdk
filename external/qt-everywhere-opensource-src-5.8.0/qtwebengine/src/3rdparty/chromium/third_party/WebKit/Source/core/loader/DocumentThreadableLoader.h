/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DocumentThreadableLoader_h
#define DocumentThreadableLoader_h

#include "core/CoreExport.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceOwner.h"
#include "core/loader/ThreadableLoader.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/ResourceError.h"
#include "wtf/Forward.h"
#include "wtf/WeakPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Document;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class ThreadableLoaderClient;

class CORE_EXPORT DocumentThreadableLoader final : public ThreadableLoader, private RawResourceClient {
    USING_FAST_MALLOC(DocumentThreadableLoader);
    public:
        static void loadResourceSynchronously(Document&, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
        static std::unique_ptr<DocumentThreadableLoader> create(Document&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
        ~DocumentThreadableLoader() override;

        void start(const ResourceRequest&) override;

        void overrideTimeout(unsigned long timeout) override;

        // |this| may be dead after calling this method in async mode.
        void cancel() override;
        void setDefersLoading(bool);

    private:
        enum BlockingBehavior {
            LoadSynchronously,
            LoadAsynchronously
        };

        DocumentThreadableLoader(Document&, ThreadableLoaderClient*, BlockingBehavior, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);

        void clear();

        // ResourceClient
        //
        // |this| may be dead after calling this method.
        void notifyFinished(Resource*) override;

        String debugName() const override { return "DocumentThreadableLoader"; }

        // RawResourceClient
        //
        // |this| may be dead after calling these methods.
        void dataSent(Resource*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
        void responseReceived(Resource*, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override;
        void setSerializedCachedMetadata(Resource*, const char*, size_t) override;
        void dataReceived(Resource*, const char* data, size_t dataLength) override;
        void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) override;
        void redirectBlocked() override;
        void dataDownloaded(Resource*, int) override;
        void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override;

        // |this| may be dead after calling this method in async mode.
        void cancelWithError(const ResourceError&);

        // Notify Inspector and log to console about resource response. Use
        // this method if response is not going to be finished normally.
        void reportResponseReceived(unsigned long identifier, const ResourceResponse&);

        // Methods containing code to handle resource fetch results which are
        // common to both sync and async mode.
        //
        // |this| may be dead after calling these method in async mode.
        void handleResponse(unsigned long identifier, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>);
        void handleReceivedData(const char* data, size_t dataLength);
        void handleSuccessfulFinish(unsigned long identifier, double finishTime);

        // |this| may be dead after calling this method.
        void didTimeout(Timer<DocumentThreadableLoader>*);
        // Calls the appropriate loading method according to policy and data
        // about origin. Only for handling the initial load (including fallback
        // after consulting ServiceWorker).
        //
        // |this| may be dead after calling this method in async mode.
        void dispatchInitialRequest(const ResourceRequest&);
        // |this| may be dead after calling this method in async mode.
        void makeCrossOriginAccessRequest(const ResourceRequest&);
        // Loads m_fallbackRequestForServiceWorker.
        //
        // |this| may be dead after calling this method in async mode.
        void loadFallbackRequestForServiceWorker();
        // Loads m_actualRequest.
        void loadActualRequest();
        // Clears m_actualRequest and reports access control check failure to
        // m_client.
        //
        // |this| may be dead after calling this method in async mode.
        void handlePreflightFailure(const String& url, const String& errorDescription);
        // Investigates the response for the preflight request. If successful,
        // the actual request will be made later in handleSuccessfulFinish().
        //
        // |this| may be dead after calling this method in async mode.
        void handlePreflightResponse(const ResourceResponse&);
        // |this| may be dead after calling this method.
        void handleError(const ResourceError&);

        void loadRequest(const ResourceRequest&, ResourceLoaderOptions);
        bool isAllowedRedirect(const KURL&) const;
        // Returns DoNotAllowStoredCredentials
        // if m_forceDoNotAllowStoredCredentials is set. Otherwise, just
        // returns allowCredentials value of m_resourceLoaderOptions.
        StoredCredentials effectiveAllowCredentials() const;

        // TODO(oilpan): DocumentThreadableLoader used to be a ResourceOwner,
        // but ResourceOwner was moved onto the oilpan heap before
        // DocumentThreadableLoader was ready. When DocumentThreadableLoader
        // moves onto the oilpan heap, make it a ResourceOwner again and remove
        // this re-implementation of ResourceOwner.
        RawResource* resource() const { return m_resource.get(); }
        void clearResource() { setResource(nullptr); }
        void setResource(RawResource* newResource)
        {
            if (newResource == m_resource)
                return;

            if (RawResource* oldResource = m_resource.release())
                oldResource->removeClient(this);

            if (newResource) {
                m_resource = newResource;
                m_resource->addClient(this);
            }
        }
        Persistent<RawResource> m_resource;
        // End of ResourceOwner re-implementation, see above.

        SecurityOrigin* getSecurityOrigin() const;
        Document& document() const;

        ThreadableLoaderClient* m_client;
        WeakPersistent<Document> m_document;

        const ThreadableLoaderOptions m_options;
        // Some items may be overridden by m_forceDoNotAllowStoredCredentials
        // and m_securityOrigin. In such a case, build a ResourceLoaderOptions
        // with up-to-date values from them and this variable, and use it.
        const ResourceLoaderOptions m_resourceLoaderOptions;

        bool m_forceDoNotAllowStoredCredentials;
        RefPtr<SecurityOrigin> m_securityOrigin;

        // True while the initial URL and all the URLs of the redirects
        // this object has followed, if any, are same-origin to
        // getSecurityOrigin().
        bool m_sameOriginRequest;
        // Set to true if the current request is cross-origin and not simple.
        bool m_crossOriginNonSimpleRequest;

        // Set to true when the response data is given to a data consumer
        // handle.
        bool m_isUsingDataConsumerHandle;

        const bool m_async;

        // Holds the original request context (used for sanity checks).
        WebURLRequest::RequestContext m_requestContext;

        // Holds the original request for fallback in case the Service Worker
        // does not respond.
        ResourceRequest m_fallbackRequestForServiceWorker;

        // Holds the original request and options for it during preflight
        // request handling phase.
        ResourceRequest m_actualRequest;
        ResourceLoaderOptions m_actualOptions;

        HTTPHeaderMap m_simpleRequestHeaders; // stores simple request headers in case of a cross-origin redirect.
        Timer<DocumentThreadableLoader> m_timeoutTimer;
        double m_requestStartedSeconds; // Time an asynchronous fetch request is started

        // Max number of times that this DocumentThreadableLoader can follow
        // cross-origin redirects.
        // This is used to limit the number of redirects.
        // But this value is not the max number of total redirects allowed,
        // because same-origin redirects are not counted here.
        int m_corsRedirectLimit;

        WebURLRequest::FetchRedirectMode m_redirectMode;

        WeakPtrFactory<DocumentThreadableLoader> m_weakFactory;
    };

} // namespace blink

#endif // DocumentThreadableLoader_h
