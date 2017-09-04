// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessage_h
#define ConsoleMessage_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SourceLocation;

class CORE_EXPORT ConsoleMessage final
    : public GarbageCollectedFinalized<ConsoleMessage> {
 public:
  // Location must be non-null.
  static ConsoleMessage* create(MessageSource,
                                MessageLevel,
                                const String& message,
                                std::unique_ptr<SourceLocation>);

  // Shortcut when location is unknown. Captures current location.
  static ConsoleMessage* create(MessageSource,
                                MessageLevel,
                                const String& message);

  // This method captures current location if available.
  static ConsoleMessage* createForRequest(MessageSource,
                                          MessageLevel,
                                          const String& message,
                                          const String& url,
                                          unsigned long requestIdentifier);

  // This creates message from WorkerMessageSource.
  static ConsoleMessage* createFromWorker(MessageLevel,
                                          const String& message,
                                          std::unique_ptr<SourceLocation>,
                                          const String& workerId);

  ~ConsoleMessage();

  SourceLocation* location() const;
  unsigned long requestIdentifier() const;
  double timestamp() const;
  MessageSource source() const;
  MessageLevel level() const;
  const String& message() const;
  const String& workerId() const;

  DECLARE_TRACE();

 private:
  ConsoleMessage(MessageSource,
                 MessageLevel,
                 const String& message,
                 std::unique_ptr<SourceLocation>);

  MessageSource m_source;
  MessageLevel m_level;
  String m_message;
  std::unique_ptr<SourceLocation> m_location;
  unsigned long m_requestIdentifier;
  double m_timestamp;
  String m_workerId;
};

}  // namespace blink

#endif  // ConsoleMessage_h
