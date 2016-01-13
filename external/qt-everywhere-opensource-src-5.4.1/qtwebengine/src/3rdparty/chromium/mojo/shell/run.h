// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUN_H_
#define MOJO_SHELL_RUN_H_

#include <vector>

#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;

void Run(Context* context, const std::vector<GURL>& app_urls);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUN_H_
