// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/cdm_message_filter_android.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/media_features.h"

using content::BrowserThread;
using media::MediaDrmBridge;
using media::SupportedCodecs;

namespace cdm {

const size_t kMaxKeySystemLength = 256;

enum CodecType {
  CODEC_AUDIO,
  CODEC_VIDEO
};

struct CodecInfo {
  SupportedCodecs codec;
  CodecType codec_type;
  const char* codec_name;
  const char* container_mime_type;
};

const CodecInfo kCodecsToQuery[] = {
  {media::EME_CODEC_WEBM_OPUS, CODEC_AUDIO, "opus", "video/webm"},
  {media::EME_CODEC_WEBM_VORBIS, CODEC_AUDIO, "vorbis", "video/webm"},
  {media::EME_CODEC_WEBM_VP8, CODEC_VIDEO, "vp8", "video/webm"},
  {media::EME_CODEC_WEBM_VP9, CODEC_VIDEO, "vp9", "video/webm"},
#if defined(USE_PROPRIETARY_CODECS)
  {media::EME_CODEC_MP4_AAC, CODEC_AUDIO, "mp4a", "video/mp4"},
  {media::EME_CODEC_MP4_AVC1, CODEC_VIDEO, "avc1", "video/mp4"},
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  {media::EME_CODEC_MP4_HEVC, CODEC_VIDEO, "hvc1", "video/mp4"},
#endif
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static SupportedCodecs GetSupportedCodecs(
    const SupportedKeySystemRequest& request,
    bool video_must_be_compositable) {
  const std::string& key_system = request.key_system;
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  for (size_t i = 0; i < arraysize(kCodecsToQuery); ++i) {
    const CodecInfo& info = kCodecsToQuery[i];
    // TODO(qinmin): Remove the composition logic when secure contents can be
    // composited.
    bool is_secure = (info.codec_type == CODEC_VIDEO)
                         ? (!video_must_be_compositable) : false;
    if ((request.codecs & info.codec) &&
        MediaDrmBridge::IsKeySystemSupportedWithType(
            key_system, info.container_mime_type) &&
        media::MediaCodecUtil::CanDecode(info.codec_name, is_secure)) {
      supported_codecs |= info.codec;
    }
  }

  return supported_codecs;
}

CdmMessageFilterAndroid::CdmMessageFilterAndroid()
    : BrowserMessageFilter(EncryptedMediaMsgStart) {}

CdmMessageFilterAndroid::~CdmMessageFilterAndroid() {}

bool CdmMessageFilterAndroid::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CdmMessageFilterAndroid, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_QueryKeySystemSupport,
                        OnQueryKeySystemSupport)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPlatformKeySystemNames,
                        OnGetPlatformKeySystemNames)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CdmMessageFilterAndroid::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  // Move the IPC handling to FILE thread as it is not very cheap.
  if (message.type() == ChromeViewHostMsg_QueryKeySystemSupport::ID)
    *thread = BrowserThread::FILE;
}

void CdmMessageFilterAndroid::OnQueryKeySystemSupport(
    const SupportedKeySystemRequest& request,
    SupportedKeySystemResponse* response) {
  if (!response) {
    NOTREACHED() << "NULL response pointer provided.";
    return;
  }

  if (request.key_system.size() > kMaxKeySystemLength) {
    NOTREACHED() << "Invalid key system: " << request.key_system;
    return;
  }

  if (!MediaDrmBridge::IsKeySystemSupported(request.key_system))
    return;

  DCHECK(request.codecs & media::EME_CODEC_ALL) << "unrecognized codec";
  response->key_system = request.key_system;
  // TODO(qinmin): check composition is supported or not.
  response->compositing_codecs = GetSupportedCodecs(request, true);
  response->non_compositing_codecs = GetSupportedCodecs(request, false);
}

void CdmMessageFilterAndroid::OnGetPlatformKeySystemNames(
    std::vector<std::string>* key_systems) {
  *key_systems = MediaDrmBridge::GetPlatformKeySystemNames();
}

}  // namespace cdm
