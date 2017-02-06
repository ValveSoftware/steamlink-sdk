// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/decrypt_config_marshaller.h"

#include <stddef.h>

#include "base/logging.h"
#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/public/media/cast_decrypt_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxKeyIdSize = 256;
const size_t kMaxIvSize = 256;
const size_t kMaxSubsampleCount = 1024;
}

// static
void DecryptConfigMarshaller::Write(const CastDecryptConfig& config,
                                    MediaMessage* msg) {
  CHECK_GT(config.key_id().size(), 0u);
  CHECK_GT(config.iv().size(), 0u);
  CHECK_GT(config.subsamples().size(), 0u);

  CHECK(msg->WritePod(config.key_id().size()));
  CHECK(msg->WriteBuffer(config.key_id().data(), config.key_id().size()));
  CHECK(msg->WritePod(config.iv().size()));
  CHECK(msg->WriteBuffer(config.iv().data(), config.iv().size()));
  CHECK(msg->WritePod(config.subsamples().size()));
  for (size_t k = 0; k < config.subsamples().size(); k++) {
    CHECK(msg->WritePod(config.subsamples()[k].clear_bytes));
    CHECK(msg->WritePod(config.subsamples()[k].cypher_bytes));
  }
}

// static
std::unique_ptr<CastDecryptConfig> DecryptConfigMarshaller::Read(
    MediaMessage* msg) {
  size_t key_id_size = 0;
  CHECK(msg->ReadPod(&key_id_size));
  CHECK_GT(key_id_size, 0u);
  CHECK_LT(key_id_size, kMaxKeyIdSize);
  std::unique_ptr<char[]> key_id(new char[key_id_size]);
  CHECK(msg->ReadBuffer(key_id.get(), key_id_size));

  size_t iv_size = 0;
  CHECK(msg->ReadPod(&iv_size));
  CHECK_GT(iv_size, 0u);
  CHECK_LT(iv_size, kMaxIvSize);
  std::unique_ptr<char[]> iv(new char[iv_size]);
  CHECK(msg->ReadBuffer(iv.get(), iv_size));

  size_t subsample_count = 0;
  CHECK(msg->ReadPod(&subsample_count));
  CHECK_GT(subsample_count, 0u);
  CHECK_LT(subsample_count, kMaxSubsampleCount);
  std::vector<SubsampleEntry> subsamples(subsample_count);
  for (size_t k = 0; k < subsample_count; k++) {
    subsamples[k].clear_bytes = 0;
    subsamples[k].cypher_bytes = 0;
    CHECK(msg->ReadPod(&subsamples[k].clear_bytes));
    CHECK(msg->ReadPod(&subsamples[k].cypher_bytes));
  }

  return std::unique_ptr<CastDecryptConfig>(
      new CastDecryptConfigImpl(std::string(key_id.get(), key_id_size),
                                std::string(iv.get(), iv_size), subsamples));
}

}  // namespace media
}  // namespace chromecast
