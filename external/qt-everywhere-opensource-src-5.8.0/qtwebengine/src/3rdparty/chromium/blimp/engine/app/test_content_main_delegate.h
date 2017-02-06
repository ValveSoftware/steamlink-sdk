// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_TEST_CONTENT_MAIN_DELEGATE_H_
#define BLIMP_ENGINE_APP_TEST_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "blimp/engine/app/blimp_content_main_delegate.h"

namespace blimp {
namespace engine {

class TestContentMainDelegate : public BlimpContentMainDelegate {
 public:
  TestContentMainDelegate();
  ~TestContentMainDelegate() override;

  static TestContentMainDelegate* GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentMainDelegate);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_TEST_CONTENT_MAIN_DELEGATE_H_
