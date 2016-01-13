// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/sounds_manager.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"
#include "media/audio/sounds/audio_stream_handler.h"

namespace media {

namespace {

SoundsManager* g_instance = NULL;
bool g_initialized_for_testing = false;

// SoundsManagerImpl ---------------------------------------------------

class SoundsManagerImpl : public SoundsManager {
 public:
  SoundsManagerImpl();
  virtual ~SoundsManagerImpl();

  // SoundsManager implementation:
  virtual bool Initialize(SoundKey key,
                          const base::StringPiece& data) OVERRIDE;
  virtual bool Play(SoundKey key) OVERRIDE;
  virtual base::TimeDelta GetDuration(SoundKey key) OVERRIDE;

 private:
  base::hash_map<SoundKey, linked_ptr<AudioStreamHandler> > handlers_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SoundsManagerImpl);
};

SoundsManagerImpl::SoundsManagerImpl()
    : task_runner_(AudioManager::Get()->GetTaskRunner()) {
}

SoundsManagerImpl::~SoundsManagerImpl() { DCHECK(CalledOnValidThread()); }

bool SoundsManagerImpl::Initialize(SoundKey key,
                                   const base::StringPiece& data) {
  if (handlers_.find(key) != handlers_.end() && handlers_[key]->IsInitialized())
    return true;
  linked_ptr<AudioStreamHandler> handler(new AudioStreamHandler(data));
  if (!handler->IsInitialized()) {
    LOG(WARNING) << "Can't initialize AudioStreamHandler for key=" << key;
    return false;
  }
  handlers_[key] = handler;
  return true;
}

bool SoundsManagerImpl::Play(SoundKey key) {
  DCHECK(CalledOnValidThread());
  if (handlers_.find(key) == handlers_.end() ||
      !handlers_[key]->IsInitialized()) {
    return false;
  }
  return handlers_[key]->Play();
}

base::TimeDelta SoundsManagerImpl::GetDuration(SoundKey key) {
  DCHECK(CalledOnValidThread());
  if (handlers_.find(key) == handlers_.end() ||
      !handlers_[key]->IsInitialized()) {
    return base::TimeDelta();
  }
  const WavAudioHandler& wav_audio = handlers_[key]->wav_audio_handler();
  return wav_audio.params().GetBufferDuration();
}

}  // namespace

SoundsManager::SoundsManager() {}

SoundsManager::~SoundsManager() { DCHECK(CalledOnValidThread()); }

// static
void SoundsManager::Create() {
  CHECK(!g_instance || g_initialized_for_testing)
      << "SoundsManager::Create() is called twice";
  if (g_initialized_for_testing)
    return;
  g_instance = new SoundsManagerImpl();
}

// static
void SoundsManager::Shutdown() {
  CHECK(g_instance) << "SoundsManager::Shutdown() is called "
                    << "without previous call to Create()";
  delete g_instance;
  g_instance = NULL;
}

// static
SoundsManager* SoundsManager::Get() {
  CHECK(g_instance) << "SoundsManager::Get() is called before Create()";
  return g_instance;
}

// static
void SoundsManager::InitializeForTesting(SoundsManager* manager) {
  CHECK(!g_instance) << "SoundsManager is already initialized.";
  CHECK(manager);
  g_instance = manager;
  g_initialized_for_testing = true;
}

}  // namespace media
