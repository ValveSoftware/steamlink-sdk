// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webmidi/MIDIMessageEvent.h"

#include "modules/webmidi/MIDIMessageEventInit.h"

namespace blink {

MIDIMessageEvent::MIDIMessageEvent(const AtomicString& type,
                                   const MIDIMessageEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasData())
    m_data = initializer.data();
}

}  // namespace blink
