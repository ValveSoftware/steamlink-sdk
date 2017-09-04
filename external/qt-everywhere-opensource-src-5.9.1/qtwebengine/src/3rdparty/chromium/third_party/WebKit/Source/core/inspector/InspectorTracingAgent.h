/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef InspectorTracingAgent_h
#define InspectorTracingAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Tracing.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;
class InspectorWorkerAgent;

class CORE_EXPORT InspectorTracingAgent final
    : public InspectorBaseAgent<protocol::Tracing::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void enableTracing(const String& categoryFilter) = 0;
    virtual void disableTracing() = 0;
    virtual void showReloadingBlanket() = 0;
    virtual void hideReloadingBlanket() = 0;
  };

  static InspectorTracingAgent* create(Client* client,
                                       InspectorWorkerAgent* workerAgent,
                                       InspectedFrames* inspectedFrames) {
    return new InspectorTracingAgent(client, workerAgent, inspectedFrames);
  }

  DECLARE_VIRTUAL_TRACE();

  // Base agent methods.
  void restore() override;
  Response disable() override;

  // InspectorInstrumentation methods
  void frameStartedLoading(LocalFrame*);
  void frameStoppedLoading(LocalFrame*);

  // Protocol method implementations.
  void start(Maybe<String> categories,
             Maybe<String> options,
             Maybe<double> bufferUsageReportingInterval,
             Maybe<String> transferMode,
             Maybe<protocol::Tracing::TraceConfig>,
             std::unique_ptr<StartCallback>) override;
  void end(std::unique_ptr<EndCallback>) override;

  // Methods for other agents to use.
  void setLayerTreeId(int);
  void rootLayerCleared();

 private:
  InspectorTracingAgent(Client*, InspectorWorkerAgent*, InspectedFrames*);

  void emitMetadataEvents();
  void innerDisable();
  String sessionId() const;
  bool isStarted() const;

  int m_layerTreeId;
  Client* m_client;
  Member<InspectorWorkerAgent> m_workerAgent;
  Member<InspectedFrames> m_inspectedFrames;
};

}  // namespace blink

#endif  // InspectorTracingAgent_h
