/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/audio/HRTFDatabaseLoader.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"

namespace blink {

using LoaderMap = HashMap<double, HRTFDatabaseLoader*>;

// getLoaderMap() returns the static hash map that contains the mapping between
// the sample rate and the corresponding HRTF database.
static LoaderMap& getLoaderMap() {
  DEFINE_STATIC_LOCAL(LoaderMap*, map, (new LoaderMap));
  return *map;
}

PassRefPtr<HRTFDatabaseLoader>
HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(float sampleRate) {
  ASSERT(isMainThread());

  RefPtr<HRTFDatabaseLoader> loader = getLoaderMap().get(sampleRate);
  if (loader) {
    ASSERT(sampleRate == loader->databaseSampleRate());
    return loader.release();
  }

  loader = adoptRef(new HRTFDatabaseLoader(sampleRate));
  getLoaderMap().add(sampleRate, loader.get());
  loader->loadAsynchronously();
  return loader.release();
}

HRTFDatabaseLoader::HRTFDatabaseLoader(float sampleRate)
    : m_databaseSampleRate(sampleRate) {
  ASSERT(isMainThread());
}

HRTFDatabaseLoader::~HRTFDatabaseLoader() {
  ASSERT(isMainThread());
  ASSERT(!m_thread);
  getLoaderMap().remove(m_databaseSampleRate);
}

void HRTFDatabaseLoader::loadTask() {
  DCHECK(!isMainThread());
  DCHECK(!m_hrtfDatabase);

  // Protect access to m_hrtfDatabase, which can be accessed from the audio
  // thread.
  MutexLocker locker(m_lock);
  // Load the default HRTF database.
  m_hrtfDatabase = HRTFDatabase::create(m_databaseSampleRate);
}

void HRTFDatabaseLoader::loadAsynchronously() {
  ASSERT(isMainThread());

  // m_hrtfDatabase and m_thread should both be unset because this should be a
  // new HRTFDatabaseLoader object that was just created by
  // createAndLoadAsynchronouslyIfNecessary and because we haven't started
  // loadTask yet for this object.
  DCHECK(!m_hrtfDatabase);
  DCHECK(!m_thread);

  // Start the asynchronous database loading process.
  m_thread =
      wrapUnique(Platform::current()->createThread("HRTF database loader"));
  // TODO(alexclarke): Should this be posted as a loading task?
  m_thread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, crossThreadBind(&HRTFDatabaseLoader::loadTask,
                                       crossThreadUnretained(this)));
}

HRTFDatabase* HRTFDatabaseLoader::database() {
  DCHECK(!isMainThread());

  // Seeing that this is only called from the audio thread, we can't block.
  // It's ok to return nullptr if we can't get the lock.
  MutexTryLocker tryLocker(m_lock);

  if (!tryLocker.locked())
    return nullptr;

  return m_hrtfDatabase.get();
}

// This cleanup task is needed just to make sure that the loader thread finishes
// the load task and thus the loader thread doesn't touch m_thread any more.
void HRTFDatabaseLoader::cleanupTask(WaitableEvent* sync) {
  sync->signal();
}

void HRTFDatabaseLoader::waitForLoaderThreadCompletion() {
  if (!m_thread)
    return;

  WaitableEvent sync;
  // TODO(alexclarke): Should this be posted as a loading task?
  m_thread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, crossThreadBind(&HRTFDatabaseLoader::cleanupTask,
                                       crossThreadUnretained(this),
                                       crossThreadUnretained(&sync)));
  sync.wait();
  m_thread.reset();
}

}  // namespace blink
