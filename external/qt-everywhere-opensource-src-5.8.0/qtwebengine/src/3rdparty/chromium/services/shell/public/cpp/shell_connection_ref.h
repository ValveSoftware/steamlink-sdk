// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_REF_H_
#define SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_REF_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace shell {

class ShellConnectionRefImpl;

// An interface implementation can keep this object as a member variable to
// hold a reference to the ShellConnection, keeping it alive as long as the
// bound implementation exists.
//
// This class is safe to use on any thread and instances may be passed to other
// threads. However, each instance should only be used on one thread at a time,
// otherwise there'll be races between the AddRef resulting from cloning and
// destruction.
class ShellConnectionRef {
 public:
  virtual ~ShellConnectionRef() {}

  virtual std::unique_ptr<ShellConnectionRef> Clone() = 0;
};

class ShellConnectionRefFactory {
 public:
  // |quit_closure| is called whenever the last ref is destroyed.
  explicit ShellConnectionRefFactory(const base::Closure& quit_closure);
  ~ShellConnectionRefFactory();

  std::unique_ptr<ShellConnectionRef> CreateRef();

  bool HasNoRefs() const { return !ref_count_; }

 private:
  friend ShellConnectionRefImpl;

  // Called from ShellConnectionRefImpl.
  void AddRef();
  void Release();

  const base::Closure quit_closure_;
  int ref_count_ = 0;
  base::WeakPtrFactory<ShellConnectionRefFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellConnectionRefFactory);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_REF_H_
