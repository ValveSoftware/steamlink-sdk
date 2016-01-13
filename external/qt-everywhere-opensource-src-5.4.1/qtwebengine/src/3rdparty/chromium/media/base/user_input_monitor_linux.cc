// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <sys/select.h>
#include <unistd.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/base/keyboard_event_counter.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/x11_types.h"

// These includes need to be later than dictated by the style guide due to
// Xlib header pollution, specifically the min, max, and Status macros.
#include <X11/XKBlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

namespace media {
namespace {

// This is the actual implementation of event monitoring. It's separated from
// UserInputMonitorLinux since it needs to be deleted on the IO thread.
class UserInputMonitorLinuxCore
    : public base::MessagePumpLibevent::Watcher,
      public base::SupportsWeakPtr<UserInputMonitorLinuxCore>,
      public base::MessageLoop::DestructionObserver {
 public:
  enum EventType {
    MOUSE_EVENT,
    KEYBOARD_EVENT
  };

  explicit UserInputMonitorLinuxCore(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<UserInputMonitor::MouseListenerList>&
          mouse_listeners);
  virtual ~UserInputMonitorLinuxCore();

  // DestructionObserver overrides.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  size_t GetKeyPressCount() const;
  void StartMonitor(EventType type);
  void StopMonitor(EventType type);

 private:
  // base::MessagePumpLibevent::Watcher interface.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // Processes key and mouse events.
  void ProcessXEvent(xEvent* event);
  static void ProcessReply(XPointer self, XRecordInterceptData* data);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<ObserverListThreadSafe<UserInputMonitor::MouseEventListener> >
      mouse_listeners_;

  //
  // The following members should only be accessed on the IO thread.
  //
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;
  Display* x_control_display_;
  Display* x_record_display_;
  XRecordRange* x_record_range_[2];
  XRecordContext x_record_context_;
  KeyboardEventCounter counter_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorLinuxCore);
};

class UserInputMonitorLinux : public UserInputMonitor {
 public:
  explicit UserInputMonitorLinux(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  virtual ~UserInputMonitorLinux();

  // Public UserInputMonitor overrides.
  virtual size_t GetKeyPressCount() const OVERRIDE;

 private:
  // Private UserInputMonitor overrides.
  virtual void StartKeyboardMonitoring() OVERRIDE;
  virtual void StopKeyboardMonitoring() OVERRIDE;
  virtual void StartMouseMonitoring() OVERRIDE;
  virtual void StopMouseMonitoring() OVERRIDE;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  UserInputMonitorLinuxCore* core_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorLinux);
};

UserInputMonitorLinuxCore::UserInputMonitorLinuxCore(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<UserInputMonitor::MouseListenerList>& mouse_listeners)
    : io_task_runner_(io_task_runner),
      mouse_listeners_(mouse_listeners),
      x_control_display_(NULL),
      x_record_display_(NULL),
      x_record_context_(0) {
  x_record_range_[0] = NULL;
  x_record_range_[1] = NULL;
}

UserInputMonitorLinuxCore::~UserInputMonitorLinuxCore() {
  DCHECK(!x_control_display_);
  DCHECK(!x_record_display_);
  DCHECK(!x_record_range_[0]);
  DCHECK(!x_record_range_[1]);
  DCHECK(!x_record_context_);
}

void UserInputMonitorLinuxCore::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  StopMonitor(MOUSE_EVENT);
  StopMonitor(KEYBOARD_EVENT);
}

size_t UserInputMonitorLinuxCore::GetKeyPressCount() const {
  return counter_.GetKeyPressCount();
}

void UserInputMonitorLinuxCore::StartMonitor(EventType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (type == KEYBOARD_EVENT)
    counter_.Reset();

  // TODO(jamiewalch): We should pass the display in. At that point, since
  // XRecord needs a private connection to the X Server for its data channel
  // and both channels are used from a separate thread, we'll need to duplicate
  // them with something like the following:
  //   XOpenDisplay(DisplayString(display));
  if (!x_control_display_)
    x_control_display_ = gfx::OpenNewXDisplay();

  if (!x_record_display_)
    x_record_display_ = gfx::OpenNewXDisplay();

  if (!x_control_display_ || !x_record_display_) {
    LOG(ERROR) << "Couldn't open X display";
    StopMonitor(type);
    return;
  }

  int xr_opcode, xr_event, xr_error;
  if (!XQueryExtension(
           x_control_display_, "RECORD", &xr_opcode, &xr_event, &xr_error)) {
    LOG(ERROR) << "X Record extension not available.";
    StopMonitor(type);
    return;
  }

  if (!x_record_range_[type])
    x_record_range_[type] = XRecordAllocRange();

  if (!x_record_range_[type]) {
    LOG(ERROR) << "XRecordAllocRange failed.";
    StopMonitor(type);
    return;
  }

  if (type == MOUSE_EVENT) {
    x_record_range_[type]->device_events.first = MotionNotify;
    x_record_range_[type]->device_events.last = MotionNotify;
  } else {
    DCHECK_EQ(KEYBOARD_EVENT, type);
    x_record_range_[type]->device_events.first = KeyPress;
    x_record_range_[type]->device_events.last = KeyRelease;
  }

  if (x_record_context_) {
    XRecordDisableContext(x_control_display_, x_record_context_);
    XFlush(x_control_display_);
    XRecordFreeContext(x_record_display_, x_record_context_);
    x_record_context_ = 0;
  }
  XRecordRange** record_range_to_use =
      (x_record_range_[0] && x_record_range_[1]) ? x_record_range_
                                                 : &x_record_range_[type];
  int number_of_ranges = (x_record_range_[0] && x_record_range_[1]) ? 2 : 1;

  XRecordClientSpec client_spec = XRecordAllClients;
  x_record_context_ = XRecordCreateContext(x_record_display_,
                                           0,
                                           &client_spec,
                                           1,
                                           record_range_to_use,
                                           number_of_ranges);
  if (!x_record_context_) {
    LOG(ERROR) << "XRecordCreateContext failed.";
    StopMonitor(type);
    return;
  }

  if (!XRecordEnableContextAsync(x_record_display_,
                                 x_record_context_,
                                 &UserInputMonitorLinuxCore::ProcessReply,
                                 reinterpret_cast<XPointer>(this))) {
    LOG(ERROR) << "XRecordEnableContextAsync failed.";
    StopMonitor(type);
    return;
  }

  if (!x_record_range_[0] || !x_record_range_[1]) {
    // Register OnFileCanReadWithoutBlocking() to be called every time there is
    // something to read from |x_record_display_|.
    base::MessageLoopForIO* message_loop = base::MessageLoopForIO::current();
    int result =
        message_loop->WatchFileDescriptor(ConnectionNumber(x_record_display_),
                                          true,
                                          base::MessageLoopForIO::WATCH_READ,
                                          &controller_,
                                          this);
    if (!result) {
      LOG(ERROR) << "Failed to create X record task.";
      StopMonitor(type);
      return;
    }

    // Start observing message loop destruction if we start monitoring the first
    // event.
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  // Fetch pending events if any.
  OnFileCanReadWithoutBlocking(ConnectionNumber(x_record_display_));
}

void UserInputMonitorLinuxCore::StopMonitor(EventType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (x_record_range_[type]) {
    XFree(x_record_range_[type]);
    x_record_range_[type] = NULL;
  }
  if (x_record_range_[0] || x_record_range_[1])
    return;

  // Context must be disabled via the control channel because we can't send
  // any X protocol traffic over the data channel while it's recording.
  if (x_record_context_) {
    XRecordDisableContext(x_control_display_, x_record_context_);
    XFlush(x_control_display_);
    XRecordFreeContext(x_record_display_, x_record_context_);
    x_record_context_ = 0;

    controller_.StopWatchingFileDescriptor();
  }
  if (x_record_display_) {
    XCloseDisplay(x_record_display_);
    x_record_display_ = NULL;
  }
  if (x_control_display_) {
    XCloseDisplay(x_control_display_);
    x_control_display_ = NULL;
  }
  // Stop observing message loop destruction if no event is being monitored.
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void UserInputMonitorLinuxCore::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  XEvent event;
  // Fetch pending events if any.
  while (XPending(x_record_display_)) {
    XNextEvent(x_record_display_, &event);
  }
}

void UserInputMonitorLinuxCore::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void UserInputMonitorLinuxCore::ProcessXEvent(xEvent* event) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (event->u.u.type == MotionNotify) {
    SkIPoint position(SkIPoint::Make(event->u.keyButtonPointer.rootX,
                                     event->u.keyButtonPointer.rootY));
    mouse_listeners_->Notify(
        &UserInputMonitor::MouseEventListener::OnMouseMoved, position);
  } else {
    ui::EventType type;
    if (event->u.u.type == KeyPress) {
      type = ui::ET_KEY_PRESSED;
    } else if (event->u.u.type == KeyRelease) {
      type = ui::ET_KEY_RELEASED;
    } else {
      NOTREACHED();
      return;
    }

    KeySym key_sym =
        XkbKeycodeToKeysym(x_control_display_, event->u.u.detail, 0, 0);
    ui::KeyboardCode key_code = ui::KeyboardCodeFromXKeysym(key_sym);
    counter_.OnKeyboardEvent(type, key_code);
  }
}

// static
void UserInputMonitorLinuxCore::ProcessReply(XPointer self,
                                             XRecordInterceptData* data) {
  if (data->category == XRecordFromServer) {
    xEvent* event = reinterpret_cast<xEvent*>(data->data);
    reinterpret_cast<UserInputMonitorLinuxCore*>(self)->ProcessXEvent(event);
  }
  XRecordFreeData(data);
}

//
// Implementation of UserInputMonitorLinux.
//

UserInputMonitorLinux::UserInputMonitorLinux(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      core_(new UserInputMonitorLinuxCore(io_task_runner, mouse_listeners())) {}

UserInputMonitorLinux::~UserInputMonitorLinux() {
  if (!io_task_runner_->DeleteSoon(FROM_HERE, core_))
    delete core_;
}

size_t UserInputMonitorLinux::GetKeyPressCount() const {
  return core_->GetKeyPressCount();
}

void UserInputMonitorLinux::StartKeyboardMonitoring() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserInputMonitorLinuxCore::StartMonitor,
                 core_->AsWeakPtr(),
                 UserInputMonitorLinuxCore::KEYBOARD_EVENT));
}

void UserInputMonitorLinux::StopKeyboardMonitoring() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UserInputMonitorLinuxCore::StopMonitor,
                 core_->AsWeakPtr(),
                 UserInputMonitorLinuxCore::KEYBOARD_EVENT));
}

void UserInputMonitorLinux::StartMouseMonitoring() {
  io_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&UserInputMonitorLinuxCore::StartMonitor,
                                       core_->AsWeakPtr(),
                                       UserInputMonitorLinuxCore::MOUSE_EVENT));
}

void UserInputMonitorLinux::StopMouseMonitoring() {
  io_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&UserInputMonitorLinuxCore::StopMonitor,
                                       core_->AsWeakPtr(),
                                       UserInputMonitorLinuxCore::MOUSE_EVENT));
}

}  // namespace

scoped_ptr<UserInputMonitor> UserInputMonitor::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  return scoped_ptr<UserInputMonitor>(
      new UserInputMonitorLinux(io_task_runner));
}

}  // namespace media
