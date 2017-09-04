// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INSTANCE_HOLDER_H_
#define COMPONENTS_ARC_INSTANCE_HOLDER_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace arc {

// Holds a Mojo instance+version pair. This also allows for listening for state
// changes for the particular instance. T should be a Mojo interface type
// (arc::mojom::XxxInstance).
template <typename T>
class InstanceHolder {
 public:
  // Notifies about connection events for individual instances.
  class Observer {
   public:
    // Called once the instance is ready.
    virtual void OnInstanceReady() {}

    // Called when the connection to the instance is closed.
    virtual void OnInstanceClosed() {}

   protected:
    virtual ~Observer() = default;
  };

  InstanceHolder() = default;

  // Returns true if the Mojo interface is ready at least for its version 0
  // interface. Use an Observer if you want to be notified when this is ready.
  // This can only be called on the thread that this class was created on.
  bool has_instance() const { return instance_; }

  // Gets the Mojo interface that's intended to call for
  // |method_name_for_logging|, but only if its reported version is at least
  // |min_version|. Returns nullptr if the instance is either not ready or does
  // not have the requested version, and logs appropriately.
  // TODO(lhchavez): Improve the API. (crbug.com/649782)
  T* GetInstanceForMethod(const std::string& method_name_for_logging,
                          uint32_t min_version) {
    if (!instance_) {
      VLOG(1) << "Instance for " << T::Name_ << "::" << method_name_for_logging
              << " not available.";
      return nullptr;
    }
    if (version_ < min_version) {
      LOG(ERROR) << "Instance for " << T::Name_
                 << "::" << method_name_for_logging
                 << " version mismatch. Expected " << min_version << " got "
                 << version_;
      return nullptr;
    }
    return instance_;
  }

  // Same as the above, but for the version zero.
  T* GetInstanceForMethod(const std::string& method_name_for_logging) {
    return GetInstanceForMethod(method_name_for_logging, 0u);
  }

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer) {
    DCHECK(thread_checker_.CalledOnValidThread());
    observer_list_.AddObserver(observer);

    if (instance_)
      observer->OnInstanceReady();
  }

  void RemoveObserver(Observer* observer) {
    DCHECK(thread_checker_.CalledOnValidThread());
    observer_list_.RemoveObserver(observer);
  }

  // Sets |instance| with |version|.
  // This can be called in both case; on ready, and on closed.
  // Passing nullptr to |instance| means closing.
  void SetInstance(T* instance, uint32_t version = T::Version_) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(instance == nullptr || instance_ == nullptr);

    // Note: This can be called with nullptr even if |instance_| is still
    // nullptr for just in case clean up purpose. No-op in such a case.
    if (instance == instance_)
      return;

    instance_ = instance;
    version_ = version;
    if (instance_) {
      for (auto& observer : observer_list_)
        observer.OnInstanceReady();
    } else {
      for (auto& observer : observer_list_)
        observer.OnInstanceClosed();
    }
  }

 private:
  // This class does not have ownership. The pointers should be managed by the
  // callee.
  T* instance_ = nullptr;
  uint32_t version_ = 0;

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(InstanceHolder<T>);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INSTANCE_HOLDER_H_
