// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_MOJO_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_MOJO_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/shell/public/cpp/interface_provider.h"

namespace content {

// Implements a client to a Mojo service running on a utility process. Takes
// care of starting the utility process and connecting to the remote Mojo
// service. The utility process is terminated in the destructor.
// Note: This class is not thread-safe. It is bound to the
// SingleThreadTaskRunner it is created on.
template <class MojoInterface>
class UtilityProcessMojoClient {
 public:
  explicit UtilityProcessMojoClient(const base::string16& process_name) {
    helper_.reset(new Helper(process_name));
  }

  ~UtilityProcessMojoClient() {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, helper_.release());
  }

  // Sets the error callback. A valid callback must be set before calling
  // Start().
  void set_error_callback(const base::Closure& on_error_callback) {
    on_error_callback_ = on_error_callback;
  }

  // Disables the sandbox in the utility process.
  void set_disable_sandbox() {
    DCHECK(!start_called_);
    helper_->set_disable_sandbox();
  }

  // Starts the utility process and connect to the remote Mojo service.
  void Start() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!on_error_callback_.is_null());
    DCHECK(!start_called_);

    start_called_ = true;

    mojo::InterfaceRequest<MojoInterface> req = mojo::GetProxy(&service_);
    service_.set_connection_error_handler(on_error_callback_);
    helper_->Start(MojoInterface::Name_, req.PassMessagePipe());
  }

  // Returns the Mojo service used to make calls to the utility process.
  MojoInterface* service() WARN_UNUSED_RESULT {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(start_called_);

    return service_.get();
  }

 private:
  // Helper class that takes care of managing the lifetime of the utility
  // process on the IO thread.
  class Helper {
   public:
    explicit Helper(const base::string16& process_name)
        : process_name_(process_name) {}

    ~Helper() {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);

      // |utility_host_| manages its own lifetime but this forces the process to
      // terminate if it's still alive.
      delete utility_host_.get();
    }

    // Starts the utility process on the IO thread.
    void Start(const std::string& mojo_interface_name,
               mojo::ScopedMessagePipeHandle interface_pipe) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&Helper::StartOnIOThread, base::Unretained(this),
                     mojo_interface_name, base::Passed(&interface_pipe)));
    }

    void set_disable_sandbox() { disable_sandbox_ = true; }

   private:
    // Starts the utility process and connects to the remote Mojo service.
    void StartOnIOThread(const std::string& mojo_interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      utility_host_ = UtilityProcessHost::Create(nullptr, nullptr)->AsWeakPtr();
      utility_host_->SetName(process_name_);
      if (disable_sandbox_)
        utility_host_->DisableSandbox();

      utility_host_->Start();

      utility_host_->GetRemoteInterfaces()->GetInterface(
          mojo_interface_name, std::move(interface_pipe));
    }

    // Properties of the utility process.
    base::string16 process_name_;
    bool disable_sandbox_ = false;

    // Must only be accessed on the IO thread.
    base::WeakPtr<UtilityProcessHost> utility_host_;

    DISALLOW_COPY_AND_ASSIGN(Helper);
  };

  std::unique_ptr<Helper> helper_;

  // Called when a connection error happens or if the process didn't start.
  base::Closure on_error_callback_;

  mojo::InterfacePtr<MojoInterface> service_;

  // Enforce calling Start() before getting the service.
  bool start_called_ = false;

  // Checks that this class is always accessed from the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessMojoClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_UTILITY_PROCESS_MOJO_CLIENT_H_
