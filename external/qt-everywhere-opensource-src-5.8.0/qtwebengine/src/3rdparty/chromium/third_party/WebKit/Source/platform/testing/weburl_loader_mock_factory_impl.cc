// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/weburl_loader_mock_factory_impl.h"

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/PtrUtil.h"

namespace blink {

std::unique_ptr<WebURLLoaderMockFactory> WebURLLoaderMockFactory::create()
{
    return wrapUnique(new WebURLLoaderMockFactoryImpl);
}

WebURLLoaderMockFactoryImpl::WebURLLoaderMockFactoryImpl() {}

WebURLLoaderMockFactoryImpl::~WebURLLoaderMockFactoryImpl() {}

WebURLLoader* WebURLLoaderMockFactoryImpl::createURLLoader(
    WebURLLoader* default_loader) {
  DCHECK(default_loader);
  return new WebURLLoaderMock(this, default_loader);
}

void WebURLLoaderMockFactoryImpl::registerURL(const WebURL& url,
                                              const WebURLResponse& response,
                                              const WebString& file_path) {
  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.isNull() && !file_path.isEmpty()) {
    response_info.file_path = blink::WebStringToFilePath(file_path);
    DCHECK(base::PathExists(response_info.file_path))
        << response_info.file_path.MaybeAsASCII() << " does not exist.";
  }

  DCHECK(url_to_reponse_info_.find(url) == url_to_reponse_info_.end());
  url_to_reponse_info_.set(url, response_info);
}


void WebURLLoaderMockFactoryImpl::registerErrorURL(
    const WebURL& url,
    const WebURLResponse& response,
    const WebURLError& error) {
  DCHECK(url_to_reponse_info_.find(url) == url_to_reponse_info_.end());
  registerURL(url, response, WebString());
  url_to_error_info_.set(url, error);
}

void WebURLLoaderMockFactoryImpl::unregisterURL(const blink::WebURL& url) {
  URLToResponseMap::iterator iter = url_to_reponse_info_.find(url);
  DCHECK(iter != url_to_reponse_info_.end());
  url_to_reponse_info_.remove(iter);

  URLToErrorMap::iterator error_iter = url_to_error_info_.find(url);
  if (error_iter != url_to_error_info_.end())
    url_to_error_info_.remove(error_iter);
}

void WebURLLoaderMockFactoryImpl::unregisterAllURLs() {
  url_to_reponse_info_.clear();
  url_to_error_info_.clear();
}

void WebURLLoaderMockFactoryImpl::serveAsynchronousRequests() {
  // Serving a request might trigger more requests, so we cannot iterate on
  // pending_loaders_ as it might get modified.
  while (!pending_loaders_.isEmpty()) {
    LoaderToRequestMap::iterator iter = pending_loaders_.begin();
    WeakPtr<WebURLLoaderMock> loader(iter->key->GetWeakPtr());
    const WebURLRequest request = iter->value;
    pending_loaders_.remove(loader.get());

    WebURLResponse response;
    WebURLError error;
    WebData data;
    LoadRequest(request, &response, &error, &data);
    // Follow any redirects while the loader is still active.
    while (response.httpStatusCode() >= 300 &&
           response.httpStatusCode() < 400) {
      WebURLRequest newRequest = loader->ServeRedirect(request, response);
      if (!loader || loader->is_cancelled() || loader->is_deferred())
        break;
      LoadRequest(newRequest, &response, &error, &data);
    }
    // Serve the request if the loader is still active.
    if (loader && !loader->is_cancelled() && !loader->is_deferred())
      loader->ServeAsynchronousRequest(delegate_, response, data, error);
  }
  base::RunLoop().RunUntilIdle();
}

bool WebURLLoaderMockFactoryImpl::IsMockedURL(const blink::WebURL& url) {
  return url_to_reponse_info_.find(url) != url_to_reponse_info_.end();
}

void WebURLLoaderMockFactoryImpl::CancelLoad(WebURLLoaderMock* loader) {
  pending_loaders_.remove(loader);
}

void WebURLLoaderMockFactoryImpl::LoadSynchronously(
    const WebURLRequest& request,
    WebURLResponse* response,
    WebURLError* error,
    WebData* data) {
  LoadRequest(request, response, error, data);
}

void WebURLLoaderMockFactoryImpl::LoadAsynchronouly(
    const WebURLRequest& request,
    WebURLLoaderMock* loader) {
  DCHECK(!pending_loaders_.contains(loader));
  pending_loaders_.set(loader, request);
}

void WebURLLoaderMockFactoryImpl::LoadRequest(const WebURLRequest& request,
                                              WebURLResponse* response,
                                              WebURLError* error,
                                              WebData* data) {
  URLToErrorMap::const_iterator error_iter =
      url_to_error_info_.find(request.url());
  if (error_iter != url_to_error_info_.end())
    *error = error_iter->value;

  URLToResponseMap::const_iterator iter =
      url_to_reponse_info_.find(request.url());
  if (iter == url_to_reponse_info_.end()) {
    // Non mocked URLs should not have been passed to the default URLLoader.
    NOTREACHED();
    return;
  }

  if (!error->reason && !ReadFile(iter->value.file_path, data)) {
    NOTREACHED();
    return;
  }

  *response = iter->value.response;
}

// static
bool WebURLLoaderMockFactoryImpl::ReadFile(const base::FilePath& file_path,
                                           WebData* data) {
  // If the path is empty then we return an empty file so tests can simulate
  // requests without needing to actually load files.
  if (file_path.empty())
    return true;

  std::string buffer;
  if (!base::ReadFileToString(file_path, &buffer))
    return false;

  data->assign(buffer.data(), buffer.size());
  return true;
}

} // namespace blink
