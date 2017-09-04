// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_port_local.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/platform/platform_event_source.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace aura {

namespace {

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<base::ThreadLocalPointer<Env> >::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if running inside of mus. Checks for mojo specific flag.
bool RunningInsideMus() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      "primordial-pipe-token");
}

}  // namespace

// Observes destruction and changes of the FocusClient for a window.
// ActiveFocusClientWindowObserver is created for the window the FocusClient is
// associated with.
class Env::ActiveFocusClientWindowObserver : public WindowObserver {
 public:
  explicit ActiveFocusClientWindowObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~ActiveFocusClientWindowObserver() override { window_->RemoveObserver(this); }

  // WindowObserver:
  void OnWindowDestroying(Window* window) override {
    Env::GetInstance()->OnActiveFocusClientWindowDestroying();
  }
  void OnWindowPropertyChanged(Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != client::kFocusClientKey)
      return;

    // Assume if the focus client changes the window is being destroyed.
    Env::GetInstance()->OnActiveFocusClientWindowDestroying();
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ActiveFocusClientWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// Env, public:

Env::~Env() {
  for (EnvObserver& observer : observers_)
    observer.OnWillDestroyEnv();
  DCHECK_EQ(this, lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(NULL);
}

// static
std::unique_ptr<Env> Env::CreateInstance(Mode mode) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  std::unique_ptr<Env> env(new Env(mode));
  env->Init();
  return env;
}

// static
Env* Env::GetInstance() {
  Env* env = lazy_tls_ptr.Pointer()->Get();
  DCHECK(env) << "Env::CreateInstance must be called before getting the "
                 "instance of Env.";
  return env;
}

// static
Env* Env::GetInstanceDontCreate() {
  return lazy_tls_ptr.Pointer()->Get();
}

std::unique_ptr<WindowPort> Env::CreateWindowPort(Window* window) {
  if (mode_ == Mode::LOCAL)
    return base::MakeUnique<WindowPortLocal>(window);

  DCHECK(window_tree_client_);
  // Use LOCAL as all other cases are created by WindowTreeClient explicitly.
  return base::MakeUnique<WindowPortMus>(window_tree_client_,
                                         WindowMusType::LOCAL);
}

void Env::AddObserver(EnvObserver* observer) {
  observers_.AddObserver(observer);
}

void Env::RemoveObserver(EnvObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Env::IsMouseButtonDown() const {
  return input_state_lookup_.get() ? input_state_lookup_->IsMouseButtonDown() :
      mouse_button_flags_ != 0;
}

void Env::SetWindowTreeClient(WindowTreeClient* window_tree_client) {
  // The WindowTreeClient should only be set once. Test code may need to change
  // the value after the fact, to do that use EnvTestHelper.
  DCHECK(!window_tree_client_);
  window_tree_client_ = window_tree_client;
}

void Env::SetActiveFocusClient(client::FocusClient* focus_client,
                               Window* focus_client_root) {
  if (focus_client == active_focus_client_ &&
      focus_client_root == active_focus_client_root_) {
    return;
  }

  active_focus_client_window_observer_.reset();
  active_focus_client_ = focus_client;
  active_focus_client_root_ = focus_client_root;
  if (focus_client_root) {
    active_focus_client_window_observer_ =
        base::MakeUnique<ActiveFocusClientWindowObserver>(focus_client_root);
  }
  for (EnvObserver& observer : observers_)
    observer.OnActiveFocusClientChanged(focus_client, focus_client_root);
}

////////////////////////////////////////////////////////////////////////////////
// Env, private:

Env::Env(Mode mode)
    : mode_(mode),
      mouse_button_flags_(0),
      is_touch_down_(false),
      input_state_lookup_(InputStateLookup::Create()),
      context_factory_(NULL) {
  DCHECK(lazy_tls_ptr.Pointer()->Get() == NULL);
  lazy_tls_ptr.Pointer()->Set(this);
}

void Env::Init() {
  if (RunningInsideMus())
    return;
#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source. If running inside mus
  // let the mus process initialize ozone instead.
  ui::OzonePlatform::InitializeForUI();
#endif
  if (!ui::PlatformEventSource::GetInstance())
    event_source_ = ui::PlatformEventSource::CreateDefault();
}

void Env::NotifyWindowInitialized(Window* window) {
  for (EnvObserver& observer : observers_)
    observer.OnWindowInitialized(window);
}

void Env::NotifyHostInitialized(WindowTreeHost* host) {
  for (EnvObserver& observer : observers_)
    observer.OnHostInitialized(host);
}

void Env::NotifyHostActivated(WindowTreeHost* host) {
  for (EnvObserver& observer : observers_)
    observer.OnHostActivated(host);
}

void Env::OnActiveFocusClientWindowDestroying() {
  SetActiveFocusClient(nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Env, ui::EventTarget implementation:

bool Env::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Env::GetParentTarget() {
  return NULL;
}

std::unique_ptr<ui::EventTargetIterator> Env::GetChildIterator() const {
  return nullptr;
}

ui::EventTargeter* Env::GetEventTargeter() {
  NOTREACHED();
  return NULL;
}

}  // namespace aura
