// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_

#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/action.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {

class Configurator;
struct CrxUpdateItem;
struct UpdateContext;

// Handles updates for a group of components. Updates for different groups
// are run concurrently but within the same group of components, updates are
// applied one at a time.
class UpdateEngine {
 public:
  using CompletionCallback = base::Callback<void(int error)>;
  using NotifyObserversCallback =
      base::Callback<void(UpdateClient::Observer::Events event,
                          const std::string& id)>;
  using CrxDataCallback = UpdateClient::CrxDataCallback;

  UpdateEngine(const scoped_refptr<Configurator>& config,
               UpdateChecker::Factory update_checker_factory,
               CrxDownloader::Factory crx_downloader_factory,
               PingManager* ping_manager,
               const NotifyObserversCallback& notify_observers_callback);
  ~UpdateEngine();

  bool GetUpdateState(const std::string& id, CrxUpdateItem* update_state);

  void Update(bool is_foreground,
              const std::vector<std::string>& ids,
              const UpdateClient::CrxDataCallback& crx_data_callback,
              const CompletionCallback& update_callback);

 private:
  void UpdateComplete(UpdateContext* update_context, int error);

  // Returns true if the update engine rejects this update call because it
  // occurs too soon.
  bool IsThrottled(bool is_foreground) const;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Configurator> config_;

  UpdateChecker::Factory update_checker_factory_;
  CrxDownloader::Factory crx_downloader_factory_;

  // TODO(sorin): refactor as a ref counted class.
  PingManager* ping_manager_;  // Not owned by this class.

  std::unique_ptr<PersistedData> metadata_;

  // Called when CRX state changes occur.
  const NotifyObserversCallback notify_observers_callback_;

  // Contains the contexts associated with each update in progress.
  std::set<UpdateContext*> update_contexts_;

  // Implements a rate limiting mechanism for background update checks. Has the
  // effect of rejecting the update call if the update call occurs before
  // a certain time, which is negotiated with the server as part of the
  // update protocol. See the comments for X-Retry-After header.
  base::Time throttle_updates_until_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngine);
};

// TODO(sorin): consider making this a ref counted type.
struct UpdateContext {
  UpdateContext(
      const scoped_refptr<Configurator>& config,
      bool is_foreground,
      const std::vector<std::string>& ids,
      const UpdateClient::CrxDataCallback& crx_data_callback,
      const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
      const UpdateEngine::CompletionCallback& callback,
      UpdateChecker::Factory update_checker_factory,
      CrxDownloader::Factory crx_downloader_factory,
      PingManager* ping_manager);

  ~UpdateContext();

  scoped_refptr<Configurator> config;

  // True if this update has been initiated by the user.
  bool is_foreground;

  // Contains the ids of all CRXs in this context.
  const std::vector<std::string> ids;

  // Called before an update check, when update metadata is needed.
  const UpdateEngine::CrxDataCallback& crx_data_callback;

  // Called when there is a state change for any update in this context.
  const UpdateEngine::NotifyObserversCallback notify_observers_callback;

  // Called when the all updates associated with this context have completed.
  const UpdateEngine::CompletionCallback callback;

  // Posts replies back to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;

  // Runs tasks in a blocking thread pool.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;

  // Creates instances of UpdateChecker;
  UpdateChecker::Factory update_checker_factory;

  // Creates instances of CrxDownloader;
  CrxDownloader::Factory crx_downloader_factory;

  PingManager* ping_manager;  // Not owned by this class.

  std::unique_ptr<Action> current_action;

  // TODO(sorin): use a map instead of vector.
  std::vector<CrxUpdateItem*> update_items;

  // Contains the ids of the items to update.
  std::queue<std::string> queue;

  // The time in seconds to wait until doing further update checks.
  int retry_after_sec_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
