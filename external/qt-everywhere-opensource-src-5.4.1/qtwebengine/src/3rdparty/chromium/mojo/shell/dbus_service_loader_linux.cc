// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/dbus_service_loader_linux.h"

#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/file_descriptor.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "mojo/dbus/dbus_external_service.h"
#include "mojo/embedder/channel_init.h"
#include "mojo/embedder/platform_channel_pair.h"
#include "mojo/shell/context.h"
#include "mojo/shell/external_service.mojom.h"
#include "mojo/shell/keep_alive.h"

namespace mojo {
namespace shell {

// Manages the connection to a single externally-running service.
class DBusServiceLoader::LoadContext {
 public:
  // Kicks off the attempt to bootstrap a connection to the externally-running
  // service specified by url_.
  // Creates a MessagePipe and passes one end over DBus to the service. Then,
  // calls ExternalService::Activate(ShellHandle) over the now-shared pipe.
  LoadContext(DBusServiceLoader* loader,
              const scoped_refptr<dbus::Bus>& bus,
              const GURL& url,
              ScopedMessagePipeHandle service_provider_handle)
      : loader_(loader),
        bus_(bus),
        service_dbus_proxy_(NULL),
        url_(url),
        service_provider_handle_(service_provider_handle.Pass()),
        keep_alive_(loader->context_) {
    base::PostTaskAndReplyWithResult(
        loader_->context_->task_runners()->io_runner(),
        FROM_HERE,
        base::Bind(&LoadContext::CreateChannelOnIOThread,
                   base::Unretained(this)),
        base::Bind(&LoadContext::ConnectChannel, base::Unretained(this)));
  }

  virtual ~LoadContext() {
  }

 private:
  // Sets up a pipe to share with the externally-running service and returns
  // the endpoint that should be sent over DBus.
  // The FD for the endpoint must be validated on an IO thread.
  scoped_ptr<dbus::FileDescriptor> CreateChannelOnIOThread() {
    base::ThreadRestrictions::AssertIOAllowed();
    CHECK(bus_->Connect());
    CHECK(bus_->SetUpAsyncOperations());

    embedder::PlatformChannelPair channel_pair;
    channel_init_.reset(new embedder::ChannelInit);
    mojo::ScopedMessagePipeHandle bootstrap_message_pipe =
        channel_init_->Init(channel_pair.PassServerHandle().release().fd,
                            loader_->context_->task_runners()->io_runner());
    CHECK(bootstrap_message_pipe.is_valid());

    external_service_.Bind(bootstrap_message_pipe.Pass());

    scoped_ptr<dbus::FileDescriptor> client_fd(new dbus::FileDescriptor);
    client_fd->PutValue(channel_pair.PassClientHandle().release().fd);
    client_fd->CheckValidity();  // Must be run on an IO thread.
    return client_fd.Pass();
  }

  // Sends client_fd over to the externally-running service. If that
  // attempt is successful, the service will then be "activated" by
  // sending it a ShellHandle.
  void ConnectChannel(scoped_ptr<dbus::FileDescriptor> client_fd) {
    size_t first_slash = url_.path().find_first_of('/');
    DCHECK_NE(first_slash, std::string::npos);

    const std::string service_name = url_.path().substr(0, first_slash);
    const std::string object_path =  url_.path().substr(first_slash);
    service_dbus_proxy_ =
        bus_->GetObjectProxy(service_name, dbus::ObjectPath(object_path));

    dbus::MethodCall call(kMojoDBusInterface, kMojoDBusConnectMethod);
    dbus::MessageWriter writer(&call);
    writer.AppendFileDescriptor(*client_fd.get());

    // TODO(cmasone): handle errors!
    service_dbus_proxy_->CallMethod(
        &call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&LoadContext::ActivateService, base::Unretained(this)));
  }

  // Sends a ShellHandle over to the now-connected externally-running service,
  // using the Mojo ExternalService API.
  void ActivateService(dbus::Response* response) {
    external_service_->Activate(
        mojo::ScopedMessagePipeHandle(
            mojo::MessagePipeHandle(
                service_provider_handle_.release().value())));
  }

  // Should the ExternalService disappear completely, destroy connection state.
  // NB: This triggers off of the service disappearing from
  // DBus. Perhaps there's a way to watch at the Mojo layer instead,
  // and that would be superior?
  void HandleNameOwnerChanged(const std::string& old_owner,
                              const std::string& new_owner) {
    DCHECK(loader_->context_->task_runners()->ui_runner()->
           BelongsToCurrentThread());

    if (new_owner.empty()) {
      loader_->context_->task_runners()->ui_runner()->PostTask(
          FROM_HERE,
          base::Bind(&DBusServiceLoader::ForgetService,
                     base::Unretained(loader_), url_));
    }
  }

  DBusServiceLoader* const loader_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* service_dbus_proxy_;  // Owned by bus_;
  const GURL url_;
  ScopedMessagePipeHandle service_provider_handle_;
  KeepAlive keep_alive_;
  scoped_ptr<embedder::ChannelInit> channel_init_;
  ExternalServicePtr external_service_;

  DISALLOW_COPY_AND_ASSIGN(LoadContext);
};

DBusServiceLoader::DBusServiceLoader(Context* context) : context_(context) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.dbus_task_runner = context_->task_runners()->io_runner();
  bus_ = new dbus::Bus(options);
}

DBusServiceLoader::~DBusServiceLoader() {
  DCHECK(url_to_load_context_.empty());
}

void DBusServiceLoader::LoadService(ServiceManager* manager,
                                    const GURL& url,
                                    ScopedMessagePipeHandle service_handle) {
  DCHECK(url.SchemeIs("dbus"));
  DCHECK(url_to_load_context_.find(url) == url_to_load_context_.end());
  url_to_load_context_[url] =
      new LoadContext(this, bus_, url, service_handle.Pass());
}

void DBusServiceLoader::OnServiceError(ServiceManager* manager,
                                       const GURL& url) {
  // TODO(cmasone): Anything at all in this method here.
}

void DBusServiceLoader::ForgetService(const GURL& url) {
  DCHECK(context_->task_runners()->ui_runner()->BelongsToCurrentThread());
  DVLOG(2) << "Forgetting service (url: " << url << ")";

  LoadContextMap::iterator it = url_to_load_context_.find(url);
  DCHECK(it != url_to_load_context_.end()) << url;

  LoadContext* doomed = it->second;
  url_to_load_context_.erase(it);

  delete doomed;
}

}  // namespace shell
}  // namespace mojo
