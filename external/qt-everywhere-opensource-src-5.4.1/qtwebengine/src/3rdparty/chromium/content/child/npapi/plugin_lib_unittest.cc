// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_lib.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Test the unloading of plugin libs. Bug http://crbug.com/46526 showed that
// if UnloadAllPlugins() simply iterates through the g_loaded_libs global
// variable, we can get a crash if no plugin libs were marked as always loaded.
class PluginLibTest : public PluginLib {
 public:
  PluginLibTest() : PluginLib(WebPluginInfo()) {}
  using PluginLib::Unload;

 protected:
  virtual ~PluginLibTest() {}
};

TEST(PluginLibLoading, UnloadAllPlugins) {
  // For the creation of the g_loaded_libs global variable.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(base::FilePath()));

  // Try with a single plugin lib.
  scoped_refptr<PluginLibTest> plugin_lib1(new PluginLibTest());
  PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(base::FilePath()));

  // Try with two plugin libs.
  plugin_lib1 = new PluginLibTest();
  scoped_refptr<PluginLibTest> plugin_lib2(new PluginLibTest());
  PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(base::FilePath()));

  // Now try to manually Unload one and then UnloadAll.
  plugin_lib1 = new PluginLibTest();
  plugin_lib2 = new PluginLibTest();
  plugin_lib1->Unload();
  PluginLib::UnloadAllPlugins();

  // Need to create it again, it should have been destroyed above.
  ASSERT_EQ(static_cast<PluginLibTest*>(NULL),
      PluginLibTest::CreatePluginLib(base::FilePath()));

  // Now try to manually Unload the only one and then UnloadAll.
  plugin_lib1 = new PluginLibTest();
  plugin_lib1->Unload();
  PluginLib::UnloadAllPlugins();
}

}  // namespace content
