/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
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

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "core/CoreExport.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class Resource;
class ResourceError;
class ResourceFetcher;

class CORE_EXPORT ResourceLoader final
    : public GarbageCollectedFinalized<ResourceLoader>,
      protected WebURLLoaderClient {
  USING_PRE_FINALIZER(ResourceLoader, dispose);

 public:
  static ResourceLoader* create(ResourceFetcher*, Resource*);
  ~ResourceLoader() override;
  DECLARE_TRACE();

  void start(const ResourceRequest&,
             WebTaskRunner* loadingTaskRunner,
             bool defersLoading);

  // This method is currently only used for service worker fallback request and
  // cache-aware loading, other users should be careful not to break
  // ResourceLoader state.
  void restart(const ResourceRequest&,
               WebTaskRunner* loadingTaskRunner,
               bool defersLoading);

  void cancel();

  void setDefersLoading(bool);

  void didChangePriority(ResourceLoadPriority, int intraPriorityValue);

  // Called before start() to activate cache-aware loading if enabled in
  // |m_resource->options()| and applicable.
  void activateCacheAwareLoadingIfNeeded(const ResourceRequest&);

  bool isCacheAwareLoadingActivated() const {
    return m_isCacheAwareLoadingActivated;
  }

  // WebURLLoaderClient
  //
  // A succesful load will consist of:
  // 0+  willFollowRedirect()
  // 0+  didSendData()
  // 1   didReceiveResponse()
  // 0-1 didReceiveCachedMetadata()
  // 0+  didReceiveData() or didDownloadData(), but never both
  // 1   didFinishLoading()
  // A failed load is indicated by 1 didFail(), which can occur at any time
  // before didFinishLoading(), including synchronous inside one of the other
  // callbacks via ResourceLoader::cancel()
  bool willFollowRedirect(WebURLLoader*,
                          WebURLRequest&,
                          const WebURLResponse& redirectResponse) override;
  void didSendData(WebURLLoader*,
                   unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent) override;
  void didReceiveResponse(WebURLLoader*, const WebURLResponse&) override;
  void didReceiveResponse(WebURLLoader*,
                          const WebURLResponse&,
                          WebDataConsumerHandle*) override;
  void didReceiveCachedMetadata(WebURLLoader*,
                                const char* data,
                                int length) override;
  void didReceiveData(WebURLLoader*,
                      const char*,
                      int,
                      int encodedDataLength,
                      int encodedBodyLength) override;
  void didDownloadData(WebURLLoader*, int, int) override;
  void didFinishLoading(WebURLLoader*,
                        double finishTime,
                        int64_t encodedDataLength) override;
  void didFail(WebURLLoader*, const WebURLError&) override;

  void didFinishLoadingFirstPartInMultipart();
  void didFail(const ResourceError&);

 private:
  // Assumes ResourceFetcher and Resource are non-null.
  ResourceLoader(ResourceFetcher*, Resource*);

  void cancelForRedirectAccessCheckError(const KURL&);
  void requestSynchronously(const ResourceRequest&);
  void dispose();

  std::unique_ptr<WebURLLoader> m_loader;
  Member<ResourceFetcher> m_fetcher;
  Member<Resource> m_resource;
  bool m_isCacheAwareLoadingActivated;
};

}  // namespace blink

#endif
