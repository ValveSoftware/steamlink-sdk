// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/test_content_main_delegate.h"

namespace blimp {
namespace engine {
namespace {
TestContentMainDelegate* g_content_main_delegate = nullptr;
}  // namespace

TestContentMainDelegate::TestContentMainDelegate() {
  DCHECK(!g_content_main_delegate);
  g_content_main_delegate = this;
}

TestContentMainDelegate::~TestContentMainDelegate() {
  DCHECK_EQ(this, g_content_main_delegate);
  g_content_main_delegate = nullptr;
}

// static
TestContentMainDelegate* TestContentMainDelegate::GetInstance() {
  return g_content_main_delegate;
}

}  // namespace engine
}  // namespace blimp
