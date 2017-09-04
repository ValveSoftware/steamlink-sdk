// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncEvent_h
#define SyncEvent_h

#include "modules/EventModules.h"
#include "modules/background_sync/SyncEventInit.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT SyncEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SyncEvent* create(const AtomicString& type,
                           const String& tag,
                           bool lastChance,
                           WaitUntilObserver* observer) {
    return new SyncEvent(type, tag, lastChance, observer);
  }
  static SyncEvent* create(const AtomicString& type,
                           const SyncEventInit& init) {
    return new SyncEvent(type, init);
  }

  ~SyncEvent() override;

  const AtomicString& interfaceName() const override;

  String tag();
  bool lastChance();

  DECLARE_VIRTUAL_TRACE();

 private:
  SyncEvent(const AtomicString& type, const String&, bool, WaitUntilObserver*);
  SyncEvent(const AtomicString& type, const SyncEventInit&);

  String m_tag;
  bool m_lastChance;
};

}  // namespace blink

#endif  // SyncEvent_h
