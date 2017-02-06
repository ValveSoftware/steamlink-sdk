// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_

#include <memory>

#include "base/callback_forward.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/shell/public/cpp/shell_client.h"

namespace media {

// Creates a MojoMediaApplication instance using the default MojoMediaClient.
std::unique_ptr<shell::ShellClient> MEDIA_MOJO_EXPORT
CreateMojoMediaApplication(const base::Closure& quit_closure);

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_
