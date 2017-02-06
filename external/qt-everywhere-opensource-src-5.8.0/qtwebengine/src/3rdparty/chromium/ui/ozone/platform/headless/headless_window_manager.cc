// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window_manager.h"

#include "base/files/file_util.h"
#include "base/location.h"

namespace ui {

HeadlessWindowManager::HeadlessWindowManager(
    const base::FilePath& dump_location)
    : location_(dump_location) {}

HeadlessWindowManager::~HeadlessWindowManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HeadlessWindowManager::Initialize() {
  if (location_.empty())
    return;
  if (!DirectoryExists(location_) && !base::CreateDirectory(location_) &&
      location_ != base::FilePath("/dev/null"))
    PLOG(FATAL) << "unable to create output directory";
  if (!base::PathIsWritable(location_))
    PLOG(FATAL) << "unable to write to output location";
}

int32_t HeadlessWindowManager::AddWindow(HeadlessWindow* window) {
  return windows_.Add(window);
}

void HeadlessWindowManager::RemoveWindow(int32_t window_id,
                                         HeadlessWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

HeadlessWindow* HeadlessWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

base::FilePath HeadlessWindowManager::base_path() const {
  return location_;
}

}  // namespace ui
