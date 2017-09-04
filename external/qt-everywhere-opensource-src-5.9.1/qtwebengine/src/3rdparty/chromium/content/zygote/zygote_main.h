// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
#define CONTENT_ZYGOTE_ZYGOTE_MAIN_H_

template <typename>
class ScopedVector;

namespace content {

struct MainFunctionParams;
class ZygoteForkDelegate;

bool ZygoteMain(const MainFunctionParams& params,
                ScopedVector<ZygoteForkDelegate> fork_delegates);

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_MAIN_H_
