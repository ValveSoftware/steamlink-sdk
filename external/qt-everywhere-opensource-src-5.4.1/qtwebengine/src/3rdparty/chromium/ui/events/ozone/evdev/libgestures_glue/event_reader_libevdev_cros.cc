// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"

#include <errno.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace ui {

namespace {

std::string FormatLog(const char* fmt, va_list args) {
  std::string msg = base::StringPrintV(fmt, args);
  if (!msg.empty() && msg[msg.size() - 1] == '\n')
    msg.erase(msg.end() - 1, msg.end());
  return msg;
}

}  // namespace

EventReaderLibevdevCros::EventReaderLibevdevCros(int fd,
                                                 const base::FilePath& path,
                                                 scoped_ptr<Delegate> delegate)
    : path_(path), delegate_(delegate.Pass()) {
  memset(&evdev_, 0, sizeof(evdev_));
  evdev_.log = OnLogMessage;
  evdev_.log_udata = this;
  evdev_.syn_report = OnSynReport;
  evdev_.syn_report_udata = this;
  evdev_.fd = fd;

  memset(&evstate_, 0, sizeof(evstate_));
  evdev_.evstate = &evstate_;
  Event_Init(&evdev_);

  Event_Open(&evdev_);

  delegate_->OnLibEvdevCrosOpen(&evdev_, &evstate_);
}

EventReaderLibevdevCros::~EventReaderLibevdevCros() {
  Stop();
  EvdevClose(&evdev_);
}

EventReaderLibevdevCros::Delegate::~Delegate() {}

void EventReaderLibevdevCros::Start() {
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      evdev_.fd,
      true,
      base::MessagePumpLibevent::WATCH_READ,
      &controller_,
      this);
}

void EventReaderLibevdevCros::Stop() {
  controller_.StopWatchingFileDescriptor();
}

void EventReaderLibevdevCros::OnFileCanReadWithoutBlocking(int fd) {
  if (EvdevRead(&evdev_)) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }
}

void EventReaderLibevdevCros::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

// static
void EventReaderLibevdevCros::OnSynReport(void* data,
                                          EventStateRec* evstate,
                                          struct timeval* tv) {
  EventReaderLibevdevCros* reader = static_cast<EventReaderLibevdevCros*>(data);
  reader->delegate_->OnLibEvdevCrosEvent(&reader->evdev_, evstate, *tv);
}

// static
void EventReaderLibevdevCros::OnLogMessage(void* data,
                                           int level,
                                           const char* fmt,
                                           ...) {
  va_list args;
  va_start(args, fmt);
  if (level >= LOGLEVEL_ERROR)
    LOG(ERROR) << "libevdev: " << FormatLog(fmt, args);
  else if (level >= LOGLEVEL_WARNING)
    LOG(WARNING) << "libevdev: " << FormatLog(fmt, args);
  else
    VLOG(3) << "libevdev: " << FormatLog(fmt, args);
  va_end(args);
}

}  // namespace ui
