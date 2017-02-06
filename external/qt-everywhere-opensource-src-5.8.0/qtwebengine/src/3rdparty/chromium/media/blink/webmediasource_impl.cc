// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediasource_impl.h"

#include "base/guid.h"
#include "media/base/mime_util.h"
#include "media/blink/websourcebuffer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/WebKit/public/platform/WebString.h"

using ::blink::WebString;
using ::blink::WebMediaSource;

namespace media {

#define STATIC_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
  static_assert(static_cast<int>(WebMediaSource::webkit_name) == \
                static_cast<int>(ChunkDemuxer::chromium_name),  \
                "mismatching status enum values: " #webkit_name)
STATIC_ASSERT_MATCHING_STATUS_ENUM(AddStatusOk, kOk);
STATIC_ASSERT_MATCHING_STATUS_ENUM(AddStatusNotSupported, kNotSupported);
STATIC_ASSERT_MATCHING_STATUS_ENUM(AddStatusReachedIdLimit, kReachedIdLimit);
#undef STATIC_ASSERT_MATCHING_STATUS_ENUM

WebMediaSourceImpl::WebMediaSourceImpl(ChunkDemuxer* demuxer,
                                       const scoped_refptr<MediaLog>& media_log)
    : demuxer_(demuxer), media_log_(media_log) {
  DCHECK(demuxer_);
}

WebMediaSourceImpl::~WebMediaSourceImpl() {}

WebMediaSource::AddStatus WebMediaSourceImpl::addSourceBuffer(
    const blink::WebString& type,
    const blink::WebString& codecs,
    blink::WebSourceBuffer** source_buffer) {
  std::string id = base::GenerateGUID();

  std::vector<std::string> parsed_codec_ids;
  media::ParseCodecString(codecs.utf8().data(), &parsed_codec_ids, false);

  WebMediaSource::AddStatus result =
      static_cast<WebMediaSource::AddStatus>(
          demuxer_->AddId(id, type.utf8().data(), parsed_codec_ids));

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
  PipelineStatus pipeline_status = PIPELINE_OK;

  switch (status) {
    case WebMediaSource::EndOfStreamStatusNoError:
      break;
    case WebMediaSource::EndOfStreamStatusNetworkError:
      pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
      break;
    case WebMediaSource::EndOfStreamStatusDecodeError:
      pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
      break;
  }

  demuxer_->MarkEndOfStream(pipeline_status);
}

void WebMediaSourceImpl::unmarkEndOfStream() {
  demuxer_->UnmarkEndOfStream();
}

}  // namespace media
