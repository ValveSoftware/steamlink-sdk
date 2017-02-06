// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/media_info_loader.h"

#include <utility>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLLoaderOptions;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

static const int kHttpOK = 200;
static const int kHttpPartialContentOK = 206;

MediaInfoLoader::MediaInfoLoader(
    const GURL& url,
    blink::WebMediaPlayer::CORSMode cors_mode,
    const ReadyCB& ready_cb)
    : loader_failed_(false),
      url_(url),
      allow_stored_credentials_(false),
      cors_mode_(cors_mode),
      single_origin_(true),
      ready_cb_(ready_cb) {}

MediaInfoLoader::~MediaInfoLoader() {}

void MediaInfoLoader::Start(blink::WebFrame* frame) {
  // Make sure we have not started.
  DCHECK(!ready_cb_.is_null());
  CHECK(frame);

  start_time_ = base::TimeTicks::Now();
  first_party_url_ = frame->document().firstPartyForCookies();

  // Prepare the request.
  WebURLRequest request(url_);
  // TODO(mkwst): Split this into video/audio.
  request.setRequestContext(WebURLRequest::RequestContextVideo);
  frame->setReferrerForRequest(request, blink::WebURL());

  // Since we don't actually care about the media data at this time, use a two
  // byte range request to avoid unnecessarily downloading resources.  Not all
  // servers support HEAD unfortunately, so use a range request; which is no
  // worse than the previous request+cancel code.  See http://crbug.com/400788
  request.addHTTPHeaderField("Range", "bytes=0-1");

  std::unique_ptr<WebURLLoader> loader;
  if (test_loader_) {
    loader = std::move(test_loader_);
  } else {
    WebURLLoaderOptions options;
    if (cors_mode_ == blink::WebMediaPlayer::CORSModeUnspecified) {
      options.allowCredentials = true;
      options.crossOriginRequestPolicy =
          WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
      allow_stored_credentials_ = true;
    } else {
      options.exposeAllResponseHeaders = true;
      // The author header set is empty, no preflight should go ahead.
      options.preflightPolicy = WebURLLoaderOptions::PreventPreflight;
      options.crossOriginRequestPolicy =
          WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;
      if (cors_mode_ == blink::WebMediaPlayer::CORSModeUseCredentials) {
        options.allowCredentials = true;
        allow_stored_credentials_ = true;
      }
    }
    loader.reset(frame->createAssociatedURLLoader(options));
  }

  // Start the resource loading.
  loader->loadAsynchronously(request, this);
  active_loader_.reset(new media::ActiveLoader(std::move(loader)));
}

/////////////////////////////////////////////////////////////////////////////
// blink::WebURLLoaderClient implementation.
void MediaInfoLoader::willFollowRedirect(
    WebURLLoader* loader,
    WebURLRequest& newRequest,
    const WebURLResponse& redirectResponse) {
  // The load may have been stopped and |ready_cb| is destroyed.
  // In this case we shouldn't do anything.
  if (ready_cb_.is_null()) {
    // Set the url in the request to an invalid value (empty url).
    newRequest.setURL(blink::WebURL());
    return;
  }

  // Only allow |single_origin_| if we haven't seen a different origin yet.
  if (single_origin_)
    single_origin_ = url_.GetOrigin() == GURL(newRequest.url()).GetOrigin();

  url_ = newRequest.url();
  first_party_url_ = newRequest.firstPartyForCookies();
  allow_stored_credentials_ = newRequest.allowStoredCredentials();
}

void MediaInfoLoader::didSendData(
    WebURLLoader* loader,
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::didReceiveResponse(
    WebURLLoader* loader,
    const WebURLResponse& response) {
  DVLOG(1) << "didReceiveResponse: HTTP/"
           << (response.httpVersion() == WebURLResponse::HTTPVersion_0_9
                   ? "0.9"
                   : response.httpVersion() == WebURLResponse::HTTPVersion_1_0
                         ? "1.0"
                         : response.httpVersion() ==
                                   WebURLResponse::HTTPVersion_1_1
                               ? "1.1"
                               : "Unknown")
           << " " << response.httpStatusCode();
  DCHECK(active_loader_.get());
  if (!url_.SchemeIs(url::kHttpScheme) && !url_.SchemeIs(url::kHttpsScheme)) {
      DidBecomeReady(kOk);
      return;
  }
  if (response.httpStatusCode() == kHttpOK ||
      response.httpStatusCode() == kHttpPartialContentOK) {
    DidBecomeReady(kOk);
    return;
  }
  loader_failed_ = true;
  DidBecomeReady(kFailed);
}

void MediaInfoLoader::didReceiveData(
    WebURLLoader* loader,
    const char* data,
    int data_length,
    int encoded_data_length) {
  // Ignored.
}

void MediaInfoLoader::didDownloadData(
    blink::WebURLLoader* loader,
    int dataLength,
    int encodedDataLength) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::didReceiveCachedMetadata(
    WebURLLoader* loader,
    const char* data,
    int data_length) {
  NOTIMPLEMENTED();
}

void MediaInfoLoader::didFinishLoading(
    WebURLLoader* loader,
    double finishTime,
    int64_t total_encoded_data_length) {
  DCHECK(active_loader_.get());
  DidBecomeReady(kOk);
}

void MediaInfoLoader::didFail(
    WebURLLoader* loader,
    const WebURLError& error) {
  DVLOG(1) << "didFail: reason=" << error.reason
           << ", isCancellation=" << error.isCancellation
           << ", domain=" << error.domain.utf8().data()
           << ", localizedDescription="
           << error.localizedDescription.utf8().data();
  DCHECK(active_loader_.get());
  loader_failed_ = true;
  DidBecomeReady(kFailed);
}

bool MediaInfoLoader::HasSingleOrigin() const {
  DCHECK(ready_cb_.is_null())
      << "Must become ready before calling HasSingleOrigin()";
  return single_origin_;
}

bool MediaInfoLoader::DidPassCORSAccessCheck() const {
  DCHECK(ready_cb_.is_null())
      << "Must become ready before calling DidPassCORSAccessCheck()";
  return !loader_failed_ &&
      cors_mode_ != blink::WebMediaPlayer::CORSModeUnspecified;
}

/////////////////////////////////////////////////////////////////////////////
// Helper methods.

void MediaInfoLoader::DidBecomeReady(Status status) {
  UMA_HISTOGRAM_TIMES("Media.InfoLoadDelay",
                      base::TimeTicks::Now() - start_time_);
  active_loader_.reset();
  if (!ready_cb_.is_null())
    base::ResetAndReturn(&ready_cb_).Run(status, url_, first_party_url_,
                                         allow_stored_credentials_);
}

}  // namespace content
