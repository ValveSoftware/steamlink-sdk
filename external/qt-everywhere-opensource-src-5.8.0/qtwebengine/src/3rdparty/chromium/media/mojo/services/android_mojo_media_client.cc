// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_media_client.h"

#include "base/memory/ptr_util.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "services/shell/public/cpp/connect.h"

namespace media {

namespace {

std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    shell::mojom::InterfaceProvider* interface_provider) {
  mojom::ProvisionFetcherPtr provision_fetcher_ptr;
  shell::GetInterface(interface_provider, &provision_fetcher_ptr);
  return base::WrapUnique(
      new MojoProvisionFetcher(std::move(provision_fetcher_ptr)));
}

}  // namespace

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.

std::unique_ptr<AudioDecoder> AndroidMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new MediaCodecAudioDecoder(task_runner));
}

std::unique_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    shell::mojom::InterfaceProvider* interface_provider) {
  return base::WrapUnique(new AndroidCdmFactory(
      base::Bind(&CreateProvisionFetcher, interface_provider)));
}

}  // namespace media
