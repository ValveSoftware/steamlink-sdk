// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_GCM_HANDLER_H_
#define COMPONENTS_COPRESENCE_HANDLERS_GCM_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace gcm {
class GCMDriver;
}

namespace copresence {

// A class to handle GCM messages from the Copresence server. As far as the
// rest of the system is concerned, it simply provides the registered GCM ID.
// The implementation will call other classes to enact the message contents.
class GCMHandler {
 public:
  // Callback to report that registration has completed.
  // Returns an empty ID if registration failed.
  using RegistrationCallback = base::Callback<void(const std::string&)>;

  GCMHandler() {}
  virtual ~GCMHandler() {}

  // Request the GCM ID. It may be returned now or later, via the callback.
  virtual void GetGcmId(const RegistrationCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMHandler);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_GCM_HANDLER_H_
