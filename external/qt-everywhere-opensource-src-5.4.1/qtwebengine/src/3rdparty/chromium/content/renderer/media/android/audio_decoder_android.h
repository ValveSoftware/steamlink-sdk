// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_

#include "content/child/thread_safe_sender.h"

namespace blink {
class WebAudioBus;
}

namespace content {

bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                         const char* data,
                         size_t data_size,
                         scoped_refptr<ThreadSafeSender> sender);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_AUDIO_DECODER_ANDROID_H_
