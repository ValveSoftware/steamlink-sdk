// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_
#define COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {

// Trivial implementation of UserInfo interface which triggers
// NOTIMPLEMENTED() for each method.
class USER_MANAGER_EXPORT EmptyUserInfo : public UserInfo {
 public:
  EmptyUserInfo();
  ~EmptyUserInfo() override;

  // UserInfo:
  base::string16 GetDisplayName() const override;
  base::string16 GetGivenName() const override;
  std::string GetEmail() const override;
  const AccountId& GetAccountId() const override;
  const gfx::ImageSkia& GetImage() const override;

 private:
  const gfx::ImageSkia null_image_;

  DISALLOW_COPY_AND_ASSIGN(EmptyUserInfo);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_
