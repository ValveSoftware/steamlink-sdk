// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INSTANCE_HOLDER_H_
#define COMPONENTS_ARC_INSTANCE_HOLDER_H_

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

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

  InstanceHolder() : weak_factory_(this) {}

  // Gets the Mojo interface for all the instance services. This will return
  // nullptr if that particular service is not ready yet. Use an Observer if you
  // want to be notified when this is ready. This can only be called on the
  // thread that this class was created on.
  T* instance() const { return raw_ptr_; }
  uint32_t version() const { return version_; }

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer) {
    DCHECK(thread_checker_.CalledOnValidThread());
    observer_list_.AddObserver(observer);

    if (instance())
      observer->OnInstanceReady();
  }

  void RemoveObserver(Observer* observer) {
    DCHECK(thread_checker_.CalledOnValidThread());
    observer_list_.RemoveObserver(observer);
  }

  // Called when the channel is closed.
  void CloseChannel() {
    if (!raw_ptr_)
      return;

    ptr_.reset();
    raw_ptr_ = nullptr;
    version_ = 0;
    if (observer_list_.might_have_observers()) {
      typename base::ObserverList<Observer>::Iterator it(&observer_list_);
      Observer* obs;
      while ((obs = it.GetNext()) != nullptr)
        obs->OnInstanceClosed();
    }
  }

  // Sets the interface pointer to |ptr|, once the version is determined. This
  // will eventually invoke SetInstance(), which will notify the observers.
  void OnInstanceReady(mojo::InterfacePtr<T> ptr) {
    temporary_ptr_ = std::move(ptr);
    temporary_ptr_.QueryVersion(base::Bind(&InstanceHolder<T>::OnVersionReady,
                                           weak_factory_.GetWeakPtr()));
  }

  // This method is not intended to be called directly. Normally it is called by
  // OnInstanceReady once the version of the instance is determined, but it is
  // also exposed so that tests can directly inject a raw pointer+version
  // combination.
  void SetInstance(T* raw_ptr, uint32_t raw_version = T::Version_) {
    raw_ptr_ = raw_ptr;
    version_ = raw_version;
    if (observer_list_.might_have_observers()) {
      typename base::ObserverList<Observer>::Iterator it(&observer_list_);
      Observer* obs;
      while ((obs = it.GetNext()) != nullptr)
        obs->OnInstanceReady();
    }
  }

 private:
  void OnVersionReady(uint32_t version) {
    ptr_ = std::move(temporary_ptr_);
    ptr_.set_connection_error_handler(base::Bind(
        &InstanceHolder<T>::CloseChannel, weak_factory_.GetWeakPtr()));
    SetInstance(ptr_.get(), version);
  }

  // These two are copies of the contents of ptr_. They are provided here just
  // so that tests can provide non-mojo implementations.
  T* raw_ptr_ = nullptr;
  uint32_t version_ = 0;

  mojo::InterfacePtr<T> ptr_;

  // Temporary Mojo interfaces.  After a Mojo interface pointer has been
  // received from the other endpoint, we still need to asynchronously query its
  // version.  While that is going on, we should still return nullptr on the
  // instance() function.
  // To keep the instance() functions being trivial, store the instance pointer
  // in a temporary variable to avoid losing its reference.
  mojo::InterfacePtr<T> temporary_ptr_;

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observer_list_;

  // This needs to be the last member in order to cancel all inflight callbacks
  // before destroying any other members.
  base::WeakPtrFactory<InstanceHolder<T>> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstanceHolder<T>);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INSTANCE_HOLDER_H_
