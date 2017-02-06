// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/input_state_lookup.h"
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

////////////////////////////////////////////////////////////////////////////////
// Env, public:

Env::~Env() {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnWillDestroyEnv());
  DCHECK_EQ(this, lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(NULL);
}

// static
std::unique_ptr<Env> Env::CreateInstance() {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  std::unique_ptr<Env> env(new Env());
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

////////////////////////////////////////////////////////////////////////////////
// Env, private:

Env::Env()
    : mouse_button_flags_(0),
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
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnWindowInitialized(window));
}

void Env::NotifyHostInitialized(WindowTreeHost* host) {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnHostInitialized(host));
}

void Env::NotifyHostActivated(WindowTreeHost* host) {
  FOR_EACH_OBSERVER(EnvObserver, observers_, OnHostActivated(host));
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
