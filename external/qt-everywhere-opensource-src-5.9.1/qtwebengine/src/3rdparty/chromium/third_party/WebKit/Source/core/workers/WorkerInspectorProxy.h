// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerThread.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class Document;
class KURL;

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class CORE_EXPORT WorkerInspectorProxy final
    : public GarbageCollectedFinalized<WorkerInspectorProxy> {
 public:
  static WorkerInspectorProxy* create();

  ~WorkerInspectorProxy();
  DECLARE_TRACE();

  class CORE_EXPORT PageInspector {
   public:
    virtual ~PageInspector() {}
    virtual void dispatchMessageFromWorker(WorkerInspectorProxy*,
                                           const String&) = 0;
  };

  WorkerThreadStartMode workerStartMode(Document*);
  void workerThreadCreated(Document*, WorkerThread*, const KURL&);
  void workerThreadTerminated();
  void dispatchMessageFromWorker(const String&);
  void addConsoleMessageFromWorker(MessageLevel,
                                   const String& message,
                                   std::unique_ptr<SourceLocation>);

  void connectToInspector(PageInspector*);
  void disconnectFromInspector(PageInspector*);
  void sendMessageToInspector(const String&);
  void writeTimelineStartedEvent(const String& sessionId);

  const String& url() { return m_url; }
  Document* getDocument() { return m_document; }
  const String& inspectorId();

  using WorkerInspectorProxySet =
      PersistentHeapHashSet<WeakMember<WorkerInspectorProxy>>;
  static const WorkerInspectorProxySet& allProxies();

 private:
  WorkerInspectorProxy();

  WorkerThread* m_workerThread;
  Member<Document> m_document;
  PageInspector* m_pageInspector;
  String m_url;
  String m_inspectorId;
};

}  // namespace blink

#endif  // WorkerInspectorProxy_h
