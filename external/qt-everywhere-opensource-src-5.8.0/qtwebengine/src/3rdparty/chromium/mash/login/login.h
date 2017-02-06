// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_LOGIN_LOGIN_H_
#define MASH_LOGIN_LOGIN_H_

namespace shell {
class ShellClient;
}

namespace mash {
namespace login {

shell::ShellClient* CreateLogin();

}  // namespace login
}  // namespace mash

#endif  // MASH_LOGIN_LOGIN_H_
