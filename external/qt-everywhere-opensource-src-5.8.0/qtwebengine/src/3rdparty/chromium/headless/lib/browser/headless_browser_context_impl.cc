// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_context_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "net/url_request/url_request_context.h"

namespace headless {

// Contains net::URLRequestContextGetter required for resource loading.
// Must be destructed on the IO thread as per content::ResourceContext
// requirements.
class HeadlessResourceContext : public content::ResourceContext {
 public:
  HeadlessResourceContext();
  ~HeadlessResourceContext() override;

  // ResourceContext implementation:
  net::HostResolver* GetHostResolver() override;
  net::URLRequestContext* GetRequestContext() override;

  // Configure the URL request context getter to be used for resource fetching.
  // Must be called before any of the other methods of this class are used. Must
  // be called on the browser UI thread.
  void set_url_request_context_getter(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_request_context_getter_ = std::move(url_request_context_getter);
  }

  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessResourceContext);
};

HeadlessResourceContext::HeadlessResourceContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

HeadlessResourceContext::~HeadlessResourceContext() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

net::HostResolver* HeadlessResourceContext::GetHostResolver() {
  CHECK(url_request_context_getter_);
  return url_request_context_getter_->GetURLRequestContext()->host_resolver();
}

net::URLRequestContext* HeadlessResourceContext::GetRequestContext() {
  CHECK(url_request_context_getter_);
  return url_request_context_getter_->GetURLRequestContext();
}

HeadlessBrowserContextImpl::HeadlessBrowserContextImpl(
    ProtocolHandlerMap protocol_handlers,
    HeadlessBrowser::Options* options)
    : protocol_handlers_(std::move(protocol_handlers)),
      options_(options),
      resource_context_(new HeadlessResourceContext) {
  InitWhileIOAllowed();
}

HeadlessBrowserContextImpl::~HeadlessBrowserContextImpl() {
  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }
}

// static
HeadlessBrowserContextImpl* HeadlessBrowserContextImpl::From(
    HeadlessBrowserContext* browser_context) {
  return reinterpret_cast<HeadlessBrowserContextImpl*>(browser_context);
}

void HeadlessBrowserContextImpl::InitWhileIOAllowed() {
  // TODO(skyostil): Allow the embedder to override this.
  PathService::Get(base::DIR_EXE, &path_);
  BrowserContext::Initialize(this, path_);
}

std::unique_ptr<content::ZoomLevelDelegate>
HeadlessBrowserContextImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::unique_ptr<content::ZoomLevelDelegate>();
}

base::FilePath HeadlessBrowserContextImpl::GetPath() const {
  return path_;
}

bool HeadlessBrowserContextImpl::IsOffTheRecord() const {
  return false;
}

content::ResourceContext* HeadlessBrowserContextImpl::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
HeadlessBrowserContextImpl::GetDownloadManagerDelegate() {
  return nullptr;
}

content::BrowserPluginGuestManager*
HeadlessBrowserContextImpl::GetGuestManager() {
  // TODO(altimin): Should be non-null? (is null in content/shell).
  return nullptr;
}

storage::SpecialStoragePolicy*
HeadlessBrowserContextImpl::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService*
HeadlessBrowserContextImpl::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate*
HeadlessBrowserContextImpl::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionManager* HeadlessBrowserContextImpl::GetPermissionManager() {
  return nullptr;
}

content::BackgroundSyncController*
HeadlessBrowserContextImpl::GetBackgroundSyncController() {
  return nullptr;
}

net::URLRequestContextGetter* HeadlessBrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  scoped_refptr<HeadlessURLRequestContextGetter> url_request_context_getter(
      new HeadlessURLRequestContextGetter(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE),
          protocol_handlers, std::move(protocol_handlers_),
          std::move(request_interceptors), options()));
  resource_context_->set_url_request_context_getter(url_request_context_getter);
  return url_request_context_getter.get();
}

net::URLRequestContextGetter*
HeadlessBrowserContextImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

net::URLRequestContextGetter*
HeadlessBrowserContextImpl::CreateMediaRequestContext() {
  return resource_context_->url_request_context_getter();
}

net::URLRequestContextGetter*
HeadlessBrowserContextImpl::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return nullptr;
}

void HeadlessBrowserContextImpl::SetOptionsForTesting(
    HeadlessBrowser::Options* options) {
  options_ = options;
}

HeadlessBrowserContext::Builder::Builder(HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessBrowserContext::Builder::~Builder() = default;

HeadlessBrowserContext::Builder::Builder(Builder&&) = default;

HeadlessBrowserContext::Builder&
HeadlessBrowserContext::Builder::SetProtocolHandlers(
    ProtocolHandlerMap protocol_handlers) {
  protocol_handlers_ = std::move(protocol_handlers);
  return *this;
}

std::unique_ptr<HeadlessBrowserContext>
HeadlessBrowserContext::Builder::Build() {
  return base::WrapUnique(new HeadlessBrowserContextImpl(
      std::move(protocol_handlers_), browser_->options()));
}

}  // namespace headless
