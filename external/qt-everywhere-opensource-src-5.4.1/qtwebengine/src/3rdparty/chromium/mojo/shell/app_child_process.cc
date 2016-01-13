// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_native_library.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/app_child_process.mojom.h"

namespace mojo {
namespace shell {

namespace {

// Blocker ---------------------------------------------------------------------

// Blocks a thread until another thread unblocks it, at which point it unblocks
// and runs a closure provided by that thread.
class Blocker {
 public:
  class Unblocker {
   public:
    ~Unblocker() {}

    void Unblock(base::Closure run_after) {
      DCHECK(blocker_);
      DCHECK(blocker_->run_after_.is_null());
      blocker_->run_after_ = run_after;
      blocker_->event_.Signal();
      blocker_ = NULL;
    }

   private:
    friend class Blocker;
    Unblocker(Blocker* blocker) : blocker_(blocker) {
      DCHECK(blocker_);
    }

    Blocker* blocker_;

    // Copy and assign allowed.
  };

  Blocker() : event_(true, false) {}
  ~Blocker() {}

  void Block() {
    DCHECK(run_after_.is_null());
    event_.Wait();
    run_after_.Run();
  }

  Unblocker GetUnblocker() {
    return Unblocker(this);
  }

 private:
  base::WaitableEvent event_;
  base::Closure run_after_;

  DISALLOW_COPY_AND_ASSIGN(Blocker);
};

// AppContext ------------------------------------------------------------------

class AppChildControllerImpl;

static void DestroyController(scoped_ptr<AppChildControllerImpl> controller) {
}

// Should be created and initialized on the main thread.
class AppContext {
 public:
  AppContext()
      : io_thread_("io_thread"),
        controller_thread_("controller_thread") {}
  ~AppContext() {}

  void Init() {
    // Initialize Mojo before starting any threads.
    embedder::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.message_loop_proxy().get();
    CHECK(io_runner_);

    // Create and start our controller thread.
    base::Thread::Options controller_thread_options;
    controller_thread_options.message_loop_type =
        base::MessageLoop::TYPE_CUSTOM;
    controller_thread_options.message_pump_factory =
        base::Bind(&common::MessagePumpMojo::Create);
    CHECK(controller_thread_.StartWithOptions(controller_thread_options));
    controller_runner_ = controller_thread_.message_loop_proxy().get();
    CHECK(controller_runner_);
  }

  void Shutdown() {
    controller_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DestroyController, base::Passed(&controller_)));
  }

  base::SingleThreadTaskRunner* io_runner() const {
    return io_runner_.get();
  }

  base::SingleThreadTaskRunner* controller_runner() const {
    return controller_runner_.get();
  }

  AppChildControllerImpl* controller() const {
    return controller_.get();
  }

  void set_controller(scoped_ptr<AppChildControllerImpl> controller) {
    controller_ = controller.Pass();
  }

 private:
  // Accessed only on the controller thread.
  // IMPORTANT: This must be BEFORE |controller_thread_|, so that the controller
  // thread gets joined (and thus |controller_| reset) before |controller_| is
  // destroyed.
  scoped_ptr<AppChildControllerImpl> controller_;

  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  base::Thread controller_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_runner_;

  DISALLOW_COPY_AND_ASSIGN(AppContext);
};

// AppChildControllerImpl ------------------------------------------------------

class AppChildControllerImpl : public InterfaceImpl<AppChildController> {
 public:
  virtual ~AppChildControllerImpl() {
    DCHECK(thread_checker_.CalledOnValidThread());

    // TODO(vtl): Pass in the result from |MainMain()|.
    client()->AppCompleted(MOJO_RESULT_UNIMPLEMENTED);
  }

  // To be executed on the controller thread. Creates the |AppChildController|,
  // etc.
  static void Init(
      AppContext* app_context,
      embedder::ScopedPlatformHandle platform_channel,
      const Blocker::Unblocker& unblocker) {
    DCHECK(app_context);
    DCHECK(platform_channel.is_valid());

    DCHECK(!app_context->controller());

    scoped_ptr<AppChildControllerImpl> impl(
        new AppChildControllerImpl(app_context, unblocker));

    ScopedMessagePipeHandle host_message_pipe(embedder::CreateChannel(
        platform_channel.Pass(),
        app_context->io_runner(),
        base::Bind(&AppChildControllerImpl::DidCreateChannel,
                   base::Unretained(impl.get())),
        base::MessageLoopProxy::current()));

    BindToPipe(impl.get(), host_message_pipe.Pass());

    app_context->set_controller(impl.Pass());
  }

  virtual void OnConnectionError() OVERRIDE {
    // TODO(darin): How should we handle a connection error here?
  }

  // |AppChildController| methods:
  virtual void StartApp(const String& app_path,
                        ScopedMessagePipeHandle service) OVERRIDE {
    DVLOG(2) << "AppChildControllerImpl::StartApp(" << app_path << ", ...)";
    DCHECK(thread_checker_.CalledOnValidThread());

    unblocker_.Unblock(base::Bind(&AppChildControllerImpl::StartAppOnMainThread,
                                  base::FilePath::FromUTF8Unsafe(app_path),
                                  base::Passed(&service)));
  }

 private:
  AppChildControllerImpl(AppContext* app_context,
                         const Blocker::Unblocker& unblocker)
      : app_context_(app_context),
        unblocker_(unblocker),
        channel_info_(NULL) {
  }

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info) {
    DVLOG(2) << "AppChildControllerImpl::DidCreateChannel()";
    DCHECK(thread_checker_.CalledOnValidThread());
    channel_info_ = channel_info;
  }

  static void StartAppOnMainThread(const base::FilePath& app_path,
                                   ScopedMessagePipeHandle service) {
    // TODO(vtl): This is copied from in_process_dynamic_service_runner.cc.
    DVLOG(2) << "Loading/running Mojo app from " << app_path.value()
             << " out of process";

    do {
      base::NativeLibraryLoadError load_error;
      base::ScopedNativeLibrary app_library(
          base::LoadNativeLibrary(app_path, &load_error));
      if (!app_library.is_valid()) {
        LOG(ERROR) << "Failed to load library (error: " << load_error.ToString()
                   << ")";
        break;
      }

      typedef MojoResult (*MojoMainFunction)(MojoHandle);
      MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
          app_library.GetFunctionPointer("MojoMain"));
      if (!main_function) {
        LOG(ERROR) << "Entrypoint MojoMain not found";
        break;
      }

      // TODO(vtl): Report the result back to our parent process.
      // |MojoMain()| takes ownership of the service handle.
      MojoResult result = main_function(service.release().value());
      if (result < MOJO_RESULT_OK)
        LOG(ERROR) << "MojoMain returned an error: " << result;
    } while (false);
  }

  base::ThreadChecker thread_checker_;
  AppContext* const app_context_;
  Blocker::Unblocker unblocker_;

  embedder::ChannelInfo* channel_info_;

  DISALLOW_COPY_AND_ASSIGN(AppChildControllerImpl);
};

}  // namespace

// AppChildProcess -------------------------------------------------------------

AppChildProcess::AppChildProcess() {
}

AppChildProcess::~AppChildProcess() {
}

void AppChildProcess::Main() {
  DVLOG(2) << "AppChildProcess::Main()";

  AppContext app_context;
  app_context.Init();

  Blocker blocker;
  app_context.controller_runner()->PostTask(
      FROM_HERE,
      base::Bind(&AppChildControllerImpl::Init, base::Unretained(&app_context),
                 base::Passed(platform_channel()), blocker.GetUnblocker()));
  // This will block, then run whatever the controller wants.
  blocker.Block();

  app_context.Shutdown();
}

}  // namespace shell
}  // namespace mojo
