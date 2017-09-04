// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/api/system_display.h"
#include "extensions/shell/test/shell_apitest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace extensions {

using api::system_display::Bounds;
using api::system_display::DisplayUnitInfo;
using display::Screen;

class MockScreen : public Screen {
 public:
  MockScreen() {
    for (int i = 0; i < 4; i++) {
      gfx::Rect bounds(0, 0, 1280, 720);
      gfx::Rect work_area(0, 0, 960, 720);
      display::Display display(i, bounds);
      display.set_work_area(work_area);
      displays_.push_back(display);
    }
  }
  ~MockScreen() override {}

 protected:
  // Overridden from display::Screen:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return gfx::NativeWindow();
  }
  int GetNumDisplays() const override {
    return static_cast<int>(displays_.size());
  }
  std::vector<display::Display> GetAllDisplays() const override {
    return displays_;
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override {
    return display::Display(0);
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return display::Display(0);
  }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    return display::Display(0);
  }
  display::Display GetPrimaryDisplay() const override { return displays_[0]; }
  void AddObserver(display::DisplayObserver* observer) override {}
  void RemoveObserver(display::DisplayObserver* observer) override {}

 private:
  std::vector<display::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(MockScreen);
};

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  MockDisplayInfoProvider() {}

  ~MockDisplayInfoProvider() override {}

  bool SetInfo(const std::string& display_id,
               const api::system_display::DisplayProperties& params,
               std::string* error) override {
    // Should get called only once per test case.
    EXPECT_FALSE(set_info_value_);
    set_info_value_ = params.ToValue();
    set_info_display_id_ = display_id;
    return true;
  }

  void EnableUnifiedDesktop(bool enable) override {
    unified_desktop_enabled_ = enable;
  }

  std::unique_ptr<base::DictionaryValue> GetSetInfoValue() {
    return std::move(set_info_value_);
  }

  std::string GetSetInfoDisplayId() const { return set_info_display_id_; }

  bool unified_desktop_enabled() const { return unified_desktop_enabled_; }

 private:
  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      extensions::api::system_display::DisplayUnitInfo* unit) override {
    int64_t id = display.id();
    unit->name = "DISPLAY NAME FOR " + base::Int64ToString(id);
    if (id == 1)
      unit->mirroring_source_id = "0";
    unit->is_primary = id == 0 ? true : false;
    unit->is_internal = id == 0 ? true : false;
    unit->is_enabled = true;
    unit->rotation = (90 * id) % 360;
    unit->dpi_x = 96.0;
    unit->dpi_y = 96.0;
    if (id == 0) {
      unit->overscan.left = 20;
      unit->overscan.top = 40;
      unit->overscan.right = 60;
      unit->overscan.bottom = 80;
    }
  }

  std::unique_ptr<base::DictionaryValue> set_info_value_;
  std::string set_info_display_id_;
  bool unified_desktop_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockDisplayInfoProvider);
};

class SystemDisplayApiTest : public ShellApiTest {
 public:
  SystemDisplayApiTest()
      : provider_(new MockDisplayInfoProvider), screen_(new MockScreen) {}

  ~SystemDisplayApiTest() override {}

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    ANNOTATE_LEAKING_OBJECT_PTR(display::Screen::GetScreen());
    display::Screen::SetScreenInstance(screen_.get());
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

 protected:
  std::unique_ptr<MockDisplayInfoProvider> provider_;
  std::unique_ptr<display::Screen> screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemDisplayApiTest);
};

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, GetDisplayInfo) {
  ASSERT_TRUE(RunAppTest("system/display/info")) << message_;
}

#if !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplay) {
  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);

  EXPECT_EQ(
      SystemDisplayFunction::kCrosOnlyError,
      api_test_utils::RunFunctionAndReturnError(
          set_info_function.get(), "[\"display_id\", {}]", browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

#else  // !defined(OS_CHROMEOS)

// TODO(stevenjb): Add API tests for {GS}etDisplayLayout. That code currently
// lives in src/chrome but should be getting moved soon.

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayNotKioskEnabled) {
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary("{\n"
                                      "  \"name\": \"Test\",\n"
                                      "  \"version\": \"1.0\",\n"
                                      "  \"app\": {\n"
                                      "    \"background\": {\n"
                                      "      \"scripts\": [\"background.js\"]\n"
                                      "    }\n"
                                      "  }\n"
                                      "}"));
  scoped_refptr<Extension> test_extension(
      api_test_utils::CreateExtension(test_extension_value.get()));

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_extension(test_extension.get());
  set_info_function->set_has_callback(true);

  EXPECT_EQ(
      SystemDisplayFunction::kKioskOnlyError,
      api_test_utils::RunFunctionAndReturnError(
          set_info_function.get(), "[\"display_id\", {}]", browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, SetDisplayKioskEnabled) {
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary("{\n"
                                      "  \"name\": \"Test\",\n"
                                      "  \"version\": \"1.0\",\n"
                                      "  \"app\": {\n"
                                      "    \"background\": {\n"
                                      "      \"scripts\": [\"background.js\"]\n"
                                      "    }\n"
                                      "  },\n"
                                      "  \"kiosk_enabled\": true\n"
                                      "}"));
  scoped_refptr<Extension> test_extension(
      api_test_utils::CreateExtension(test_extension_value.get()));

  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);
  set_info_function->set_extension(test_extension.get());

  ASSERT_TRUE(api_test_utils::RunFunction(
      set_info_function.get(),
      "[\"display_id\", {\n"
      "  \"isPrimary\": true,\n"
      "  \"mirroringSourceId\": \"mirroringId\",\n"
      "  \"boundsOriginX\": 100,\n"
      "  \"boundsOriginY\": 200,\n"
      "  \"rotation\": 90,\n"
      "  \"overscan\": {\"left\": 1, \"top\": 2, \"right\": 3, \"bottom\": 4}\n"
      "}]",
      browser_context()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  ASSERT_TRUE(set_info);
  EXPECT_TRUE(api_test_utils::GetBoolean(set_info.get(), "isPrimary"));
  EXPECT_EQ("mirroringId",
            api_test_utils::GetString(set_info.get(), "mirroringSourceId"));
  EXPECT_EQ(100, api_test_utils::GetInteger(set_info.get(), "boundsOriginX"));
  EXPECT_EQ(200, api_test_utils::GetInteger(set_info.get(), "boundsOriginY"));
  EXPECT_EQ(90, api_test_utils::GetInteger(set_info.get(), "rotation"));
  base::DictionaryValue* overscan;
  ASSERT_TRUE(set_info->GetDictionary("overscan", &overscan));
  EXPECT_EQ(1, api_test_utils::GetInteger(overscan, "left"));
  EXPECT_EQ(2, api_test_utils::GetInteger(overscan, "top"));
  EXPECT_EQ(3, api_test_utils::GetInteger(overscan, "right"));
  EXPECT_EQ(4, api_test_utils::GetInteger(overscan, "bottom"));

  EXPECT_EQ("display_id", provider_->GetSetInfoDisplayId());
}

IN_PROC_BROWSER_TEST_F(SystemDisplayApiTest, EnableUnifiedDesktop) {
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary("{\n"
                                      "  \"name\": \"Test\",\n"
                                      "  \"version\": \"1.0\",\n"
                                      "  \"app\": {\n"
                                      "    \"background\": {\n"
                                      "      \"scripts\": [\"background.js\"]\n"
                                      "    }\n"
                                      "  },\n"
                                      "  \"kiosk_enabled\": true\n"
                                      "}"));
  scoped_refptr<Extension> test_extension(
      api_test_utils::CreateExtension(test_extension_value.get()));
  {
    scoped_refptr<SystemDisplayEnableUnifiedDesktopFunction>
        enable_unified_function(
            new SystemDisplayEnableUnifiedDesktopFunction());

    enable_unified_function->set_has_callback(true);
    enable_unified_function->set_extension(test_extension.get());

    EXPECT_FALSE(provider_->unified_desktop_enabled());

    ASSERT_TRUE(api_test_utils::RunFunction(enable_unified_function.get(),
                                            "[true]", browser_context()));
    EXPECT_TRUE(provider_->unified_desktop_enabled());
  }
  {
    scoped_refptr<SystemDisplayEnableUnifiedDesktopFunction>
        enable_unified_function(
            new SystemDisplayEnableUnifiedDesktopFunction());

    enable_unified_function->set_has_callback(true);
    enable_unified_function->set_extension(test_extension.get());
    ASSERT_TRUE(api_test_utils::RunFunction(enable_unified_function.get(),
                                            "[false]", browser_context()));
    EXPECT_FALSE(provider_->unified_desktop_enabled());
  }
}

#endif  // !defined(OS_CHROMEOS)

}  // namespace extensions
