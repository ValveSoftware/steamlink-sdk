// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_device_handle.h"

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"

namespace ui {

namespace {

bool Authenticate(int fd) {
  drm_magic_t magic;
  memset(&magic, 0, sizeof(magic));
  // We need to make sure the DRM device has enough privilege. Use the DRM
  // authentication logic to figure out if the device has enough permissions.
  return !drmGetMagic(fd, &magic) && !drmAuthMagic(fd, magic);
}

}  // namespace

DrmDeviceHandle::DrmDeviceHandle() {
}

DrmDeviceHandle::~DrmDeviceHandle() {
  if (file_.is_valid())
    base::ThreadRestrictions::AssertIOAllowed();
}

bool DrmDeviceHandle::Initialize(const base::FilePath& dev_path,
                                 const base::FilePath& sys_path) {
  // Security folks have requested that we assert the graphics device has the
  // expected path, so use a CHECK instead of a DCHECK. The sys_path is only
  // used a label and is otherwise unvalidated.
  CHECK(dev_path.DirName() == base::FilePath("/dev/dri"));
  base::ThreadRestrictions::AssertIOAllowed();
  bool print_warning = true;
  while (true) {
    file_.reset();
    int fd = HANDLE_EINTR(open(dev_path.value().c_str(), O_RDWR | O_CLOEXEC));
    if (fd < 0) {
      PLOG(ERROR) << "Failed to open " << dev_path.value();
      return false;
    }

    file_.reset(fd);
    sys_path_ = sys_path;

    if (Authenticate(file_.get()))
      break;

    LOG_IF(WARNING, print_warning) << "Failed to authenticate "
                                   << dev_path.value();
    print_warning = false;
    usleep(100000);
  }

  VLOG(1) << "Succeeded authenticating " << dev_path.value();
  return true;
}

bool DrmDeviceHandle::IsValid() const {
  return file_.is_valid();
}

base::ScopedFD DrmDeviceHandle::PassFD() {
  return std::move(file_);
}

}  // namespace ui
