// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_util.h"

#include <math.h>

#include "base/metrics/histogram.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"

namespace content {

// Compile asserts shared by all platforms.

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT( \
  static_cast<int>(blink::WebMediaPlayerClient::MediaKeyErrorCode ## name) == \
  static_cast<int>(media::MediaKeys::k ## name ## Error), \
  mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(Unknown);
COMPILE_ASSERT_MATCHING_ENUM(Client);
#undef COMPILE_ASSERT_MATCHING_ENUM

base::TimeDelta ConvertSecondsToTimestamp(double seconds) {
  double microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  return base::TimeDelta::FromMicroseconds(
      microseconds > 0 ? microseconds + 0.5 : ceil(microseconds - 0.5));
}

blink::WebTimeRanges ConvertToWebTimeRanges(
    const media::Ranges<base::TimeDelta>& ranges) {
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); ++i) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

blink::WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    media::PipelineStatus error) {
  DCHECK_NE(error, media::PIPELINE_OK);

  switch (error) {
    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
      return blink::WebMediaPlayer::NetworkStateNetworkError;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
    case media::PIPELINE_ERROR_COULD_NOT_RENDER:
    case media::PIPELINE_ERROR_URL_NOT_FOUND:
    case media::DEMUXER_ERROR_COULD_NOT_OPEN:
    case media::DEMUXER_ERROR_COULD_NOT_PARSE:
    case media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case media::DECODER_ERROR_NOT_SUPPORTED:
      return blink::WebMediaPlayer::NetworkStateFormatError;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_OPERATION_PENDING:
    case media::PIPELINE_ERROR_INVALID_STATE:
      return blink::WebMediaPlayer::NetworkStateDecodeError;

    case media::PIPELINE_ERROR_DECRYPT:
      // TODO(xhwang): Change to use NetworkStateDecryptError once it's added in
      // Webkit (see http://crbug.com/124486).
      return blink::WebMediaPlayer::NetworkStateDecodeError;

    case media::PIPELINE_OK:
      NOTREACHED() << "Unexpected status! " << error;
  }
  return blink::WebMediaPlayer::NetworkStateFormatError;
}

namespace {

// Helper enum for reporting scheme histograms.
enum URLSchemeForHistogram {
  kUnknownURLScheme,
  kMissingURLScheme,
  kHttpURLScheme,
  kHttpsURLScheme,
  kFtpURLScheme,
  kChromeExtensionURLScheme,
  kJavascriptURLScheme,
  kFileURLScheme,
  kBlobURLScheme,
  kDataURLScheme,
  kFileSystemScheme,
  kMaxURLScheme = kFileSystemScheme  // Must be equal to highest enum value.
};

URLSchemeForHistogram URLScheme(const GURL& url) {
  if (!url.has_scheme()) return kMissingURLScheme;
  if (url.SchemeIs("http")) return kHttpURLScheme;
  if (url.SchemeIs("https")) return kHttpsURLScheme;
  if (url.SchemeIs("ftp")) return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension")) return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript")) return kJavascriptURLScheme;
  if (url.SchemeIs("file")) return kFileURLScheme;
  if (url.SchemeIs("blob")) return kBlobURLScheme;
  if (url.SchemeIs("data")) return kDataURLScheme;
  if (url.SchemeIs("filesystem")) return kFileSystemScheme;

  return kUnknownURLScheme;
}

}  // namespace

void ReportMediaSchemeUma(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);
}

}  // namespace content
