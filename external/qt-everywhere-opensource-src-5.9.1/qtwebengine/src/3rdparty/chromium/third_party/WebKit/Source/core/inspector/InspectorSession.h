// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorSession_h
#define InspectorSession_h

#include "core/CoreExport.h"
#include "core/inspector/protocol/Forward.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

#include <v8-inspector-protocol.h>

namespace blink {

class InspectorAgent;
class InstrumentingAgents;
class LocalFrame;

class CORE_EXPORT InspectorSession
    : public GarbageCollectedFinalized<InspectorSession>,
      public protocol::FrontendChannel,
      public v8_inspector::V8Inspector::Channel {
  WTF_MAKE_NONCOPYABLE(InspectorSession);

 public:
  class Client {
   public:
    virtual void sendProtocolMessage(int sessionId,
                                     int callId,
                                     const String& response,
                                     const String& state) = 0;
    virtual ~Client() {}
  };

  InspectorSession(Client*,
                   InstrumentingAgents*,
                   int sessionId,
                   v8_inspector::V8Inspector*,
                   int contextGroupId,
                   const String* savedState);
  ~InspectorSession() override;
  int sessionId() { return m_sessionId; }
  v8_inspector::V8InspectorSession* v8Session() { return m_v8Session.get(); }

  void append(InspectorAgent*);
  void restore();
  void dispose();
  void didCommitLoadForLocalFrame(LocalFrame*);
  void dispatchProtocolMessage(const String& method, const String& message);
  void flushProtocolNotifications() override;

  DECLARE_TRACE();

 private:
  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(int callId, const String& message) override;
  void sendProtocolNotification(const String& message) override;

  // v8_inspector::V8Inspector::Channel implementation.
  void sendProtocolResponse(int callId,
                            const v8_inspector::StringView& message) override;
  void sendProtocolNotification(
      const v8_inspector::StringView& message) override;

  Client* m_client;
  std::unique_ptr<v8_inspector::V8InspectorSession> m_v8Session;
  int m_sessionId;
  bool m_disposed;
  Member<InstrumentingAgents> m_instrumentingAgents;
  std::unique_ptr<protocol::UberDispatcher> m_inspectorBackendDispatcher;
  std::unique_ptr<protocol::DictionaryValue> m_state;
  HeapVector<Member<InspectorAgent>> m_agents;
  Vector<String> m_notificationQueue;
  String m_lastSentState;
};

}  // namespace blink

#endif  // !defined(InspectorSession_h)
