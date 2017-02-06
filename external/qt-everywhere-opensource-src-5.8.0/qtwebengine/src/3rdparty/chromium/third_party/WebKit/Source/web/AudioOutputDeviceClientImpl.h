// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioOutputDeviceClientImpl_h
#define AudioOutputDeviceClientImpl_h

#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class AudioOutputDeviceClientImpl : public GarbageCollectedFinalized<AudioOutputDeviceClientImpl>, public AudioOutputDeviceClient {
    USING_GARBAGE_COLLECTED_MIXIN(AudioOutputDeviceClientImpl);
    WTF_MAKE_NONCOPYABLE(AudioOutputDeviceClientImpl);
public:
    static AudioOutputDeviceClientImpl* create();

    ~AudioOutputDeviceClientImpl() override;

    // AudioOutputDeviceClient implementation.
    void checkIfAudioSinkExistsAndIsAuthorized(ExecutionContext*, const WebString& sinkId, std::unique_ptr<WebSetSinkIdCallbacks>) override;

    // GarbageCollectedFinalized implementation.
    DEFINE_INLINE_VIRTUAL_TRACE() { AudioOutputDeviceClient::trace(visitor); }

private:
    AudioOutputDeviceClientImpl();
};

} // namespace blink

#endif // AudioOutputDeviceClientImpl_h
