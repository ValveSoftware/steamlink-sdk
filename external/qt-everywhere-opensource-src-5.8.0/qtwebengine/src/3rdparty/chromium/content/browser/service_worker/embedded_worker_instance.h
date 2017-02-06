// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/common/console_message_level.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

struct EmbeddedWorkerMsg_StartWorker_Params;

namespace IPC {
class Message;
}

namespace shell {
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

class EmbeddedWorkerRegistry;
class MessagePortMessageFilter;
class ServiceWorkerContextCore;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
class CONTENT_EXPORT EmbeddedWorkerInstance {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;

  // This enum is used in UMA histograms. Append-only.
  enum StartingPhase {
    NOT_STARTING,
    ALLOCATING_PROCESS,
    REGISTERING_TO_DEVTOOLS,
    SENT_START_WORKER,
    SCRIPT_DOWNLOADING,
    SCRIPT_LOADED,
    SCRIPT_EVALUATED,
    THREAD_STARTED,  // Happens after SCRIPT_LOADED and before SCRIPT_EVALUATED
    // Script read happens after SENT_START_WORKER and before SCRIPT_LOADED
    // (installed scripts only)
    SCRIPT_READ_STARTED,
    SCRIPT_READ_FINISHED,
    // Add new values here.
    STARTING_PHASE_MAX_VALUE,
  };

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnStarting() {}
    virtual void OnProcessAllocated() {}
    virtual void OnStartWorkerMessageSent() {}
    virtual void OnThreadStarted() {}
    virtual void OnStarted() {}

    virtual void OnStopping() {}
    // Received ACK from renderer that the worker context terminated.
    virtual void OnStopped(EmbeddedWorkerStatus old_status) {}
    // The browser-side IPC endpoint for communication with the worker died.
    virtual void OnDetached(EmbeddedWorkerStatus old_status) {}
    virtual void OnScriptLoaded() {}
    virtual void OnScriptLoadFailed() {}
    virtual void OnReportException(const base::string16& error_message,
                                   int line_number,
                                   int column_number,
                                   const GURL& source_url) {}
    virtual void OnReportConsoleMessage(int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    // Returns false if the message is not handled by this listener.
    CONTENT_EXPORT virtual bool OnMessageReceived(const IPC::Message& message);
  };

  ~EmbeddedWorkerInstance();

  // Starts the worker. It is invalid to call this when the worker is not in
  // STOPPED status. |callback| is invoked after the worker script has been
  // started and evaluated, or when an error occurs.
  // |params| should be populated with service worker version info needed
  // to start the worker.
  void Start(std::unique_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
             const StatusCallback& callback);

  // Stops the worker. It is invalid to call this when the worker is
  // not in STARTING or RUNNING status.
  // This returns false if stopping a worker fails immediately, e.g. when
  // IPC couldn't be sent to the worker.
  ServiceWorkerStatusCode Stop();

  // Stops the worker if the worker is not being debugged (i.e. devtools is
  // not attached). This method is called by a stop-worker timer to kill
  // idle workers.
  void StopIfIdle();

  // Sends |message| to the embedded worker running in the child process.
  // It is invalid to call this while the worker is not in STARTING or RUNNING
  // status.
  ServiceWorkerStatusCode SendMessage(const IPC::Message& message);

  // Resumes the worker if it paused after download.
  void ResumeAfterDownload();

  // Returns the shell::InterfaceRegistry and shell::InterfaceProvider for this
  // worker. It is invalid to call this when the worker is not in STARTING or
  // RUNNING status.
  shell::InterfaceRegistry* GetInterfaceRegistry();
  shell::InterfaceProvider* GetRemoteInterfaces();

  int embedded_worker_id() const { return embedded_worker_id_; }
  EmbeddedWorkerStatus status() const { return status_; }
  StartingPhase starting_phase() const {
    DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status());
    return starting_phase_;
  }
  int process_id() const;
  int thread_id() const { return thread_id_; }
  // This should be called only when the worker instance has a valid process,
  // that is, when |process_id()| returns a valid process id.
  bool is_new_process() const;
  int worker_devtools_agent_route_id() const;
  MessagePortMessageFilter* message_port_message_filter() const;

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  void set_devtools_attached(bool attached) { devtools_attached_ = attached; }
  bool devtools_attached() const { return devtools_attached_; }

  bool network_accessed_for_script() const {
    return network_accessed_for_script_;
  }

  ServiceWorkerMetrics::StartSituation start_situation() const {
    DCHECK(status() == EmbeddedWorkerStatus::STARTING ||
           status() == EmbeddedWorkerStatus::RUNNING);
    return start_situation_;
  }

  // Called when the main script load accessed the network.
  void OnNetworkAccessedForScriptLoad();

  // Called when reading the main script from the service worker script cache
  // begins and ends.
  void OnScriptReadStarted();
  void OnScriptReadFinished();

  // Called when the worker is installed.
  void OnWorkerVersionInstalled();

  // Called when the worker is doomed.
  void OnWorkerVersionDoomed();

  // Called when the net::URLRequestJob to load the service worker script
  // created. Not called for import scripts.
  void OnURLJobCreatedForMainScript();

  // Add message to the devtools console.
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message);

  static std::string StatusToString(EmbeddedWorkerStatus status);
  static std::string StartingPhaseToString(StartingPhase phase);

  void Detach();

  base::WeakPtr<EmbeddedWorkerInstance> AsWeakPtr();

 private:
  typedef base::ObserverList<Listener> ListenerList;
  class DevToolsProxy;
  class StartTask;
  class WorkerProcessHandle;
  friend class EmbeddedWorkerRegistry;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, DetachDuringStart);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StopDuringStart);

  // Constructor is called via EmbeddedWorkerRegistry::CreateWorker().
  // This instance holds a ref of |registry|.
  EmbeddedWorkerInstance(base::WeakPtr<ServiceWorkerContextCore> context,
                         int embedded_worker_id);

  // Called back from StartTask after a process is allocated on the UI thread.
  void OnProcessAllocated(std::unique_ptr<WorkerProcessHandle> handle,
                          ServiceWorkerMetrics::StartSituation start_situation);

  // Called back from StartTask after the worker is registered to
  // WorkerDevToolsManager.
  void OnRegisteredToDevToolsManager(bool is_new_process,
                                     int worker_devtools_agent_route_id,
                                     bool wait_for_debugger);

  // Called back from StartTask after a start worker message is sent.
  void OnStartWorkerMessageSent();

  // Called back from Registry when the worker instance has ack'ed that
  // it is ready for inspection.
  void OnReadyForInspection();

  // Called back from Registry when the worker instance has ack'ed that
  // it finished loading the script.
  void OnScriptLoaded();

  // Called back from Registry when the worker instance has ack'ed that
  // it has started a worker thread.
  void OnThreadStarted(int thread_id);

  // Called back from Registry when the worker instance has ack'ed that
  // it failed to load the script.
  void OnScriptLoadFailed();

  // Called back from Registry when the worker instance has ack'ed that
  // it finished evaluating the script. This is called before OnStarted.
  void OnScriptEvaluated(bool success);

  // Called back from Registry when the worker instance has ack'ed that its
  // WorkerGlobalScope has actually started and evaluated the script. This is
  // called after OnScriptEvaluated.
  // This will change the internal status from STARTING to RUNNING.
  void OnStarted();

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually stopped in the child process.
  // This will change the internal status from STARTING or RUNNING to
  // STOPPED.
  void OnStopped();

  // Called when ServiceWorkerDispatcherHost for the worker died while it was
  // running.
  void OnDetached();

  // Called back from Registry when the worker instance sends message
  // to the browser (i.e. EmbeddedWorker observers).
  // Returns false if the message is not handled.
  bool OnMessageReceived(const IPC::Message& message);

  // Called back from Registry when the worker instance reports the exception.
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);

  // Called back from Registry when the worker instance reports to the console.
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url);

  // Resets all running state. After this function is called, |status_| is
  // STOPPED.
  void ReleaseProcess();

  // Called back from StartTask when the startup sequence failed. Calls
  // ReleaseProcess() and invokes |callback| with |status|. May destroy |this|.
  void OnStartFailed(const StatusCallback& callback,
                     ServiceWorkerStatusCode status);

  // Returns the time elapsed since |step_time_| and updates |step_time_|
  // to the current time.
  base::TimeDelta UpdateStepTime();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<EmbeddedWorkerRegistry> registry_;
  const int embedded_worker_id_;
  EmbeddedWorkerStatus status_;
  StartingPhase starting_phase_;

  // Current running information.
  std::unique_ptr<EmbeddedWorkerInstance::WorkerProcessHandle> process_handle_;
  int thread_id_;
  std::unique_ptr<shell::InterfaceRegistry> interface_registry_;
  std::unique_ptr<shell::InterfaceProvider> remote_interfaces_;

  // Whether devtools is attached or not.
  bool devtools_attached_;

  // True if the script load request accessed the network. If the script was
  // served from HTTPCache or ServiceWorkerDatabase this value is false.
  bool network_accessed_for_script_;

  ListenerList listener_list_;
  std::unique_ptr<DevToolsProxy> devtools_proxy_;

  std::unique_ptr<StartTask> inflight_start_task_;

  // This is valid only after a process is allocated for the worker.
  ServiceWorkerMetrics::StartSituation start_situation_ =
      ServiceWorkerMetrics::StartSituation::UNKNOWN;

  // Used for UMA. The start time of the current start sequence step.
  base::TimeTicks step_time_;

  base::WeakPtrFactory<EmbeddedWorkerInstance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
