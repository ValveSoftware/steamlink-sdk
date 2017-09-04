// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InProcessWorkerBase_h
#define InProcessWorkerBase_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/MessagePort.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/workers/AbstractWorker.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class InProcessWorkerMessagingProxy;
class WorkerScriptLoader;

// Base class for workers that operate in the same process as the document that
// creates them.
class CORE_EXPORT InProcessWorkerBase : public AbstractWorker,
                                        public ActiveScriptWrappable {
 public:
  ~InProcessWorkerBase() override;

  void postMessage(ExecutionContext*,
                   PassRefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);
  static bool canTransferArrayBuffer() { return true; }
  void terminate();

  // ActiveDOMObject
  void contextDestroyed() override;

  // ScriptWrappable
  bool hasPendingActivity() const final;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit InProcessWorkerBase(ExecutionContext*);
  bool initialize(ExecutionContext*, const String&, ExceptionState&);

  // Creates a proxy to allow communicating with the worker's global scope.
  // InProcessWorkerBase does not take ownership of the created proxy. The proxy
  // is expected to manage its own lifetime, and delete itself in response to
  // terminateWorkerGlobalScope().
  virtual InProcessWorkerMessagingProxy* createInProcessWorkerMessagingProxy(
      ExecutionContext*) = 0;

 private:
  // Callbacks for m_scriptLoader.
  void onResponse();
  void onFinished();

  RefPtr<WorkerScriptLoader> m_scriptLoader;

  // The proxy outlives the worker to perform thread shutdown.
  InProcessWorkerMessagingProxy* m_contextProxy;
};

}  // namespace blink

#endif  // InProcessWorkerBase_h
