// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBMIDIACCESSOR_IMPL_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBMIDIACCESSOR_IMPL_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessor.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessorClient.h"

namespace content {

class MidiMessageFilter;

class RendererWebMIDIAccessorImpl
    : public blink::WebMIDIAccessor {
 public:
  explicit RendererWebMIDIAccessorImpl(
      blink::WebMIDIAccessorClient* client);
  ~RendererWebMIDIAccessorImpl() override;

  // blink::WebMIDIAccessor implementation.
  void startSession() override;
  void sendMIDIData(unsigned port_index,
                    const unsigned char* data,
                    size_t length,
                    double timestamp) override;

 private:
  blink::WebMIDIAccessorClient* client_;

  MidiMessageFilter* midi_message_filter();

  DISALLOW_COPY_AND_ASSIGN(RendererWebMIDIAccessorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBMIDIACCESSOR_IMPL_H_
