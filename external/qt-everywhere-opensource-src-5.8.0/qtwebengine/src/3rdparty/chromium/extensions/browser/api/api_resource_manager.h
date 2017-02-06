// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_

#include <map>
#include <memory>

#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/threading/non_thread_safe.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"

namespace extensions {
class CastChannelAsyncApiFunction;

namespace api {
class BluetoothSocketApiFunction;
class BluetoothSocketEventDispatcher;
class SerialEventDispatcher;
class TCPServerSocketEventDispatcher;
class TCPSocketEventDispatcher;
class UDPSocketEventDispatcher;
}

template <typename T>
struct NamedThreadTraits {
  static bool IsCalledOnValidThread() {
    return content::BrowserThread::CurrentlyOn(T::kThreadId);
  }

  static bool IsMessageLoopValid() {
    return content::BrowserThread::IsMessageLoopValid(T::kThreadId);
  }

  static scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
    return content::BrowserThread::GetMessageLoopProxyForThread(T::kThreadId);
  }
};

// An ApiResourceManager manages the lifetime of a set of resources that
// that live on named threads (i.e. BrowserThread::IO) which ApiFunctions use.
// Examples of such resources are sockets or USB connections.
//
// Users of this class should define kThreadId to be the thread that
// ApiResourceManager to works on. The default is defined in ApiResource.
// The user must also define a static const char* service_name() that returns
// the name of the service, and in order for ApiResourceManager to use
// service_name() friend this class.
//
// In the cc file the user must define a GetFactoryInstance() and manage their
// own instances (typically using LazyInstance or Singleton).
//
// E.g.:
//
// class Resource {
//  public:
//   static const BrowserThread::ID kThreadId = BrowserThread::FILE;
//  private:
//   friend class ApiResourceManager<Resource>;
//   static const char* service_name() {
//     return "ResourceManager";
//    }
// };
//
// In the cc file:
//
// static base::LazyInstance<BrowserContextKeyedAPIFactory<
//     ApiResourceManager<Resource> > >
//         g_factory = LAZY_INSTANCE_INITIALIZER;
//
//
// template <>
// BrowserContextKeyedAPIFactory<ApiResourceManager<Resource> >*
// ApiResourceManager<Resource>::GetFactoryInstance() {
//   return g_factory.Pointer();
// }
template <class T, typename ThreadingTraits = NamedThreadTraits<T>>
class ApiResourceManager : public BrowserContextKeyedAPI,
                           public base::NonThreadSafe,
                           public ExtensionRegistryObserver,
                           public ProcessManagerObserver {
 public:
  explicit ApiResourceManager(content::BrowserContext* context)
      : data_(new ApiResourceData()),
        extension_registry_observer_(this),
        process_manager_observer_(this) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(context));
    process_manager_observer_.Add(ProcessManager::Get(context));
  }

  virtual ~ApiResourceManager() {
    DCHECK(CalledOnValidThread());
    DCHECK(ThreadingTraits::IsMessageLoopValid())
        << "A unit test is using an ApiResourceManager but didn't provide "
           "the thread message loop needed for that kind of resource. "
           "Please ensure that the appropriate message loop is operational.";

    data_->InititateCleanup();
  }

  // Takes ownership.
  int Add(T* api_resource) { return data_->Add(api_resource); }

  void Remove(const std::string& extension_id, int api_resource_id) {
    data_->Remove(extension_id, api_resource_id);
  }

  T* Get(const std::string& extension_id, int api_resource_id) {
    return data_->Get(extension_id, api_resource_id);
  }

  base::hash_set<int>* GetResourceIds(const std::string& extension_id) {
    return data_->GetResourceIds(extension_id);
  }

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ApiResourceManager<T> >*
      GetFactoryInstance();

  // Convenience method to get the ApiResourceManager for a profile.
  static ApiResourceManager<T>* Get(content::BrowserContext* context) {
    return BrowserContextKeyedAPIFactory<ApiResourceManager<T> >::Get(context);
  }

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return T::service_name(); }

  // Change the resource mapped to this |extension_id| at this
  // |api_resource_id| to |resource|. Returns true and succeeds unless
  // |api_resource_id| does not already identify a resource held by
  // |extension_id|.
  bool Replace(const std::string& extension_id,
               int api_resource_id,
               T* resource) {
    return data_->Replace(extension_id, api_resource_id, resource);
  }

 protected:
  // ProcessManagerObserver:
  void OnBackgroundHostClose(const std::string& extension_id) override {
    data_->InitiateExtensionSuspendedCleanup(extension_id);
  }

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override {
    data_->InitiateExtensionUnloadedCleanup(extension->id());
  }

 private:
  // TODO(rockot): ApiResourceData could be moved out of ApiResourceManager and
  // we could avoid maintaining a friends list here.
  friend class BluetoothAPI;
  friend class CastChannelAsyncApiFunction;
  friend class api::BluetoothSocketApiFunction;
  friend class api::BluetoothSocketEventDispatcher;
  friend class api::SerialEventDispatcher;
  friend class api::TCPServerSocketEventDispatcher;
  friend class api::TCPSocketEventDispatcher;
  friend class api::UDPSocketEventDispatcher;
  friend class BrowserContextKeyedAPIFactory<ApiResourceManager<T> >;

  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // ApiResourceData class handles resource bookkeeping on a thread
  // where resource lifetime is handled.
  class ApiResourceData : public base::RefCountedThreadSafe<ApiResourceData> {
   public:
    typedef std::map<int, linked_ptr<T> > ApiResourceMap;
    // Lookup map from extension id's to allocated resource id's.
    typedef std::map<std::string, base::hash_set<int> > ExtensionToResourceMap;

    ApiResourceData() : next_id_(1) {}

    int Add(T* api_resource) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      int id = GenerateId();
      if (id > 0) {
        linked_ptr<T> resource_ptr(api_resource);
        api_resource_map_[id] = resource_ptr;

        const std::string& extension_id = api_resource->owner_extension_id();
        ExtensionToResourceMap::iterator it =
            extension_resource_map_.find(extension_id);
        if (it == extension_resource_map_.end()) {
          it = extension_resource_map_.insert(
              std::make_pair(extension_id, base::hash_set<int>())).first;
        }
        it->second.insert(id);
        return id;
      }
      return 0;
    }

    void Remove(const std::string& extension_id, int api_resource_id) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      if (GetOwnedResource(extension_id, api_resource_id)) {
        ExtensionToResourceMap::iterator it =
            extension_resource_map_.find(extension_id);
        it->second.erase(api_resource_id);
        api_resource_map_.erase(api_resource_id);
      }
    }

    T* Get(const std::string& extension_id, int api_resource_id) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      return GetOwnedResource(extension_id, api_resource_id);
    }

    // Change the resource mapped to this |extension_id| at this
    // |api_resource_id| to |resource|. Returns true and succeeds unless
    // |api_resource_id| does not already identify a resource held by
    // |extension_id|.
    bool Replace(const std::string& extension_id,
                 int api_resource_id,
                 T* api_resource) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      T* old_resource = api_resource_map_[api_resource_id].get();
      if (old_resource && extension_id == old_resource->owner_extension_id()) {
        api_resource_map_[api_resource_id] = linked_ptr<T>(api_resource);
        return true;
      }
      return false;
    }

    base::hash_set<int>* GetResourceIds(const std::string& extension_id) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      return GetOwnedResourceIds(extension_id);
    }

    void InitiateExtensionUnloadedCleanup(const std::string& extension_id) {
      if (ThreadingTraits::IsCalledOnValidThread()) {
        CleanupResourcesFromUnloadedExtension(extension_id);
      } else {
        ThreadingTraits::GetSequencedTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ApiResourceData::CleanupResourcesFromUnloadedExtension,
                       this,
                       extension_id));
      }
    }

    void InitiateExtensionSuspendedCleanup(const std::string& extension_id) {
      if (ThreadingTraits::IsCalledOnValidThread()) {
        CleanupResourcesFromSuspendedExtension(extension_id);
      } else {
        ThreadingTraits::GetSequencedTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ApiResourceData::CleanupResourcesFromSuspendedExtension,
                       this,
                       extension_id));
      }
    }

    void InititateCleanup() {
      if (ThreadingTraits::IsCalledOnValidThread()) {
        Cleanup();
      } else {
        ThreadingTraits::GetSequencedTaskRunner()->PostTask(
            FROM_HERE, base::Bind(&ApiResourceData::Cleanup, this));
      }
    }

   private:
    friend class base::RefCountedThreadSafe<ApiResourceData>;

    virtual ~ApiResourceData() {}

    T* GetOwnedResource(const std::string& extension_id, int api_resource_id) {
      linked_ptr<T> ptr = api_resource_map_[api_resource_id];
      T* resource = ptr.get();
      if (resource && extension_id == resource->owner_extension_id())
        return resource;
      return NULL;
    }

    base::hash_set<int>* GetOwnedResourceIds(const std::string& extension_id) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());
      ExtensionToResourceMap::iterator it =
          extension_resource_map_.find(extension_id);
      if (it == extension_resource_map_.end())
        return NULL;
      return &(it->second);
    }

    void CleanupResourcesFromUnloadedExtension(
        const std::string& extension_id) {
      CleanupResourcesFromExtension(extension_id, true);
    }

    void CleanupResourcesFromSuspendedExtension(
        const std::string& extension_id) {
      CleanupResourcesFromExtension(extension_id, false);
    }

    void CleanupResourcesFromExtension(const std::string& extension_id,
                                       bool remove_all) {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());

      ExtensionToResourceMap::iterator it =
          extension_resource_map_.find(extension_id);
      if (it == extension_resource_map_.end())
        return;

      // Remove all resources, or the non persistent ones only if |remove_all|
      // is false.
      base::hash_set<int>& resource_ids = it->second;
      for (base::hash_set<int>::iterator it = resource_ids.begin();
           it != resource_ids.end();) {
        bool erase = false;
        if (remove_all) {
          erase = true;
        } else {
          linked_ptr<T> ptr = api_resource_map_[*it];
          T* resource = ptr.get();
          erase = (resource && !resource->IsPersistent());
        }

        if (erase) {
          api_resource_map_.erase(*it);
          resource_ids.erase(it++);
        } else {
          ++it;
        }
      }  // end for

      // Remove extension entry if we removed all its resources.
      if (resource_ids.size() == 0) {
        extension_resource_map_.erase(extension_id);
      }
    }

    void Cleanup() {
      DCHECK(ThreadingTraits::IsCalledOnValidThread());

      api_resource_map_.clear();
      extension_resource_map_.clear();
    }

    int GenerateId() { return next_id_++; }

    int next_id_;
    ApiResourceMap api_resource_map_;
    ExtensionToResourceMap extension_resource_map_;
  };

  content::NotificationRegistrar registrar_;
  scoped_refptr<ApiResourceData> data_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;
};

// With WorkerPoolThreadTraits, ApiResourceManager can be used to manage the
// lifetime of a set of resources that live on sequenced task runner threads
// which ApiFunctions use. Examples of such resources are temporary file
// resources produced by certain API calls.
//
// Instead of kThreadId. classes used for tracking such resources should define
// kSequenceToken and kShutdownBehavior to identify sequence task runner for
// ApiResourceManager to work on and how pending tasks should behave on
// shutdown.
// The user must also define a static const char* service_name() that returns
// the name of the service, and in order for ApiWorkerPoolResourceManager to use
// service_name() friend this class.
//
// In the cc file the user must define a GetFactoryInstance() and manage their
// own instances (typically using LazyInstance or Singleton).
//
// E.g.:
//
// class PoolResource {
//  public:
//   static const char kSequenceToken[] = "temp_files";
//   static const base::SequencedWorkerPool::WorkerShutdown kShutdownBehavior =
//       base::SequencedWorkerPool::BLOCK_SHUTDOWN;
//  private:
//   friend class ApiResourceManager<WorkerPoolResource,
//                                   WorkerPoolThreadTraits>;
//   static const char* service_name() {
//     return "TempFilesResourceManager";
//    }
// };
//
// In the cc file:
//
// static base::LazyInstance<BrowserContextKeyedAPIFactory<
//     ApiResourceManager<Resource, WorkerPoolThreadTraits> > >
//         g_factory = LAZY_INSTANCE_INITIALIZER;
//
//
// template <>
// BrowserContextKeyedAPIFactory<ApiResourceManager<WorkerPoolResource> >*
// ApiResourceManager<WorkerPoolPoolResource,
//                    WorkerPoolThreadTraits>::GetFactoryInstance() {
//   return g_factory.Pointer();
// }
template <typename T>
struct WorkerPoolThreadTraits {
  static bool IsCalledOnValidThread() {
    return content::BrowserThread::GetBlockingPool()
        ->IsRunningSequenceOnCurrentThread(
            content::BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
                T::kSequenceToken));
  }

  static bool IsMessageLoopValid() {
    return content::BrowserThread::GetBlockingPool() != NULL;
  }

  static scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
    return content::BrowserThread::GetBlockingPool()
        ->GetSequencedTaskRunnerWithShutdownBehavior(
            content::BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
                T::kSequenceToken),
            T::kShutdownBehavior);
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_
