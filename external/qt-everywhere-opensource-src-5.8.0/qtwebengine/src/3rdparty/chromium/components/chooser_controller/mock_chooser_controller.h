// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_
#define COMPONENTS_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "components/chooser_controller/chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockChooserController : public ChooserController {
 public:
  explicit MockChooserController(content::RenderFrameHost* owner);
  ~MockChooserController() override;

  // ChooserController:
  size_t NumOptions() const override;
  const base::string16& GetOption(size_t index) const override;
  MOCK_METHOD1(Select, void(size_t index));
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(OpenHelpCenterUrl, void());

  void OptionAdded(const base::string16 option_name);
  void OptionRemoved(const base::string16 option_name);

 private:
  std::vector<base::string16> option_names_;

  DISALLOW_COPY_AND_ASSIGN(MockChooserController);
};

#endif  // COMPONENTS_CHOOSER_CONTROLLER_MOCK_CHOOSER_CONTROLLER_H_
