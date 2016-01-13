// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/scoped_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

using std::make_pair;

namespace wm {

namespace {

ShadowType GetShadowTypeFromWindow(aura::Window* window) {
  switch (window->type()) {
    case ui::wm::WINDOW_TYPE_NORMAL:
    case ui::wm::WINDOW_TYPE_PANEL:
    case ui::wm::WINDOW_TYPE_MENU:
    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return SHADOW_TYPE_RECTANGULAR;
    default:
      break;
  }
  return SHADOW_TYPE_NONE;
}

bool ShouldUseSmallShadowForWindow(aura::Window* window) {
  switch (window->type()) {
    case ui::wm::WINDOW_TYPE_MENU:
    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return true;
    default:
      break;
  }
  return false;
}

// Returns the shadow style to be applied to |losing_active| when it is losing
// active to |gaining_active|. |gaining_active| may be of a type that hides when
// inactive, and as such we do not want to render |losing_active| as inactive.
Shadow::Style GetShadowStyleForWindowLosingActive(
    aura::Window* losing_active,
    aura::Window* gaining_active) {
  if (gaining_active && aura::client::GetHideOnDeactivate(gaining_active)) {
    aura::Window::Windows::const_iterator it =
        std::find(GetTransientChildren(losing_active).begin(),
                  GetTransientChildren(losing_active).end(),
                  gaining_active);
    if (it != GetTransientChildren(losing_active).end())
      return Shadow::STYLE_ACTIVE;
  }
  return Shadow::STYLE_INACTIVE;
}

}  // namespace

// ShadowController::Impl ------------------------------------------------------

// Real implementation of the ShadowController. ShadowController observes
// ActivationChangeObserver, which are per ActivationClient, where as there is
// only a single Impl (as it observes all window creation by way of an
// EnvObserver).
class ShadowController::Impl :
      public aura::EnvObserver,
      public aura::WindowObserver,
      public base::RefCounted<Impl> {
 public:
  // Returns the singleton instance, destroyed when there are no more refs.
  static Impl* GetInstance();

  // aura::EnvObserver override:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(
      aura::Window* window, const void* key, intptr_t old) OVERRIDE;
  virtual void OnWindowBoundsChanged(
      aura::Window* window,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  friend class base::RefCounted<Impl>;
  friend class ShadowController;
  friend class ShadowController::TestApi;

  typedef std::map<aura::Window*, linked_ptr<Shadow> > WindowShadowMap;

  Impl();
  virtual ~Impl();

  // Forwarded from ShadowController.
  void OnWindowActivated(aura::Window* gained_active,
                         aura::Window* lost_active);

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Returns |window|'s shadow from |window_shadows_|, or NULL if no shadow
  // exists.
  Shadow* GetShadowForWindow(aura::Window* window);

  // Updates the shadow styles for windows when activation changes.
  void HandleWindowActivationChange(aura::Window* gaining_active,
                                    aura::Window* losing_active);

  // Shows or hides |window|'s shadow as needed (creating the shadow if
  // necessary).
  void HandlePossibleShadowVisibilityChange(aura::Window* window);

  // Creates a new shadow for |window| and stores it in |window_shadows_|.  The
  // shadow's bounds are initialized and it is added to the window's layer.
  void CreateShadowForWindow(aura::Window* window);

  WindowShadowMap window_shadows_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  static Impl* instance_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// static
ShadowController::Impl* ShadowController::Impl::instance_ = NULL;

// static
ShadowController::Impl* ShadowController::Impl::GetInstance() {
  if (!instance_)
    instance_ = new Impl();
  return instance_;
}

void ShadowController::Impl::OnWindowInitialized(aura::Window* window) {
  observer_manager_.Add(window);
  SetShadowType(window, GetShadowTypeFromWindow(window));
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == kShadowTypeKey) {
    HandlePossibleShadowVisibilityChange(window);
    return;
  }
}

void ShadowController::Impl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->SetContentBounds(gfx::Rect(new_bounds.size()));
}

void ShadowController::Impl::OnWindowDestroyed(aura::Window* window) {
  window_shadows_.erase(window);
  observer_manager_.Remove(window);
}

void ShadowController::Impl::OnWindowActivated(aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (gained_active) {
    Shadow* shadow = GetShadowForWindow(gained_active);
    if (shadow && !ShouldUseSmallShadowForWindow(gained_active))
      shadow->SetStyle(Shadow::STYLE_ACTIVE);
  }
  if (lost_active) {
    Shadow* shadow = GetShadowForWindow(lost_active);
    if (shadow && !ShouldUseSmallShadowForWindow(lost_active)) {
      shadow->SetStyle(GetShadowStyleForWindowLosingActive(lost_active,
                                                           gained_active));
    }
  }
}

bool ShadowController::Impl::ShouldShowShadowForWindow(
    aura::Window* window) const {
  const ShadowType type = GetShadowType(window);
  switch (type) {
    case SHADOW_TYPE_NONE:
      return false;
    case SHADOW_TYPE_RECTANGULAR:
      return true;
    default:
      NOTREACHED() << "Unknown shadow type " << type;
      return false;
  }
}

Shadow* ShadowController::Impl::GetShadowForWindow(aura::Window* window) {
  WindowShadowMap::const_iterator it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : NULL;
}

void ShadowController::Impl::HandlePossibleShadowVisibilityChange(
    aura::Window* window) {
  const bool should_show = ShouldShowShadowForWindow(window);
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->layer()->SetVisible(should_show);
  else if (should_show && !shadow)
    CreateShadowForWindow(window);
}

void ShadowController::Impl::CreateShadowForWindow(aura::Window* window) {
  linked_ptr<Shadow> shadow(new Shadow());
  window_shadows_.insert(make_pair(window, shadow));

  shadow->Init(ShouldUseSmallShadowForWindow(window) ?
               Shadow::STYLE_SMALL : Shadow::STYLE_ACTIVE);
  shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));
  window->layer()->Add(shadow->layer());
}

ShadowController::Impl::Impl()
    : observer_manager_(this) {
  aura::Env::GetInstance()->AddObserver(this);
}

ShadowController::Impl::~Impl() {
  DCHECK_EQ(instance_, this);
  aura::Env::GetInstance()->RemoveObserver(this);
  instance_ = NULL;
}

// ShadowController ------------------------------------------------------------

ShadowController::ShadowController(
    aura::client::ActivationClient* activation_client)
    : activation_client_(activation_client),
      impl_(Impl::GetInstance()) {
  // Watch for window activation changes.
  activation_client_->AddObserver(this);
}

ShadowController::~ShadowController() {
  activation_client_->RemoveObserver(this);
}

void ShadowController::OnWindowActivated(aura::Window* gained_active,
                                         aura::Window* lost_active) {
  impl_->OnWindowActivated(gained_active, lost_active);
}

// ShadowController::TestApi ---------------------------------------------------

Shadow* ShadowController::TestApi::GetShadowForWindow(aura::Window* window) {
  return controller_->impl_->GetShadowForWindow(window);
}

}  // namespace wm
