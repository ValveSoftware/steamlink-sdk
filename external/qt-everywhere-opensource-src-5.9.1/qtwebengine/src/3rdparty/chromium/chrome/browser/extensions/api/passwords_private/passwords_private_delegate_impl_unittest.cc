// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

template <typename T>
class CallbackTracker {
 public:
  CallbackTracker() : callback_(
      base::Bind(&CallbackTracker::Callback, base::Unretained(this))) {}

  using TypedCallback = base::Callback<void(const T&)>;

  const TypedCallback& callback() const { return callback_; }

  size_t call_count() const { return call_count_; }

 private:
  void Callback(const T& args) {
    EXPECT_FALSE(args.empty());
    ++call_count_;
  }

  size_t call_count_ = 0;

  TypedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackTracker);
};

using PasswordFormList = std::vector<std::unique_ptr<autofill::PasswordForm>>;

TEST(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  CallbackTracker<PasswordsPrivateDelegate::UiEntries> tracker;

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  PasswordsPrivateDelegateImpl delegate(&profile);

  delegate.GetSavedPasswordsList(tracker.callback());
  EXPECT_EQ(0u, tracker.call_count());

  PasswordFormList list;
  list.push_back(base::MakeUnique<autofill::PasswordForm>());
  delegate.SetPasswordList(list);
  EXPECT_EQ(1u, tracker.call_count());

  delegate.GetSavedPasswordsList(tracker.callback());
  EXPECT_EQ(2u, tracker.call_count());
}

TEST(PasswordsPrivateDelegateImplTest, GetPasswordExceptionsList) {
  CallbackTracker<PasswordsPrivateDelegate::ExceptionPairs> tracker;

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  PasswordsPrivateDelegateImpl delegate(&profile);

  delegate.GetPasswordExceptionsList(tracker.callback());
  EXPECT_EQ(0u, tracker.call_count());

  PasswordFormList list;
  list.push_back(base::MakeUnique<autofill::PasswordForm>());
  delegate.SetPasswordExceptionList(list);
  EXPECT_EQ(1u, tracker.call_count());

  delegate.GetPasswordExceptionsList(tracker.callback());
  EXPECT_EQ(2u, tracker.call_count());
}

}  // namespace extensions
