// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/audio/fake_audio_log_factory.h"

namespace media {
namespace {
AudioManager* g_last_created = NULL;
}

// Forward declaration of the platform specific AudioManager factory function.
AudioManager* CreateAudioManager(AudioLogFactory* audio_log_factory);

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {
  CHECK(!g_last_created || g_last_created == this);
  g_last_created = NULL;
}

// static
AudioManager* AudioManager::Create(AudioLogFactory* audio_log_factory) {
  CHECK(!g_last_created);
  g_last_created = CreateAudioManager(audio_log_factory);
  return g_last_created;
}

// static
AudioManager* AudioManager::CreateForTesting() {
  static base::LazyInstance<FakeAudioLogFactory>::Leaky fake_log_factory =
      LAZY_INSTANCE_INITIALIZER;
  return Create(fake_log_factory.Pointer());
}

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

}  // namespace media
