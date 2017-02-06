// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/component_updater/timer.h"

namespace base {
class TimeTicks;
}

namespace component_updater {

class OnDemandUpdater;

using CrxInstaller = update_client::CrxInstaller;
using UpdateClient = update_client::UpdateClient;

class CrxUpdateService : public ComponentUpdateService,
                         public ComponentUpdateService::Observer,
                         public OnDemandUpdater {
  using Observer = ComponentUpdateService::Observer;

 public:
  CrxUpdateService(const scoped_refptr<Configurator>& config,
                   const scoped_refptr<UpdateClient>& update_client);
  ~CrxUpdateService() override;

  // Overrides for ComponentUpdateService.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool RegisterComponent(const CrxComponent& component) override;
  bool UnregisterComponent(const std::string& id) override;
  std::vector<std::string> GetComponentIDs() const override;
  OnDemandUpdater& GetOnDemandUpdater() override;
  void MaybeThrottle(const std::string& id,
                     const base::Closure& callback) override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() override;
  bool GetComponentDetails(const std::string& id,
                           CrxUpdateItem* item) const override;

  // Overrides for Observer.
  void OnEvent(Events event, const std::string& id) override;

  // Overrides for OnDemandUpdater.
  bool OnDemandUpdate(const std::string& id) override;

 private:
  void Start();
  void Stop();

  bool CheckForUpdates();

  bool OnDemandUpdateInternal(const std::string& id);
  bool OnDemandUpdateWithCooldown(const std::string& id);

  bool DoUnregisterComponent(const CrxComponent& component);

  const CrxComponent* GetComponent(const std::string& id) const;

  const CrxUpdateItem* GetComponentState(const std::string& id) const;

  void OnUpdate(const std::vector<std::string>& ids,
                std::vector<CrxComponent>* components);
  void OnUpdateComplete(const base::TimeTicks& start_time, int error);

  base::ThreadChecker thread_checker_;

  scoped_refptr<Configurator> config_;

  scoped_refptr<UpdateClient> update_client_;

  Timer timer_;

  // A collection of every registered component.
  using Components = std::map<std::string, CrxComponent>;
  Components components_;

  // Maintains the order in which components have been registered. The position
  // of a component id in this sequence indicates the priority of the component.
  // The sooner the component gets registered, the higher its priority, and
  // the closer this component is to the beginning of the vector.
  std::vector<std::string> components_order_;

  // Contains the components pending unregistration. If a component is not
  // busy installing or updating, it can be unregistered right away. Otherwise,
  // the component will be lazily unregistered after the its operations have
  // completed.
  std::vector<std::string> components_pending_unregistration_;

  // Contains the active resource throttles associated with a given component.
  using ResourceThrottleCallbacks = std::multimap<std::string, base::Closure>;
  ResourceThrottleCallbacks ready_callbacks_;

  // Contains the state of the component.
  using ComponentStates = std::map<std::string, CrxUpdateItem>;
  ComponentStates component_states_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CrxUpdateService);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_
