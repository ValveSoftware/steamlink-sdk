// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/service_manager_connection_impl.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "content/common/service_manager/embedded_service_runner.h"
#include "content/public/common/connection_filter.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/runner/common/client_util.h"

namespace content {
namespace {

base::LazyInstance<std::unique_ptr<ServiceManagerConnection>>::Leaky
    g_connection_for_process = LAZY_INSTANCE_INITIALIZER;

ServiceManagerConnection::Factory* service_manager_connection_factory = nullptr;

}  // namespace

// A ref-counted object which owns the IO thread state of a
// ServiceManagerConnectionImpl. This includes Service and ServiceFactory
// bindings.
class ServiceManagerConnectionImpl::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public service_manager::Service,
      public service_manager::InterfaceFactory<
          service_manager::mojom::ServiceFactory>,
      public service_manager::mojom::ServiceFactory {
 public:
  using InitializeCallback =
      base::Callback<void(const service_manager::Identity&)>;
  using ServiceFactoryCallback =
      base::Callback<void(service_manager::mojom::ServiceRequest,
                          const std::string&)>;

  IOThreadContext(
      service_manager::mojom::ServiceRequest service_request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      std::unique_ptr<service_manager::Connector> io_thread_connector,
      service_manager::mojom::ConnectorRequest connector_request)
      : pending_service_request_(std::move(service_request)),
        io_task_runner_(io_task_runner),
        io_thread_connector_(std::move(io_thread_connector)),
        pending_connector_request_(std::move(connector_request)),
        weak_factory_(this) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }

  // Safe to call from any thread.
  void Start(
      const InitializeCallback& initialize_callback,
      const ServiceManagerConnection::OnConnectHandler& on_connect_callback,
      const ServiceFactoryCallback& create_service_callback,
      const base::Closure& stop_callback) {
    DCHECK(!started_);

    started_ = true;
    callback_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    initialize_handler_ = initialize_callback;
    on_connect_callback_ = on_connect_callback;
    create_service_callback_ = create_service_callback;
    stop_callback_ = stop_callback;
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOThreadContext::StartOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  // Safe to call any time before a message is received from a process.
  // i.e. can be called when starting the process but not afterwards.
  int AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) {
    base::AutoLock lock(lock_);

    int id = ++next_filter_id_;

    // We should never hit this in practice, but let's crash just in case.
    CHECK_NE(id, kInvalidConnectionFilterId);

    connection_filters_[id] = std::move(filter);
    return id;
  }

  void RemoveConnectionFilter(int filter_id) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::RemoveConnectionFilterOnIOThread, this,
                   filter_id));
  }

  // Safe to call any time before Start() is called.
  void SetDefaultBinderForBrowserConnection(
      const service_manager::InterfaceRegistry::Binder& binder) {
    DCHECK(!started_);
    default_browser_binder_ = base::Bind(
        &IOThreadContext::CallBinderOnTaskRunner,
        base::ThreadTaskRunnerHandle::Get(), binder);
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  class MessageLoopObserver : public base::MessageLoop::DestructionObserver {
   public:
    explicit MessageLoopObserver(base::WeakPtr<IOThreadContext> context)
        : context_(context) {
      base::MessageLoop::current()->AddDestructionObserver(this);
    }

    ~MessageLoopObserver() override {
      base::MessageLoop::current()->RemoveDestructionObserver(this);
    }

    void ShutDown() {
      if (!is_active_)
        return;

      // The call into |context_| below may reenter ShutDown(), hence we set
      // |is_active_| to false here.
      is_active_ = false;
      if (context_)
        context_->ShutDownOnIOThread();

      delete this;
    }

   private:
    void WillDestroyCurrentMessageLoop() override {
      DCHECK(is_active_);
      ShutDown();
    }

    bool is_active_ = true;
    base::WeakPtr<IOThreadContext> context_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
  };

  ~IOThreadContext() override {}

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_context_.reset(new service_manager::ServiceContext(
        base::MakeUnique<service_manager::ForwardingService>(this),
        std::move(pending_service_request_),
        std::move(io_thread_connector_),
        std::move(pending_connector_request_)));

    // MessageLoopObserver owns itself.
    message_loop_observer_ =
        new MessageLoopObserver(weak_factory_.GetWeakPtr());
  }

  void ShutDownOnIOThread() {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    weak_factory_.InvalidateWeakPtrs();

    // Note that this method may be invoked by MessageLoopObserver observing
    // MessageLoop destruction. In that case, this call to ShutDown is
    // effectively a no-op. In any case it's safe.
    if (message_loop_observer_) {
      message_loop_observer_->ShutDown();
      message_loop_observer_ = nullptr;
    }

    // Resetting the ServiceContext below may otherwise release the last
    // reference to this IOThreadContext. We keep it alive until the stack
    // unwinds.
    scoped_refptr<IOThreadContext> keepalive(this);

    factory_bindings_.CloseAllBindings();
    service_context_.reset();

    ClearConnectionFiltersOnIOThread();
  }

  void ClearConnectionFiltersOnIOThread() {
    base::AutoLock lock(lock_);
    connection_filters_.clear();
  }

  void RemoveConnectionFilterOnIOThread(int filter_id) {
    base::AutoLock lock(lock_);
    auto it = connection_filters_.find(filter_id);
    DCHECK(it != connection_filters_.end());
    connection_filters_.erase(it);
  }

  void OnBrowserConnectionLost() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    has_browser_connection_ = false;
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::Service implementation

  void OnStart() override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    DCHECK(!initialize_handler_.is_null());
    local_info_ = context()->local_info();

    InitializeCallback handler = base::ResetAndReturn(&initialize_handler_);
    callback_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(handler, local_info_.identity));
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    callback_task_runner_->PostTask(
        FROM_HERE, base::Bind(on_connect_callback_, local_info_, remote_info));

    std::string remote_service = remote_info.identity.name();
    if (remote_service == service_manager::mojom::kServiceName) {
      // Only expose the ServiceFactory interface to the Service Manager.
      registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
      return true;
    }

    bool accept = false;
    {
      base::AutoLock lock(lock_);
      for (auto& entry : connection_filters_) {
        accept |= entry.second->OnConnect(remote_info.identity, registry,
                                          service_context_->connector());
      }
    }

    if (remote_service == "content_browser" &&
        !has_browser_connection_) {
      has_browser_connection_ = true;
      registry->set_default_binder(default_browser_binder_);
      registry->AddConnectionLostClosure(
          base::Bind(&IOThreadContext::OnBrowserConnectionLost, this));
      return true;
    }

    // If no filters were interested, reject the connection.
    return accept;
  }

  bool OnStop() override {
    ClearConnectionFiltersOnIOThread();
    callback_task_runner_->PostTask(FROM_HERE, stop_callback_);
    return true;
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::InterfaceFactory<service_manager::mojom::ServiceFactory>
  // implementation

  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    factory_bindings_.AddBinding(this, std::move(request));
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::mojom::ServiceFactory implementation

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    callback_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(create_service_callback_, base::Passed(&request), name));
  }

  static void CallBinderOnTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const service_manager::InterfaceRegistry::Binder& binder,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle request_handle) {
    task_runner->PostTask(FROM_HERE, base::Bind(binder, interface_name,
                                                base::Passed(&request_handle)));
  }

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  service_manager::mojom::ServiceRequest pending_service_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<service_manager::Connector> io_thread_connector_;
  service_manager::mojom::ConnectorRequest pending_connector_request_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Callback to run once Service::OnStart is invoked.
  InitializeCallback initialize_handler_;

  // Callback to run when a connection request is received.
  ServiceManagerConnection::OnConnectHandler on_connect_callback_;

  // Callback to run when a new Service request is received.
  ServiceFactoryCallback create_service_callback_;

  // Callback to run if the service is stopped by the service manager.
  base::Closure stop_callback_;

  // Called once a connection has been received from the browser process & the
  // default binder (below) has been set up.
  bool has_browser_connection_ = false;

  service_manager::ServiceInfo local_info_;

  // Default binder callback used for the browser connection's
  // InterfaceRegistry.
  //
  // TODO(rockot): Remove this once all interfaces exposed to the browser are
  // exposed via a ConnectionFilter.
  service_manager::InterfaceRegistry::Binder default_browser_binder_;

  std::unique_ptr<service_manager::ServiceContext> service_context_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory> factory_bindings_;
  int next_filter_id_ = kInvalidConnectionFilterId;

  // Not owned.
  MessageLoopObserver* message_loop_observer_ = nullptr;

  // Guards |connection_filters_|.
  base::Lock lock_;
  std::map<int, std::unique_ptr<ConnectionFilter>> connection_filters_;

  base::WeakPtrFactory<IOThreadContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnection, public:

// static
void ServiceManagerConnection::SetForProcess(
    std::unique_ptr<ServiceManagerConnection> connection) {
  DCHECK(!g_connection_for_process.Get());
  g_connection_for_process.Get() = std::move(connection);
}

// static
ServiceManagerConnection* ServiceManagerConnection::GetForProcess() {
  return g_connection_for_process.Get().get();
}

// static
void ServiceManagerConnection::DestroyForProcess() {
  // This joins the service manager controller thread.
  g_connection_for_process.Get().reset();
}

// static
void ServiceManagerConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!g_connection_for_process.Get());
  service_manager_connection_factory = factory;
}

// static
std::unique_ptr<ServiceManagerConnection> ServiceManagerConnection::Create(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  if (service_manager_connection_factory)
    return service_manager_connection_factory->Run();
  return base::MakeUnique<ServiceManagerConnectionImpl>(
      std::move(request), io_task_runner);
}

ServiceManagerConnection::~ServiceManagerConnection() {}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, public:

ServiceManagerConnectionImpl::ServiceManagerConnectionImpl(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : weak_factory_(this) {
  service_manager::mojom::ConnectorRequest connector_request;
  connector_ = service_manager::Connector::Create(&connector_request);

  std::unique_ptr<service_manager::Connector> io_thread_connector =
      connector_->Clone();
  context_ = new IOThreadContext(
      std::move(request), io_task_runner, std::move(io_thread_connector),
      std::move(connector_request));
}

ServiceManagerConnectionImpl::~ServiceManagerConnectionImpl() {
  context_->ShutDown();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, ServiceManagerConnection implementation:

void ServiceManagerConnectionImpl::Start() {
  context_->Start(
      base::Bind(&ServiceManagerConnectionImpl::OnContextInitialized,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceManagerConnectionImpl::OnConnect,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceManagerConnectionImpl::CreateService,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceManagerConnectionImpl::OnConnectionLost,
                 weak_factory_.GetWeakPtr()));
}

void ServiceManagerConnectionImpl::SetInitializeHandler(
    const base::Closure& handler) {
  DCHECK(initialize_handler_.is_null());
  initialize_handler_ = handler;
}

service_manager::Connector* ServiceManagerConnectionImpl::GetConnector() {
  return connector_.get();
}

const service_manager::Identity& ServiceManagerConnectionImpl::GetIdentity()
    const {
  return identity_;
}

void ServiceManagerConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  connection_lost_handler_ = closure;
}

void ServiceManagerConnectionImpl::SetupInterfaceRequestProxies(
    service_manager::InterfaceRegistry* registry,
    service_manager::InterfaceProvider* provider) {
  // It's safe to bind |registry| as a raw pointer because the caller must
  // guarantee that it outlives |this|, and |this| is bound as a weak ptr here.
  context_->SetDefaultBinderForBrowserConnection(
      base::Bind(&ServiceManagerConnectionImpl::GetInterface,
                 weak_factory_.GetWeakPtr(), registry));

  // TODO(beng): remove provider parameter.
}

int ServiceManagerConnectionImpl::AddConnectionFilter(
    std::unique_ptr<ConnectionFilter> filter) {
  return context_->AddConnectionFilter(std::move(filter));
}

void ServiceManagerConnectionImpl::RemoveConnectionFilter(int filter_id) {
  context_->RemoveConnectionFilter(filter_id);
}

void ServiceManagerConnectionImpl::AddEmbeddedService(const std::string& name,
                                                      const ServiceInfo& info) {
  std::unique_ptr<EmbeddedServiceRunner> service(
      new EmbeddedServiceRunner(name, info));
  AddServiceRequestHandler(
      name, base::Bind(&EmbeddedServiceRunner::BindServiceRequest,
                       base::Unretained(service.get())));
  auto result =
      embedded_services_.insert(std::make_pair(name, std::move(service)));
  DCHECK(result.second);
}

void ServiceManagerConnectionImpl::AddServiceRequestHandler(
    const std::string& name,
    const ServiceRequestHandler& handler) {
  auto result = request_handlers_.insert(std::make_pair(name, handler));
  DCHECK(result.second);
}

int ServiceManagerConnectionImpl::AddOnConnectHandler(
    const OnConnectHandler& handler) {
  int id = ++next_on_connect_handler_id_;
  on_connect_handlers_[id] = handler;
  return id;
}

void ServiceManagerConnectionImpl::RemoveOnConnectHandler(int id) {
  auto it = on_connect_handlers_.find(id);
  DCHECK(it != on_connect_handlers_.end());
  on_connect_handlers_.erase(it);
}

void ServiceManagerConnectionImpl::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name) {
  auto it = request_handlers_.find(name);
  if (it != request_handlers_.end())
    it->second.Run(std::move(request));
}

void ServiceManagerConnectionImpl::OnContextInitialized(
    const service_manager::Identity& identity) {
  identity_ = identity;
  if (!initialize_handler_.is_null())
    base::ResetAndReturn(&initialize_handler_).Run();
}

void ServiceManagerConnectionImpl::OnConnectionLost() {
  if (!connection_lost_handler_.is_null())
    connection_lost_handler_.Run();
}

void ServiceManagerConnectionImpl::OnConnect(
    const service_manager::ServiceInfo& local_info,
    const service_manager::ServiceInfo& remote_info) {
  local_info_ = local_info;
  for (auto& handler : on_connect_handlers_)
    handler.second.Run(local_info, remote_info);
}

void ServiceManagerConnectionImpl::GetInterface(
    service_manager::mojom::InterfaceProvider* provider,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  provider->GetInterface(interface_name, std::move(request_handle));
}

}  // namespace content

