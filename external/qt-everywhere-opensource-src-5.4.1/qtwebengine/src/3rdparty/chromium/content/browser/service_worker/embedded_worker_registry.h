// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

struct EmbeddedWorkerMsg_StartWorker_Params;
class GURL;

namespace IPC {
class Message;
class Sender;
}

namespace content {

class EmbeddedWorkerInstance;
class ServiceWorkerContextCore;

// Acts as a thin stub between MessageFilter and each EmbeddedWorkerInstance,
// which sends/receives messages to/from each EmbeddedWorker in child process.
//
// Hangs off ServiceWorkerContextCore (its reference is also held by each
// EmbeddedWorkerInstance).  Operated only on IO thread.
class CONTENT_EXPORT EmbeddedWorkerRegistry
    : public NON_EXPORTED_BASE(base::RefCounted<EmbeddedWorkerRegistry>) {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;

  explicit EmbeddedWorkerRegistry(
      base::WeakPtr<ServiceWorkerContextCore> context);

  bool OnMessageReceived(const IPC::Message& message);

  // Creates and removes a new worker instance entry for bookkeeping.
  // This doesn't actually start or stop the worker.
  scoped_ptr<EmbeddedWorkerInstance> CreateWorker();

  // Called from EmbeddedWorkerInstance, relayed to the child process.
  void SendStartWorker(scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
                       const StatusCallback& callback,
                       int process_id);
  ServiceWorkerStatusCode StopWorker(int process_id,
                                     int embedded_worker_id);

  // Stop all active workers, even if they're handling events.
  void Shutdown();

  // Called back from EmbeddedWorker in the child process, relayed via
  // ServiceWorkerDispatcherHost.
  void OnWorkerScriptLoaded(int process_id, int embedded_worker_id);
  void OnWorkerScriptLoadFailed(int process_id, int embedded_worker_id);
  void OnWorkerStarted(int process_id, int thread_id, int embedded_worker_id);
  void OnWorkerStopped(int process_id, int embedded_worker_id);
  void OnReportException(int embedded_worker_id,
                         const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);
  void OnReportConsoleMessage(int embedded_worker_id,
                              int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url);

  // Keeps a map from process_id to sender information.
  void AddChildProcessSender(int process_id, IPC::Sender* sender);
  void RemoveChildProcessSender(int process_id);

  // Returns an embedded worker instance for given |embedded_worker_id|.
  EmbeddedWorkerInstance* GetWorker(int embedded_worker_id);

 private:
  friend class base::RefCounted<EmbeddedWorkerRegistry>;
  friend class EmbeddedWorkerInstance;

  typedef std::map<int, EmbeddedWorkerInstance*> WorkerInstanceMap;
  typedef std::map<int, IPC::Sender*> ProcessToSenderMap;

  ~EmbeddedWorkerRegistry();

  ServiceWorkerStatusCode Send(int process_id, IPC::Message* message);

  // RemoveWorker is called when EmbeddedWorkerInstance is destructed.
  // |process_id| could be invalid (i.e. -1) if it's not running.
  void RemoveWorker(int process_id, int embedded_worker_id);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  WorkerInstanceMap worker_map_;
  ProcessToSenderMap process_sender_map_;

  // Map from process_id to embedded_worker_id.
  // This map only contains running workers.
  std::map<int, std::set<int> > worker_process_map_;

  int next_embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
