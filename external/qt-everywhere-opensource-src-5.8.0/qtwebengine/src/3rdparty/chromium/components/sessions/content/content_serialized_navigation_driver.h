// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_

#include "components/sessions/core/serialized_navigation_driver.h"

#include "components/sessions/core/sessions_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace sessions {

// Provides an implementation of SerializedNavigationDriver that is backed by
// content classes.
class SESSIONS_EXPORT ContentSerializedNavigationDriver
    : public SerializedNavigationDriver {
 public:
  ~ContentSerializedNavigationDriver() override;

  // Returns the singleton ContentSerializedNavigationDriver.  Almost all
  // callers should use SerializedNavigationDriver::Get() instead.
  static ContentSerializedNavigationDriver* GetInstance();

  // SerializedNavigationDriver implementation.
  int GetDefaultReferrerPolicy() const override;
  bool MapReferrerPolicyToOldValues(int referrer_policy,
                                    int* mapped_referrer_policy) const override;
  bool MapReferrerPolicyToNewValues(int referrer_policy,
                                    int* mapped_referrer_policy) const override;
  std::string GetSanitizedPageStateForPickle(
      const SerializedNavigationEntry* navigation) const override;
  void Sanitize(SerializedNavigationEntry* navigation) const override;
  std::string StripReferrerFromPageState(
      const std::string& page_state) const override;

 private:
  ContentSerializedNavigationDriver();
  friend struct base::DefaultSingletonTraits<ContentSerializedNavigationDriver>;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_DRIVER_H_
