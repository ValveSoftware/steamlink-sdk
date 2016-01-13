// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmidiaccessor_impl.h"

#include "base/logging.h"
#include "content/renderer/media/midi_message_filter.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

RendererWebMIDIAccessorImpl::RendererWebMIDIAccessorImpl(
    blink::WebMIDIAccessorClient* client)
    : client_(client) {
  DCHECK(client_);
}

RendererWebMIDIAccessorImpl::~RendererWebMIDIAccessorImpl() {
  midi_message_filter()->RemoveClient(client_);
}

void RendererWebMIDIAccessorImpl::startSession() {
  midi_message_filter()->StartSession(client_);
}

void RendererWebMIDIAccessorImpl::sendMIDIData(
    unsigned port_index,
    const unsigned char* data,
    size_t length,
    double timestamp) {
  midi_message_filter()->SendMidiData(
      port_index,
      data,
      length,
      timestamp);
}

MidiMessageFilter* RendererWebMIDIAccessorImpl::midi_message_filter() {
  return RenderThreadImpl::current()->midi_message_filter();
}

}  // namespace content
