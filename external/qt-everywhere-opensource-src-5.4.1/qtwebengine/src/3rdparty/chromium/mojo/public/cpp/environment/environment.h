// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_
#define MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_

#include "mojo/public/cpp/system/macros.h"

struct MojoAsyncWaiter;
struct MojoLogger;

namespace mojo {

// Other parts of the Mojo C++ APIs use the *static* methods of this class.
//
// The "standalone" implementation of this class requires that this class (in
// the lib/ subdirectory) be instantiated (and remain so) while using the Mojo
// C++ APIs. I.e., the static methods depend on things set up by the constructor
// and torn down by the destructor.
//
// Other implementations may not have this requirement.
class Environment {
 public:
  Environment();
  // This constructor allows the standard implementations to be overridden (set
  // a parameter to null to get the standard implementation).
  Environment(const MojoAsyncWaiter* default_async_waiter,
              const MojoLogger* default_logger);
  ~Environment();

  static const MojoAsyncWaiter* GetDefaultAsyncWaiter();
  static const MojoLogger* GetDefaultLogger();

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_ENVIRONMENT_ENVIRONMENT_H_
