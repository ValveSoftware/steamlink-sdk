// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_H_
#define UI_AURA_ENV_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "ui/aura/aura_export.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/gfx/point.h"

namespace ui {
class ContextFactory;
class PlatformEventSource;
}
namespace aura {

namespace test {
class EnvTestHelper;
}

class EnvObserver;
class InputStateLookup;
class Window;
class WindowTreeHost;

// A singleton object that tracks general state within Aura.
class AURA_EXPORT Env : public ui::EventTarget, public base::SupportsUserData {
 public:
  // Creates the single Env instance (if it hasn't been created yet). If
  // |create_event_source| is true a PlatformEventSource is created.
  // TODO(sky): nuke |create_event_source|. Only necessary while mojo's
  // nativeviewportservice lives in the same process as the viewmanager.
  static void CreateInstance(bool create_event_source);
  static Env* GetInstance();
  static void DeleteInstance();

  void AddObserver(EnvObserver* observer);
  void RemoveObserver(EnvObserver* observer);

  const int mouse_button_flags() const { return mouse_button_flags_; }
  void set_mouse_button_flags(int mouse_button_flags) {
    mouse_button_flags_ = mouse_button_flags;
  }
  // Returns true if a mouse button is down. This may query the native OS,
  // otherwise it uses |mouse_button_flags_|.
  bool IsMouseButtonDown() const;

  // Gets/sets the last mouse location seen in a mouse event in the screen
  // coordinates.
  const gfx::Point& last_mouse_location() const { return last_mouse_location_; }
  void set_last_mouse_location(const gfx::Point& last_mouse_location) {
    last_mouse_location_ = last_mouse_location;
  }

  // Whether any touch device is currently down.
  bool is_touch_down() const { return is_touch_down_; }
  void set_touch_down(bool value) { is_touch_down_ = value; }

  void set_context_factory(ui::ContextFactory* context_factory) {
    context_factory_ = context_factory;
  }
  ui::ContextFactory* context_factory() { return context_factory_; }

 private:
  friend class test::EnvTestHelper;
  friend class Window;
  friend class WindowTreeHost;

  Env();
  virtual ~Env();

  // See description of CreateInstance() for deatils of |create_event_source|.
  void Init(bool create_event_source);

  // Called by the Window when it is initialized. Notifies observers.
  void NotifyWindowInitialized(Window* window);

  // Called by the WindowTreeHost when it is initialized. Notifies observers.
  void NotifyHostInitialized(WindowTreeHost* host);

  // Invoked by WindowTreeHost when it is activated. Notifies observers.
  void NotifyHostActivated(WindowTreeHost* host);

  // Overridden from ui::EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE;
  virtual ui::EventTarget* GetParentTarget() OVERRIDE;
  virtual scoped_ptr<ui::EventTargetIterator> GetChildIterator() const OVERRIDE;
  virtual ui::EventTargeter* GetEventTargeter() OVERRIDE;

  ObserverList<EnvObserver> observers_;

  int mouse_button_flags_;
  // Location of last mouse event, in screen coordinates.
  gfx::Point last_mouse_location_;
  bool is_touch_down_;

  scoped_ptr<InputStateLookup> input_state_lookup_;
  scoped_ptr<ui::PlatformEventSource> event_source_;

  ui::ContextFactory* context_factory_;

  DISALLOW_COPY_AND_ASSIGN(Env);
};

}  // namespace aura

#endif  // UI_AURA_ENV_H_
