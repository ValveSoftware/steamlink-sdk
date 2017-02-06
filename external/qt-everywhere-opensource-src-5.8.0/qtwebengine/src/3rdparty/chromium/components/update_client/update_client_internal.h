// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {

class Configurator;
class PingManager;
class Task;
struct TaskContext;
class UpdateEngine;

class UpdateClientImpl : public UpdateClient {
 public:
  UpdateClientImpl(const scoped_refptr<Configurator>& config,
                   std::unique_ptr<PingManager> ping_manager,
                   UpdateChecker::Factory update_checker_factory,
                   CrxDownloader::Factory crx_downloader_factory);

  // Overrides for UpdateClient.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Install(const std::string& id,
               const CrxDataCallback& crx_data_callback,
               const CompletionCallback& completion_callback) override;
  void Update(const std::vector<std::string>& ids,
              const CrxDataCallback& crx_data_callback,
              const CompletionCallback& completion_callback) override;
  bool GetCrxUpdateState(const std::string& id,
                         CrxUpdateItem* update_item) const override;
  bool IsUpdating(const std::string& id) const override;
  void Stop() override;
  void SendUninstallPing(const std::string& id,
                         const Version& version,
                         int reason) override;

 private:
  ~UpdateClientImpl() override;

  void RunTask(std::unique_ptr<Task> task);
  void OnTaskComplete(const CompletionCallback& completion_callback,
                      Task* task,
                      int error);

  void NotifyObservers(Observer::Events event, const std::string& id);

  base::ThreadChecker thread_checker_;

  // True is Stop method has been called.
  bool is_stopped_;

  scoped_refptr<Configurator> config_;

  // Contains the tasks that are pending. In the current implementation,
  // only update tasks (background tasks) are queued up. These tasks are
  // pending while they are in this queue. They have not been picked up yet
  // by the update engine.
  std::queue<Task*> task_queue_;

  // Contains all tasks in progress. These are the tasks that the update engine
  // is executing at one moment. Install tasks are run concurrently, update
  // tasks are always serialized, and update tasks are queued up if install
  // tasks are running. In addition, concurrent install tasks for the same id
  // are not allowed.
  std::set<Task*> tasks_;

  // TODO(sorin): try to make the ping manager an observer of the service.
  std::unique_ptr<PingManager> ping_manager_;
  std::unique_ptr<UpdateEngine> update_engine_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UpdateClientImpl);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_
