// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/video_decoder_config_marshaller.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc_streamer/encryption_scheme_marshaller.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxExtraDataSize = 16 * 1024;

class SizeMarshaller {
 public:
  static void Write(const gfx::Size& size, MediaMessage* msg) {
    CHECK(msg->WritePod(size.width()));
    CHECK(msg->WritePod(size.height()));
  }

  static gfx::Size Read(MediaMessage* msg) {
    int w, h;
    CHECK(msg->ReadPod(&w));
    CHECK(msg->ReadPod(&h));
    return gfx::Size(w, h);
  }
};

class RectMarshaller {
 public:
  static void Write(const gfx::Rect& rect, MediaMessage* msg) {
    CHECK(msg->WritePod(rect.x()));
    CHECK(msg->WritePod(rect.y()));
    CHECK(msg->WritePod(rect.width()));
    CHECK(msg->WritePod(rect.height()));
  }

  static gfx::Rect Read(MediaMessage* msg) {
    int x, y, w, h;
    CHECK(msg->ReadPod(&x));
    CHECK(msg->ReadPod(&y));
    CHECK(msg->ReadPod(&w));
    CHECK(msg->ReadPod(&h));
    return gfx::Rect(x, y, w, h);
  }
};

}  // namespace

// static
void VideoDecoderConfigMarshaller::Write(
    const ::media::VideoDecoderConfig& config, MediaMessage* msg) {
  CHECK(msg->WritePod(config.codec()));
  CHECK(msg->WritePod(config.profile()));
  CHECK(msg->WritePod(config.format()));
  CHECK(msg->WritePod(config.color_space()));
  SizeMarshaller::Write(config.coded_size(), msg);
  RectMarshaller::Write(config.visible_rect(), msg);
  SizeMarshaller::Write(config.natural_size(), msg);
  EncryptionSchemeMarshaller::Write(config.encryption_scheme(), msg);
  CHECK(msg->WritePod(config.extra_data().size()));
  if (!config.extra_data().empty())
    CHECK(msg->WriteBuffer(&config.extra_data()[0],
                           config.extra_data().size()));
}

// static
::media::VideoDecoderConfig VideoDecoderConfigMarshaller::Read(
    MediaMessage* msg) {
  ::media::VideoCodec codec;
  ::media::VideoCodecProfile profile;
  ::media::VideoPixelFormat format;
  ::media::ColorSpace color_space;
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  size_t extra_data_size;
  ::media::EncryptionScheme encryption_scheme;
  std::vector<uint8_t> extra_data;

  CHECK(msg->ReadPod(&codec));
  CHECK(msg->ReadPod(&profile));
  CHECK(msg->ReadPod(&format));
  CHECK(msg->ReadPod(&color_space));
  coded_size = SizeMarshaller::Read(msg);
  visible_rect = RectMarshaller::Read(msg);
  natural_size = SizeMarshaller::Read(msg);
  encryption_scheme = EncryptionSchemeMarshaller::Read(msg);
  CHECK(msg->ReadPod(&extra_data_size));

  CHECK_GE(codec, ::media::kUnknownVideoCodec);
  CHECK_LE(codec, ::media::kVideoCodecMax);
  CHECK_GE(profile, ::media::VIDEO_CODEC_PROFILE_UNKNOWN);
  CHECK_LE(profile, ::media::VIDEO_CODEC_PROFILE_MAX);
  CHECK_GE(format, ::media::PIXEL_FORMAT_UNKNOWN);
  CHECK_LE(format, ::media::PIXEL_FORMAT_MAX);
  CHECK_GE(color_space, ::media::COLOR_SPACE_UNSPECIFIED);
  CHECK_LE(color_space, ::media::COLOR_SPACE_MAX);
  CHECK_LT(extra_data_size, kMaxExtraDataSize);
  if (extra_data_size > 0) {
    extra_data.resize(extra_data_size);
    CHECK(msg->ReadBuffer(&extra_data[0], extra_data.size()));
  }

  return ::media::VideoDecoderConfig(
      codec, profile, format, color_space,
      coded_size, visible_rect, natural_size,
      extra_data, encryption_scheme);
}

}  // namespace media
}  // namespace chromecast
