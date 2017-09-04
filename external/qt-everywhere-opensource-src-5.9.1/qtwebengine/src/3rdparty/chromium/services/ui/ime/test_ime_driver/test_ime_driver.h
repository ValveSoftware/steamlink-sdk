// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
#define SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/ime.mojom.h"

namespace ui {
namespace test {

class TestIMEDriver : public ui::mojom::IMEDriver {
 public:
  TestIMEDriver();
  ~TestIMEDriver() override;

 private:
  // ui::mojom::IMEDriver:
  void StartSession(
      int32_t session_id,
      ui::mojom::TextInputClientPtr client,
      ui::mojom::InputMethodRequest input_method_request) override;
  void CancelSession(int32_t session_id) override;

  std::map<int32_t, std::unique_ptr<mojo::Binding<mojom::InputMethod>>>
      input_method_bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestIMEDriver);
};

}  // namespace test
}  // namespace ui

#endif  // SERVICES_UI_IME_TEST_IME_DRIVER_TEST_IME_DRIVER_H_
