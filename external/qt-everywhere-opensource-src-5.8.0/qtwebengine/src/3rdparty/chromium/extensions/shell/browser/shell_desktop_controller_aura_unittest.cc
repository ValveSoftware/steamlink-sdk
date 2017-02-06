// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/aura/test/aura_test_base.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#endif

namespace extensions {

class ShellDesktopControllerAuraTest : public aura::test::AuraTestBase {
 public:
  ShellDesktopControllerAuraTest()
#if defined(OS_CHROMEOS)
      : power_manager_client_(NULL)
#endif
      {
  }
  ~ShellDesktopControllerAuraTest() override {}

  void SetUp() override {
#if defined(OS_CHROMEOS)
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new chromeos::FakePowerManagerClient();
    dbus_setter->SetPowerManagerClient(base::WrapUnique(power_manager_client_));
#endif
    aura::test::AuraTestBase::SetUp();
    controller_.reset(new ShellDesktopControllerAura());
  }

  void TearDown() override {
    controller_.reset();
    aura::test::AuraTestBase::TearDown();
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif
  }

 protected:
  std::unique_ptr<ShellDesktopControllerAura> controller_;

#if defined(OS_CHROMEOS)
  chromeos::FakePowerManagerClient* power_manager_client_;  // Not owned.
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAuraTest);
};

#if defined(OS_CHROMEOS)
// Tests that a shutdown request is sent to the power manager when the power
// button is pressed.
TEST_F(ShellDesktopControllerAuraTest, PowerButton) {
  // Ignore button releases.
  power_manager_client_->SendPowerButtonEvent(false /* down */,
                                              base::TimeTicks());
  EXPECT_EQ(0, power_manager_client_->num_request_shutdown_calls());

  // A button press should trigger a shutdown request.
  power_manager_client_->SendPowerButtonEvent(true /* down */,
                                              base::TimeTicks());
  EXPECT_EQ(1, power_manager_client_->num_request_shutdown_calls());
}
#endif

}  // namespace extensions
