// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_KEEP_ALIVE_H_
#define MOJO_SHELL_KEEP_ALIVE_H_

#include "base/basictypes.h"

namespace mojo {
namespace shell {

class Context;
class KeepAlive;

class KeepAliveCounter {
 public:
  KeepAliveCounter() : count_(0) {
  }
 private:
  friend class KeepAlive;
  int count_;
};

// Instantiate this class to extend the lifetime of the thread associated
// with |context| (i.e., the shell's UI thread). Must only be used from
// the shell's UI thread.
class KeepAlive {
 public:
  explicit KeepAlive(Context* context);
  ~KeepAlive();

 private:
  static void MaybeQuit(Context* context);

  Context* context_;

  DISALLOW_COPY_AND_ASSIGN(KeepAlive);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_KEEP_ALIVE_H_
