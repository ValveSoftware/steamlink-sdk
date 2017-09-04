/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamDescriptor_h
#define MediaStreamDescriptor_h

#include "platform/mediastream/MediaStreamComponent.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT MediaStreamDescriptorClient
    : public GarbageCollectedMixin {
 public:
  virtual ~MediaStreamDescriptorClient() {}

  virtual void streamEnded() = 0;
  virtual void addRemoteTrack(MediaStreamComponent*) = 0;
  virtual void removeRemoteTrack(MediaStreamComponent*) = 0;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

class PLATFORM_EXPORT MediaStreamDescriptor final
    : public GarbageCollectedFinalized<MediaStreamDescriptor> {
 public:
  class ExtraData {
    USING_FAST_MALLOC(ExtraData);

   public:
    virtual ~ExtraData() {}
  };

  // Only used for AudioDestinationNode.
  static MediaStreamDescriptor* create(
      const MediaStreamSourceVector& audioSources,
      const MediaStreamSourceVector& videoSources);

  static MediaStreamDescriptor* create(
      const MediaStreamComponentVector& audioComponents,
      const MediaStreamComponentVector& videoComponents);

  static MediaStreamDescriptor* create(
      const String& id,
      const MediaStreamComponentVector& audioComponents,
      const MediaStreamComponentVector& videoComponents);

  MediaStreamDescriptorClient* client() const { return m_client; }
  void setClient(MediaStreamDescriptorClient* client) { m_client = client; }

  String id() const { return m_id; }

  unsigned numberOfAudioComponents() const { return m_audioComponents.size(); }
  MediaStreamComponent* audioComponent(unsigned index) const {
    return m_audioComponents[index].get();
  }

  unsigned numberOfVideoComponents() const { return m_videoComponents.size(); }
  MediaStreamComponent* videoComponent(unsigned index) const {
    return m_videoComponents[index].get();
  }

  void addComponent(MediaStreamComponent*);
  void removeComponent(MediaStreamComponent*);

  void addRemoteTrack(MediaStreamComponent*);
  void removeRemoteTrack(MediaStreamComponent*);

  bool active() const { return m_active; }
  void setActive(bool active) { m_active = active; }

  ExtraData* getExtraData() const { return m_extraData.get(); }
  void setExtraData(std::unique_ptr<ExtraData> extraData) {
    m_extraData = std::move(extraData);
  }

  // |m_extraData| may hold pointers to GC objects, and it may touch them in
  // destruction.  So this class is eagerly finalized to finalize |m_extraData|
  // promptly.
  EAGERLY_FINALIZE();
  DECLARE_TRACE();

 private:
  MediaStreamDescriptor(const String& id,
                        const MediaStreamSourceVector& audioSources,
                        const MediaStreamSourceVector& videoSources);
  MediaStreamDescriptor(const String& id,
                        const MediaStreamComponentVector& audioComponents,
                        const MediaStreamComponentVector& videoComponents);

  Member<MediaStreamDescriptorClient> m_client;
  String m_id;
  HeapVector<Member<MediaStreamComponent>> m_audioComponents;
  HeapVector<Member<MediaStreamComponent>> m_videoComponents;
  bool m_active;

  std::unique_ptr<ExtraData> m_extraData;
};

typedef HeapVector<Member<MediaStreamDescriptor>> MediaStreamDescriptorVector;

}  // namespace blink

#endif  // MediaStreamDescriptor_h
