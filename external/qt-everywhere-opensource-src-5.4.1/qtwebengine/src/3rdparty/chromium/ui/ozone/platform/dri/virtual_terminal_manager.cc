// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/virtual_terminal_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/logging.h"

namespace ui {

namespace {

const char kTTYDevice[] = "/dev/tty1";

const int kVT = 1;

}  // namespace

VirtualTerminalManager::VirtualTerminalManager() {
  // Use the current console.
  fd_ = open(kTTYDevice, O_RDWR | O_CLOEXEC, 0);
  if (fd_ < 0)
    LOG(ERROR) << "Failed to open '" << kTTYDevice << "' " << strerror(errno);

  if (ioctl(fd_, VT_ACTIVATE, kVT) || ioctl(fd_, VT_WAITACTIVE, kVT))
    LOG(ERROR) << "Failed to switch to VT: " << kVT
               << " error: " << strerror(errno);;

  if (ioctl(fd_, KDGETMODE, &vt_mode_))
    LOG(ERROR) << "Failed to get VT mode: " << strerror(errno);

  if (ioctl(fd_, KDSETMODE, KD_GRAPHICS))
    LOG(ERROR) << "Failed to set graphics mode: " << strerror(errno);

  if (tcgetattr(fd_, &terminal_attributes_))
    LOG(ERROR) << "Failed to get terminal attributes";

  // Stop the TTY from processing keys and echo-ing them to the terminal.
  struct termios raw_attributes = terminal_attributes_;
  cfmakeraw(&raw_attributes);
  raw_attributes.c_oflag |= OPOST;
  if (tcsetattr(fd_, TCSANOW, &raw_attributes))
    LOG(ERROR) << "Failed to set raw attributes";

  if (ioctl(fd_, KDGKBMODE, &previous_keyboard_mode_))
    LOG(ERROR) << "Failed to get keyboard mode";

  if (ioctl(fd_, KDSKBMODE, K_OFF) && ioctl(fd_, KDSKBMODE, K_RAW))
    LOG(ERROR) << "Failed to set keyboard mode";
}

VirtualTerminalManager::~VirtualTerminalManager() {
  if (fd_ < 0)
    return;

  if (ioctl(fd_, KDSETMODE, &vt_mode_))
    LOG(ERROR) << "Failed to restore VT mode";

  if (ioctl(fd_, KDSKBMODE, previous_keyboard_mode_))
    LOG(ERROR) << "Failed to restore keyboard mode";

  if (tcsetattr(fd_, TCSANOW, &terminal_attributes_))
    LOG(ERROR) << "Failed to restore terminal attributes";

  close(fd_);
}

}  // namespace ui
