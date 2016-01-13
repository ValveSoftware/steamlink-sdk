// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ozone/media_ozone_platform.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace media {

namespace {

class MediaOzonePlatformStub : public MediaOzonePlatform {
 public:
  MediaOzonePlatformStub() {}

  virtual ~MediaOzonePlatformStub() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaOzonePlatformStub);
};

}  // namespace

// The following statics are just convenient stubs, declared by the
// generate_constructor_list.py script. They should be removed once the
// internal Ozone platforms decide to actually implement their media specifics.
MediaOzonePlatform* CreateMediaOzonePlatformCaca() {
  return new MediaOzonePlatformStub;
}

MediaOzonePlatform* CreateMediaOzonePlatformDri() {
  return new MediaOzonePlatformStub;
}

MediaOzonePlatform* CreateMediaOzonePlatformEgltest() {
  return new MediaOzonePlatformStub;
}

MediaOzonePlatform* CreateMediaOzonePlatformGbm() {
  return new MediaOzonePlatformStub;
}

MediaOzonePlatform* CreateMediaOzonePlatformTest() {
  return new MediaOzonePlatformStub;
}

MediaOzonePlatform::MediaOzonePlatform() {
  CHECK(!instance_) << "There should only be a single MediaOzonePlatform.";
  instance_ = this;
}

MediaOzonePlatform::~MediaOzonePlatform() {
  CHECK_EQ(instance_, this);
  instance_ = NULL;
}

// static
MediaOzonePlatform* MediaOzonePlatform::GetInstance() {
  if (!instance_)
    CreateInstance();
  return instance_;
}

VideoDecodeAccelerator* MediaOzonePlatform::CreateVideoDecodeAccelerator(
    const base::Callback<bool(void)>& make_context_current) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
void MediaOzonePlatform::CreateInstance() {
  if (instance_)
    return;

  TRACE_EVENT1("ozone",
               "MediaOzonePlatform::Initialize",
               "platform",
               ui::GetOzonePlatformName());
  scoped_ptr<MediaOzonePlatform> platform =
      ui::PlatformObject<MediaOzonePlatform>::Create();

  // TODO(spang): Currently need to leak this object.
  CHECK_EQ(instance_, platform.release());
}

// static
MediaOzonePlatform* MediaOzonePlatform::instance_;

}  // namespace media
