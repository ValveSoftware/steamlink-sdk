// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessageStorage_h
#define ConsoleMessageStorage_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class ConsoleMessage;
class ExecutionContext;

class CORE_EXPORT ConsoleMessageStorage
    : public GarbageCollected<ConsoleMessageStorage> {
  WTF_MAKE_NONCOPYABLE(ConsoleMessageStorage);

 public:
  ConsoleMessageStorage();

  void addConsoleMessage(ExecutionContext*, ConsoleMessage*);
  void clear();
  size_t size() const;
  ConsoleMessage* at(size_t index) const;
  int expiredCount() const;

  DECLARE_TRACE();

 private:
  int m_expiredCount;
  HeapDeque<Member<ConsoleMessage>> m_messages;
};

}  // namespace blink

#endif  // ConsoleMessageStorage_h
