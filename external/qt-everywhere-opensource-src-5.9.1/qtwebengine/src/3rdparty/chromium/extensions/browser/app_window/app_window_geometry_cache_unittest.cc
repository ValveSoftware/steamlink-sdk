// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_geometry_cache.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

namespace {
const char kWindowId[] = "windowid";
const char kWindowId2[] = "windowid2";

// Create a very simple extension with id.
scoped_refptr<Extension> CreateExtension(const std::string& id) {
  return ExtensionBuilder()
      .SetManifest(
          DictionaryBuilder().Set("name", "test").Set("version", "0.1").Build())
      .SetID(id)
      .Build();
}

}  // namespace

// Base class for tests.
class AppWindowGeometryCacheTest : public ExtensionsTest {
 public:
  AppWindowGeometryCacheTest()
      : ui_thread_(BrowserThread::UI, &ui_message_loop_) {}

  // testing::Test overrides:
  void SetUp() override;
  void TearDown() override;

  void AddGeometryAndLoadExtension(const std::string& extension_id,
                                   const std::string& window_id,
                                   const gfx::Rect& bounds,
                                   const gfx::Rect& screen_bounds,
                                   ui::WindowShowState state);

  // Spins the UI threads' message loops to make sure any task
  // posted to sync the geometry to the value store gets a chance to run.
  void WaitForSync();

  void LoadExtension(const std::string& extension_id);
  void UnloadExtension(const std::string& extension_id);

  // Creates and adds an extension with associated prefs. Returns the extension
  // ID.
  std::string AddExtensionWithPrefs(const std::string& name);

 protected:
  base::MessageLoopForUI ui_message_loop_;
  content::TestBrowserThread ui_thread_;
  std::unique_ptr<ExtensionPrefValueMap> extension_pref_value_map_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<ExtensionPrefs> extension_prefs_;
  std::unique_ptr<AppWindowGeometryCache> cache_;
};

void AppWindowGeometryCacheTest::SetUp() {
  ExtensionsTest::SetUp();

  // Set up all the dependencies of ExtensionPrefs.
  extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore);
  factory.set_extension_prefs(new TestingPrefStore);
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable;
  // Prefs should be registered before the PrefService is created.
  ExtensionPrefs::RegisterProfilePrefs(pref_registry);
  pref_service_ = factory.Create(pref_registry);

  extension_prefs_.reset(ExtensionPrefs::Create(
      browser_context(), pref_service_.get(),
      browser_context()->GetPath().AppendASCII("Extensions"),
      extension_pref_value_map_.get(), false /* extensions_disabled */,
      std::vector<ExtensionPrefsObserver*>()));

  cache_.reset(
      new AppWindowGeometryCache(browser_context(), extension_prefs_.get()));
  cache_->SetSyncDelayForTests(0);
}

void AppWindowGeometryCacheTest::TearDown() {
  cache_.reset();
  extension_prefs_.reset();
  pref_service_.reset();
  extension_pref_value_map_.reset();

  ExtensionsTest::TearDown();
}

void AppWindowGeometryCacheTest::AddGeometryAndLoadExtension(
    const std::string& extension_id,
    const std::string& window_id,
    const gfx::Rect& bounds,
    const gfx::Rect& screen_bounds,
    ui::WindowShowState state) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::MakeUnique<base::DictionaryValue>();
  std::unique_ptr<base::DictionaryValue> value =
      base::MakeUnique<base::DictionaryValue>();
  value->SetInteger("x", bounds.x());
  value->SetInteger("y", bounds.y());
  value->SetInteger("w", bounds.width());
  value->SetInteger("h", bounds.height());
  value->SetInteger("screen_bounds_x", screen_bounds.x());
  value->SetInteger("screen_bounds_y", screen_bounds.y());
  value->SetInteger("screen_bounds_w", screen_bounds.width());
  value->SetInteger("screen_bounds_h", screen_bounds.height());
  value->SetInteger("state", state);
  dict->SetWithoutPathExpansion(window_id, std::move(value));
  extension_prefs_->SetGeometryCache(extension_id, std::move(dict));
  LoadExtension(extension_id);
}

void AppWindowGeometryCacheTest::WaitForSync() {
  content::RunAllPendingInMessageLoop();
}

void AppWindowGeometryCacheTest::LoadExtension(
    const std::string& extension_id) {
  cache_->LoadGeometryFromStorage(extension_id);
  WaitForSync();
}

void AppWindowGeometryCacheTest::UnloadExtension(
    const std::string& extension_id) {
  scoped_refptr<Extension> extension = CreateExtension(extension_id);
  cache_->OnExtensionUnloaded(browser_context(),
                              extension.get(),
                              UnloadedExtensionInfo::REASON_DISABLE);
  WaitForSync();
}

std::string AppWindowGeometryCacheTest::AddExtensionWithPrefs(
    const std::string& name) {
  // Generate the extension with a path based on the name so that extensions
  // with different names will have different IDs.
  base::FilePath path =
      browser_context()->GetPath().AppendASCII("Extensions").AppendASCII(name);
  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "test")
                           .Set("version", "0.1")
                           .Build())
          .SetPath(path)
          .Build();

  extension_prefs_->OnExtensionInstalled(
      extension.get(),
      Extension::ENABLED,
      syncer::StringOrdinal::CreateInitialOrdinal(),
      std::string());
  return extension->id();
}

// Test getting geometry from an empty store.
TEST_F(AppWindowGeometryCacheTest, GetGeometryEmptyStore) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  ASSERT_FALSE(cache_->GetGeometry(extension_id, kWindowId, NULL, NULL, NULL));
}

// Test getting geometry for an unknown extension.
TEST_F(AppWindowGeometryCacheTest, GetGeometryUnkownExtension) {
  const std::string extension_id1 = AddExtensionWithPrefs("ext1");
  const std::string extension_id2 = AddExtensionWithPrefs("ext2");
  AddGeometryAndLoadExtension(extension_id1,
                              kWindowId,
                              gfx::Rect(4, 5, 31, 43),
                              gfx::Rect(0, 0, 1600, 900),
                              ui::SHOW_STATE_NORMAL);
  ASSERT_FALSE(cache_->GetGeometry(extension_id2, kWindowId, NULL, NULL, NULL));
}

// Test getting geometry for an unknown window in a known extension.
TEST_F(AppWindowGeometryCacheTest, GetGeometryUnkownWindow) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  AddGeometryAndLoadExtension(extension_id,
                              kWindowId,
                              gfx::Rect(4, 5, 31, 43),
                              gfx::Rect(0, 0, 1600, 900),
                              ui::SHOW_STATE_NORMAL);
  ASSERT_FALSE(cache_->GetGeometry(extension_id, kWindowId2, NULL, NULL, NULL));
}

// Test that loading geometry, screen_bounds and state from the store works
// correctly.
TEST_F(AppWindowGeometryCacheTest, GetGeometryAndStateFromStore) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::WindowShowState state = ui::SHOW_STATE_NORMAL;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::WindowShowState new_state = ui::SHOW_STATE_DEFAULT;
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);
}

// Test corrupt bounds will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptBounds) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds;
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::WindowShowState state = ui::SHOW_STATE_NORMAL;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::WindowShowState new_state = ui::SHOW_STATE_DEFAULT;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::SHOW_STATE_DEFAULT);
}

// Test corrupt screen bounds will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptScreenBounds) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds;
  ui::WindowShowState state = ui::SHOW_STATE_NORMAL;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::WindowShowState new_state = ui::SHOW_STATE_DEFAULT;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::SHOW_STATE_DEFAULT);
}

// Test corrupt state will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptState) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::WindowShowState state = ui::SHOW_STATE_DEFAULT;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::WindowShowState new_state = ui::SHOW_STATE_DEFAULT;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::SHOW_STATE_DEFAULT);
}

// Test saving geometry, screen_bounds and state to the cache and state store,
// and reading it back.
TEST_F(AppWindowGeometryCacheTest, SaveGeometryAndStateToStore) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  const std::string window_id(kWindowId);

  // inform cache of extension
  LoadExtension(extension_id);

  // update geometry stored in cache
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::WindowShowState state = ui::SHOW_STATE_NORMAL;
  cache_->SaveGeometry(extension_id, window_id, bounds, screen_bounds, state);

  // make sure that immediately reading back geometry works
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::WindowShowState new_state = ui::SHOW_STATE_DEFAULT;
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, window_id, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);

  // unload extension to force cache to save data to the state store
  UnloadExtension(extension_id);

  // check if geometry got stored correctly in the state store
  const base::DictionaryValue* dict =
      extension_prefs_->GetGeometryCache(extension_id);
  ASSERT_TRUE(dict);

  ASSERT_TRUE(dict->HasKey(window_id));
  int v;
  ASSERT_TRUE(dict->GetInteger(window_id + ".x", &v));
  ASSERT_EQ(bounds.x(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".y", &v));
  ASSERT_EQ(bounds.y(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".w", &v));
  ASSERT_EQ(bounds.width(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".h", &v));
  ASSERT_EQ(bounds.height(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".screen_bounds_x", &v));
  ASSERT_EQ(screen_bounds.x(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".screen_bounds_y", &v));
  ASSERT_EQ(screen_bounds.y(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".screen_bounds_w", &v));
  ASSERT_EQ(screen_bounds.width(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".screen_bounds_h", &v));
  ASSERT_EQ(screen_bounds.height(), v);
  ASSERT_TRUE(dict->GetInteger(window_id + ".state", &v));
  ASSERT_EQ(state, v);

  // reload extension
  LoadExtension(extension_id);
  // and make sure the geometry got reloaded properly too
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, window_id, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);
}

// Tests that we won't do writes to the state store for SaveGeometry calls
// which don't change the state we already have.
TEST_F(AppWindowGeometryCacheTest, NoDuplicateWrites) {
  using testing::_;
  using testing::Mock;

  const std::string extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds1(100, 200, 300, 400);
  gfx::Rect bounds2(200, 400, 600, 800);
  gfx::Rect bounds2_duplicate(200, 400, 600, 800);

  gfx::Rect screen_bounds1(0, 0, 1600, 900);
  gfx::Rect screen_bounds2(0, 0, 1366, 768);
  gfx::Rect screen_bounds2_duplicate(0, 0, 1366, 768);

  MockPrefChangeCallback observer(pref_service_.get());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service_.get());
  registrar.Add("extensions.settings", observer.GetCallback());

  // Write the first bounds - it should do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(
      extension_id, kWindowId, bounds1, screen_bounds1, ui::SHOW_STATE_NORMAL);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different bounds - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(
      extension_id, kWindowId, bounds2, screen_bounds1, ui::SHOW_STATE_NORMAL);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different screen bounds - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(
      extension_id, kWindowId, bounds2, screen_bounds2, ui::SHOW_STATE_NORMAL);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different state - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id,
                       kWindowId,
                       bounds2,
                       screen_bounds2,
                       ui::SHOW_STATE_MAXIMIZED);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a bounds, screen bounds and state that's a duplicate of what we
  // already have. This should not do any writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
  cache_->SaveGeometry(extension_id,
                       kWindowId,
                       bounds2_duplicate,
                       screen_bounds2_duplicate,
                       ui::SHOW_STATE_MAXIMIZED);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);
}

// Tests that no more than kMaxCachedWindows windows will be cached.
TEST_F(AppWindowGeometryCacheTest, MaxWindows) {
  const std::string extension_id = AddExtensionWithPrefs("ext1");
  // inform cache of extension
  LoadExtension(extension_id);

  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  for (size_t i = 0; i < AppWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::SizeTToString(i);
    cache_->SaveGeometry(
        extension_id, window_id, bounds, screen_bounds, ui::SHOW_STATE_NORMAL);
  }

  // The first added window should no longer have cached geometry.
  EXPECT_FALSE(cache_->GetGeometry(extension_id, "window_0", NULL, NULL, NULL));
  // All other windows should still exist.
  for (size_t i = 1; i < AppWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::SizeTToString(i);
    EXPECT_TRUE(cache_->GetGeometry(extension_id, window_id, NULL, NULL, NULL));
  }
}

}  // namespace extensions
