// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"

#ifndef NDEBUG
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#endif  // NDEBUG

DependencyManager::DependencyManager() {
}

DependencyManager::~DependencyManager() {
}

void DependencyManager::AddComponent(KeyedServiceBaseFactory* component) {
  dependency_graph_.AddNode(component);
}

void DependencyManager::RemoveComponent(KeyedServiceBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void DependencyManager::AddEdge(KeyedServiceBaseFactory* depended,
                                KeyedServiceBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void DependencyManager::RegisterPrefsForServices(
    base::SupportsUserData* context,
    user_prefs::PrefRegistrySyncable* pref_registry) {
  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

  for (auto* dependency_node : construction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->RegisterPrefsIfNecessaryForContext(context, pref_registry);
  }
}

void DependencyManager::CreateContextServices(base::SupportsUserData* context,
                                              bool is_testing_context) {
#ifndef NDEBUG
  MarkContextLiveForTesting(context);
#endif

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  for (auto* dependency_node : construction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    if (is_testing_context && factory->ServiceIsNULLWhileTesting() &&
        !factory->HasTestingFactory(context)) {
      factory->SetEmptyTestingFactory(context);
    } else if (factory->ServiceIsCreatedWithContext()) {
      factory->CreateServiceNow(context);
    }
  }
}

void DependencyManager::DestroyContextServices(
    base::SupportsUserData* context) {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpContextDependencies(context);
#endif

  for (auto* dependency_node : destruction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->ContextShutdown(context);
  }

#ifndef NDEBUG
  // The context is now dead to the rest of the program.
  dead_context_pointers_.insert(context);
#endif

  for (auto* dependency_node : destruction_order) {
    KeyedServiceBaseFactory* factory =
        static_cast<KeyedServiceBaseFactory*>(dependency_node);
    factory->ContextDestroyed(context);
  }
}

#ifndef NDEBUG
void DependencyManager::AssertContextWasntDestroyed(
    base::SupportsUserData* context) {
  if (dead_context_pointers_.find(context) != dead_context_pointers_.end()) {
    NOTREACHED() << "Attempted to access a context that was ShutDown(). "
                 << "This is most likely a heap smasher in progress. After "
                 << "KeyedService::Shutdown() completes, your service MUST "
                 << "NOT refer to depended services again.";
  }
}

void DependencyManager::MarkContextLiveForTesting(
    base::SupportsUserData* context) {
  dead_context_pointers_.erase(context);
}

namespace {

std::string KeyedServiceBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<KeyedServiceBaseFactory*>(node)->name();
}

}  // namespace

void DependencyManager::DumpDependenciesAsGraphviz(
    const std::string& top_level_name,
    const base::FilePath& dot_file) const {
  DCHECK(!dot_file.empty());
  std::string contents = dependency_graph_.DumpAsGraphviz(
      top_level_name, base::Bind(&KeyedServiceBaseFactoryGetNodeName));
  base::WriteFile(dot_file, contents.c_str(), contents.size());
}
#endif  // NDEBUG
