// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediasource_impl.h"

#include "base/guid.h"
#include "content/renderer/media/websourcebuffer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

using ::blink::WebString;
using ::blink::WebMediaSource;

namespace content {

#define COMPILE_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaSource::webkit_name) == \
                 static_cast<int>(media::ChunkDemuxer::chromium_name),  \
                 mismatching_status_enums)
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusOk, kOk);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusNotSupported, kNotSupported);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(AddStatusReachedIdLimit, kReachedIdLimit);
#undef COMPILE_ASSERT_MATCHING_STATUS_ENUM

WebMediaSourceImpl::WebMediaSourceImpl(
    media::ChunkDemuxer* demuxer, media::LogCB log_cb)
    : demuxer_(demuxer),
      log_cb_(log_cb) {
  DCHECK(demuxer_);
}

WebMediaSourceImpl::~WebMediaSourceImpl() {}

WebMediaSource::AddStatus WebMediaSourceImpl::addSourceBuffer(
    const blink::WebString& type,
    const blink::WebVector<blink::WebString>& codecs,
    blink::WebSourceBuffer** source_buffer) {
  std::string id = base::GenerateGUID();
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i].utf8().data();

  WebMediaSource::AddStatus result =
      static_cast<WebMediaSource::AddStatus>(
          demuxer_->AddId(id, type.utf8().data(), new_codecs));

  if (result == WebMediaSource::AddStatusOk)
    *source_buffer = new WebSourceBufferImpl(id, demuxer_);

  return result;
}

double WebMediaSourceImpl::duration() {
  return demuxer_->GetDuration();
}

void WebMediaSourceImpl::setDuration(double new_duration) {
  DCHECK_GE(new_duration, 0);
  demuxer_->SetDuration(new_duration);
}

void WebMediaSourceImpl::markEndOfStream(
    WebMediaSource::EndOfStreamStatus status) {
  media::PipelineStatus pipeline_status = media::PIPELINE_OK;

  switch (status) {
    case WebMediaSource::EndOfStreamStatusNoError:
      break;
    case WebMediaSource::EndOfStreamStatusNetworkError:
      pipeline_status = media::PIPELINE_ERROR_NETWORK;
      break;
    case WebMediaSource::EndOfStreamStatusDecodeError:
      pipeline_status = media::PIPELINE_ERROR_DECODE;
      break;
  }

  demuxer_->MarkEndOfStream(pipeline_status);
}

void WebMediaSourceImpl::unmarkEndOfStream() {
  demuxer_->UnmarkEndOfStream();
}

}  // namespace content
