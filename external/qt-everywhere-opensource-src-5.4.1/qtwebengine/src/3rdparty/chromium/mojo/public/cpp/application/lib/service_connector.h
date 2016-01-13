// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_

#include <assert.h>

#include <vector>

#include "mojo/public/cpp/application/lib/service_registry.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {
namespace internal {

template <class ServiceImpl, typename Context>
class ServiceConnector;

// Specialization of ServiceConnection.
// ServiceImpl: Subclass of InterfaceImpl<...>.
// Context: Type of shared context.
template <class ServiceImpl, typename Context>
class ServiceConnection : public ServiceImpl {
 public:
  ServiceConnection() : ServiceImpl() {}
  ServiceConnection(Context* context) : ServiceImpl(context) {}

  virtual void OnConnectionError() MOJO_OVERRIDE {
    service_connector_->RemoveConnection(static_cast<ServiceImpl*>(this));
    ServiceImpl::OnConnectionError();
  }

private:
  friend class ServiceConnector<ServiceImpl, Context>;

  // Called shortly after this class is instantiated.
  void set_service_connector(
      ServiceConnector<ServiceImpl, Context>* connector) {
    service_connector_ = connector;
  }

  ServiceConnector<ServiceImpl, Context>* service_connector_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceConnection);
};

template <typename ServiceImpl, typename Context>
struct ServiceConstructor {
  static ServiceConnection<ServiceImpl, Context>* New(Context* context) {
    return new ServiceConnection<ServiceImpl, Context>(context);
  }
};

template <typename ServiceImpl>
struct ServiceConstructor<ServiceImpl, void> {
 public:
  static ServiceConnection<ServiceImpl, void>* New(void* context) {
    return new ServiceConnection<ServiceImpl, void>();
  }
};

class ServiceConnectorBase {
 public:
  ServiceConnectorBase(const std::string& name);
  virtual ~ServiceConnectorBase();
  virtual void ConnectToService(const std::string& url,
                                const std::string& name,
                                ScopedMessagePipeHandle client_handle) = 0;
  std::string name() const { return name_; }
  void set_registry(ServiceRegistry* registry) { registry_ = registry; }

 protected:
  std::string name_;
  ServiceRegistry* registry_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceConnectorBase);
};

template <class ServiceImpl, typename Context=void>
class ServiceConnector : public internal::ServiceConnectorBase {
 public:
  ServiceConnector(const std::string& name, Context* context = NULL)
      : ServiceConnectorBase(name), context_(context) {}

  virtual ~ServiceConnector() {
    ConnectionList doomed;
    doomed.swap(connections_);
    for (typename ConnectionList::iterator it = doomed.begin();
         it != doomed.end(); ++it) {
      delete *it;
    }
    assert(connections_.empty());  // No one should have added more!
  }

  virtual void ConnectToService(const std::string& url,
                                const std::string& name,
                                ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    ServiceConnection<ServiceImpl, Context>* impl =
        ServiceConstructor<ServiceImpl, Context>::New(context_);
    impl->set_service_connector(this);
    BindToPipe(impl, handle.Pass());

    connections_.push_back(impl);
  }

  void RemoveConnection(ServiceImpl* impl) {
    // Called from ~ServiceImpl, in response to a connection error.
    for (typename ConnectionList::iterator it = connections_.begin();
         it != connections_.end(); ++it) {
      if (*it == impl) {
        delete impl;
        connections_.erase(it);
        return;
      }
    }
  }

  Context* context() const { return context_; }

 private:
  typedef std::vector<ServiceImpl*> ConnectionList;
  ConnectionList connections_;
  Context* context_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceConnector);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
