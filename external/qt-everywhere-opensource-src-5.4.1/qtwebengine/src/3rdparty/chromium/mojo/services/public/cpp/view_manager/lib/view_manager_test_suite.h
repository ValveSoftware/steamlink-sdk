// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_TEST_SUITE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace mojo {
namespace view_manager {

class ViewManagerTestSuite : public base::TestSuite {
 public:
  ViewManagerTestSuite(int argc, char** argv);
  virtual ~ViewManagerTestSuite();

 protected:
  virtual void Initialize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewManagerTestSuite);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_TEST_SUITE_H_
