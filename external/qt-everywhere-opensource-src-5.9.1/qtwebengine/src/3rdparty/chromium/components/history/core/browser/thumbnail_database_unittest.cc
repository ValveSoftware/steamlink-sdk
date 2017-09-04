// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "components/history/core/browser/thumbnail_database.h"
#include "components/history/core/test/database_test_utils.h"
#include "sql/connection.h"
#include "sql/recovery.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"

namespace history {

namespace {

// Blobs for the bitmap tests.  These aren't real bitmaps.  Golden
// database files store the same blobs (see VersionN tests).
const unsigned char kBlob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
const unsigned char kBlob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";

// Page and icon urls shared by tests.  Present in golden database
// files (see VersionN tests).
const GURL kPageUrl1 = GURL("http://google.com/");
const GURL kPageUrl2 = GURL("http://yahoo.com/");
const GURL kPageUrl3 = GURL("http://www.google.com/");
const GURL kPageUrl4 = GURL("http://www.google.com/blank.html");
const GURL kPageUrl5 = GURL("http://www.bing.com/");

const GURL kIconUrl1 = GURL("http://www.google.com/favicon.ico");
const GURL kIconUrl2 = GURL("http://www.yahoo.com/favicon.ico");
const GURL kIconUrl3 = GURL("http://www.google.com/touch.ico");
const GURL kIconUrl5 = GURL("http://www.bing.com/favicon.ico");

const gfx::Size kSmallSize = gfx::Size(16, 16);
const gfx::Size kLargeSize = gfx::Size(32, 32);

// Verify that the up-to-date database has the expected tables and
// columns.  Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present.  Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifyTablesAndColumns(sql::Connection* db) {
  // [meta], [favicons], [favicon_bitmaps], and [icon_mapping].
  EXPECT_EQ(4u, sql::test::CountSQLTables(db));

  // Implicit index on [meta], index on [favicons], index on
  // [favicon_bitmaps], two indices on [icon_mapping].
  EXPECT_EQ(5u, sql::test::CountSQLIndices(db));

  // [key] and [value].
  EXPECT_EQ(2u, sql::test::CountTableColumns(db, "meta"));

  // [id], [url], and [icon_type].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "favicons"));

  // [id], [icon_id], [last_updated], [image_data], [width], [height] and
  // [last_requested].
  EXPECT_EQ(7u, sql::test::CountTableColumns(db, "favicon_bitmaps"));

  // [id], [page_url], and [icon_id].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "icon_mapping"));
}

void VerifyDatabaseEmpty(sql::Connection* db) {
  size_t rows = 0;
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicons", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicon_bitmaps", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "icon_mapping", &rows));
  EXPECT_EQ(0u, rows);
}

// Helper to check that an expected mapping exists.
WARN_UNUSED_RESULT bool CheckPageHasIcon(
    ThumbnailDatabase* db,
    const GURL& page_url,
    favicon_base::IconType expected_icon_type,
    const GURL& expected_icon_url,
    const gfx::Size& expected_icon_size,
    size_t expected_icon_contents_size,
    const unsigned char* expected_icon_contents) {
  std::vector<IconMapping> icon_mappings;
  if (!db->GetIconMappingsForPageURL(page_url, &icon_mappings)) {
    ADD_FAILURE() << "failed GetIconMappingsForPageURL()";
    return false;
  }

  // Scan for the expected type.
  std::vector<IconMapping>::const_iterator iter = icon_mappings.begin();
  for (; iter != icon_mappings.end(); ++iter) {
    if (iter->icon_type == expected_icon_type)
      break;
  }
  if (iter == icon_mappings.end()) {
    ADD_FAILURE() << "failed to find |expected_icon_type|";
    return false;
  }

  if (expected_icon_url != iter->icon_url) {
    EXPECT_EQ(expected_icon_url, iter->icon_url);
    return false;
  }

  std::vector<FaviconBitmap> favicon_bitmaps;
  if (!db->GetFaviconBitmaps(iter->icon_id, &favicon_bitmaps)) {
    ADD_FAILURE() << "failed GetFaviconBitmaps()";
    return false;
  }

  if (1 != favicon_bitmaps.size()) {
    EXPECT_EQ(1u, favicon_bitmaps.size());
    return false;
  }

  if (expected_icon_size != favicon_bitmaps[0].pixel_size) {
    EXPECT_EQ(expected_icon_size, favicon_bitmaps[0].pixel_size);
    return false;
  }

  if (expected_icon_contents_size != favicon_bitmaps[0].bitmap_data->size()) {
    EXPECT_EQ(expected_icon_contents_size,
              favicon_bitmaps[0].bitmap_data->size());
    return false;
  }

  if (memcmp(favicon_bitmaps[0].bitmap_data->front(),
             expected_icon_contents, expected_icon_contents_size)) {
    ADD_FAILURE() << "failed to match |expected_icon_contents|";
    return false;
  }
  return true;
}

}  // namespace

class ThumbnailDatabaseTest : public testing::Test {
 public:
  ThumbnailDatabaseTest() {}
  ~ThumbnailDatabaseTest() override {}

  // Initialize a thumbnail database instance from the SQL file at
  // |golden_path| in the "History/" subdirectory of test data.
  std::unique_ptr<ThumbnailDatabase> LoadFromGolden(const char* golden_path) {
    if (!CreateDatabaseFromSQL(file_name_, golden_path)) {
      ADD_FAILURE() << "Failed loading " << golden_path;
      return std::unique_ptr<ThumbnailDatabase>();
    }

    std::unique_ptr<ThumbnailDatabase> db(new ThumbnailDatabase(NULL));
    EXPECT_EQ(sql::INIT_OK, db->Init(file_name_));
    db->BeginTransaction();

    return db;
  }

 protected:
  void SetUp() override {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestFavicons.db");
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

TEST_F(ThumbnailDatabaseTest, AddIconMapping) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  base::Time time = base::Time::Now();
  favicon_base::FaviconID id =
      db.AddFavicon(url, favicon_base::TOUCH_ICON, favicon, time, gfx::Size());
  EXPECT_NE(0, id);

  EXPECT_NE(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
}

TEST_F(ThumbnailDatabaseTest, LastRequestedTime) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  base::Time now = base::Time::Now();
  favicon_base::FaviconID id =
      db.AddFavicon(url, favicon_base::TOUCH_ICON, favicon, now, gfx::Size());
  ASSERT_NE(0, id);

  // Fetching the last requested time of a non-existent bitmap should fail.
  base::Time last_requested = base::Time::UnixEpoch();
  EXPECT_FALSE(db.GetFaviconBitmap(id + 1, NULL, &last_requested, NULL, NULL));
  EXPECT_EQ(last_requested, base::Time::UnixEpoch());  // Remains unchanged.

  // Fetching the last requested time of a bitmap that has no last request
  // should return a null timestamp.
  last_requested = base::Time::UnixEpoch();
  EXPECT_TRUE(db.GetFaviconBitmap(id, NULL, &last_requested, NULL, NULL));
  EXPECT_TRUE(last_requested.is_null());

  // Setting the last requested time of an existing bitmap should succeed, and
  // the set time should be returned by the corresponding "Get".
  last_requested = base::Time::UnixEpoch();
  EXPECT_TRUE(db.SetFaviconBitmapLastRequestedTime(id, now));
  EXPECT_TRUE(db.GetFaviconBitmap(id, NULL, &last_requested, NULL, NULL));
  EXPECT_EQ(last_requested, now);
}

TEST_F(ThumbnailDatabaseTest, DeleteIconMappings) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  favicon_base::FaviconID id = db.AddFavicon(url, favicon_base::TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id, favicon, time, gfx::Size());
  EXPECT_LT(0, db.AddIconMapping(url, id));

  favicon_base::FaviconID id2 = db.AddFavicon(url, favicon_base::FAVICON);
  EXPECT_LT(0, db.AddIconMapping(url, id2));
  ASSERT_NE(id, id2);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_mapping.front().icon_type, favicon_base::TOUCH_ICON);
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, favicon_base::FAVICON, NULL));

  db.DeleteIconMappings(url);

  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, NULL));
  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, favicon_base::FAVICON, NULL));
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURL) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");

  favicon_base::FaviconID id1 = db.AddFavicon(url, favicon_base::TOUCH_ICON);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id1, favicon, time, kSmallSize);
  db.AddFaviconBitmap(id1, favicon, time, kLargeSize);
  EXPECT_LT(0, db.AddIconMapping(url, id1));

  favicon_base::FaviconID id2 = db.AddFavicon(url, favicon_base::FAVICON);
  EXPECT_NE(id1, id2);
  db.AddFaviconBitmap(id2, favicon, time, kSmallSize);
  EXPECT_LT(0, db.AddIconMapping(url, id2));

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  ASSERT_EQ(2u, icon_mappings.size());
  EXPECT_EQ(id1, icon_mappings[0].icon_id);
  EXPECT_EQ(id2, icon_mappings[1].icon_id);
}

TEST_F(ThumbnailDatabaseTest, RetainDataForPageUrls) {
  ThumbnailDatabase db(NULL);

  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

  db.BeginTransaction();

  // Build a database mapping
  // kPageUrl1 -> kIconUrl1
  // kPageUrl2 -> kIconUrl2
  // kPageUrl3 -> kIconUrl1
  // kPageUrl4 -> kIconUrl1
  // kPageUrl5 -> kIconUrl5
  // Then retain kPageUrl1, kPageUrl3, and kPageUrl5. kPageUrl2
  // and kPageUrl4 should go away, but the others should be retained
  // correctly.

  // TODO(shess): This would probably make sense as a golden file.

  scoped_refptr<base::RefCountedStaticMemory> favicon1(
      new base::RefCountedStaticMemory(kBlob1, sizeof(kBlob1)));
  scoped_refptr<base::RefCountedStaticMemory> favicon2(
      new base::RefCountedStaticMemory(kBlob2, sizeof(kBlob2)));

  favicon_base::FaviconID kept_id1 =
      db.AddFavicon(kIconUrl1, favicon_base::FAVICON);
  db.AddFaviconBitmap(kept_id1, favicon1, base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl1, kept_id1);
  db.AddIconMapping(kPageUrl3, kept_id1);
  db.AddIconMapping(kPageUrl4, kept_id1);

  favicon_base::FaviconID unkept_id =
      db.AddFavicon(kIconUrl2, favicon_base::FAVICON);
  db.AddFaviconBitmap(unkept_id, favicon1, base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl2, unkept_id);

  favicon_base::FaviconID kept_id2 =
      db.AddFavicon(kIconUrl5, favicon_base::FAVICON);
  db.AddFaviconBitmap(kept_id2, favicon2, base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl5, kept_id2);

  // RetainDataForPageUrls() uses schema manipulations for efficiency.
  // Grab a copy of the schema to make sure the final schema matches.
  const std::string original_schema = db.db_.GetSchema();

  std::vector<GURL> pages_to_keep;
  pages_to_keep.push_back(kPageUrl1);
  pages_to_keep.push_back(kPageUrl3);
  pages_to_keep.push_back(kPageUrl5);
  EXPECT_TRUE(db.RetainDataForPageUrls(pages_to_keep));

  // Mappings from the retained urls should be left.
  EXPECT_TRUE(CheckPageHasIcon(&db,
                               kPageUrl1,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(&db,
                               kPageUrl3,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(&db,
                               kPageUrl5,
                               favicon_base::FAVICON,
                               kIconUrl5,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));

  // The ones not retained should be missing.
  EXPECT_FALSE(db.GetFaviconIDForFaviconURL(kPageUrl2, false, NULL));
  EXPECT_FALSE(db.GetFaviconIDForFaviconURL(kPageUrl4, false, NULL));

  // Schema should be the same.
  EXPECT_EQ(original_schema, db.db_.GetSchema());
}

// Test that RetainDataForPageUrls() expires retained favicons.
TEST_F(ThumbnailDatabaseTest, RetainDataForPageUrlsExpiresRetainedFavicons) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  scoped_refptr<base::RefCountedStaticMemory> favicon1(
      new base::RefCountedStaticMemory(kBlob1, sizeof(kBlob1)));
  favicon_base::FaviconID kept_id = db.AddFavicon(
      kIconUrl1, favicon_base::FAVICON, favicon1, base::Time::Now(),
      gfx::Size());
  db.AddIconMapping(kPageUrl1, kept_id);

  EXPECT_TRUE(db.RetainDataForPageUrls(std::vector<GURL>(1u, kPageUrl1)));

  favicon_base::FaviconID new_favicon_id = db.GetFaviconIDForFaviconURL(
      kIconUrl1, favicon_base::FAVICON, nullptr);
  ASSERT_NE(0, new_favicon_id);
  std::vector<FaviconBitmap> new_favicon_bitmaps;
  db.GetFaviconBitmaps(new_favicon_id, &new_favicon_bitmaps);

  ASSERT_EQ(1u, new_favicon_bitmaps.size());
  EXPECT_EQ(0, new_favicon_bitmaps[0].last_updated.ToInternalValue());
}

// Tests that deleting a favicon deletes the favicon row and favicon bitmap
// rows from the database.
TEST_F(ThumbnailDatabaseTest, DeleteFavicon) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data1(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon1(
      new base::RefCountedBytes(data1));
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2(
      new base::RefCountedBytes(data2));

  GURL url("http://google.com");
  favicon_base::FaviconID id = db.AddFavicon(url, favicon_base::FAVICON);
  base::Time last_updated = base::Time::Now();
  db.AddFaviconBitmap(id, favicon1, last_updated, kSmallSize);
  db.AddFaviconBitmap(id, favicon2, last_updated, kLargeSize);

  EXPECT_TRUE(db.GetFaviconBitmaps(id, NULL));

  EXPECT_TRUE(db.DeleteFavicon(id));
  EXPECT_FALSE(db.GetFaviconBitmaps(id, NULL));
}

TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURLForReturnOrder) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  // Add a favicon
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL page_url("http://google.com");
  GURL icon_url("http://google.com/favicon.ico");
  base::Time time = base::Time::Now();

  favicon_base::FaviconID id = db.AddFavicon(
      icon_url, favicon_base::FAVICON, favicon, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::FAVICON, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch icon
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2 =
      new base::RefCountedBytes(data);

  favicon_base::FaviconID id2 = db.AddFavicon(
      icon_url, favicon_base::TOUCH_ICON, favicon2, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id2));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id2, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::TOUCH_ICON, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch precomposed icon
  scoped_refptr<base::RefCountedBytes> favicon3 =
      new base::RefCountedBytes(data2);

  favicon_base::FaviconID id3 =
      db.AddFavicon(icon_url,
                    favicon_base::TOUCH_PRECOMPOSED_ICON,
                    favicon3,
                    time,
                    gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id3));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id3, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON,
            icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);
}

// Test result of GetIconMappingsForPageURL when an icon type is passed in.
TEST_F(ThumbnailDatabaseTest, GetIconMappingsForPageURLWithIconType) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  GURL url("http://google.com");
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));
  base::Time time = base::Time::Now();

  favicon_base::FaviconID id1 =
      db.AddFavicon(url, favicon_base::FAVICON, favicon, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id1));

  favicon_base::FaviconID id2 =
      db.AddFavicon(url, favicon_base::TOUCH_ICON, favicon, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id2));

  favicon_base::FaviconID id3 =
      db.AddFavicon(url, favicon_base::TOUCH_ICON, favicon, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(url, id3));

  // Only the mappings for favicons of type TOUCH_ICON should be returned as
  // TOUCH_ICON is a larger icon type than FAVICON.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      url,
      favicon_base::FAVICON | favicon_base::TOUCH_ICON |
          favicon_base::TOUCH_PRECOMPOSED_ICON,
      &icon_mappings));

  EXPECT_EQ(2u, icon_mappings.size());
  if (id2 == icon_mappings[0].icon_id) {
    EXPECT_EQ(id3, icon_mappings[1].icon_id);
  } else {
    EXPECT_EQ(id3, icon_mappings[0].icon_id);
    EXPECT_EQ(id2, icon_mappings[1].icon_id);
  }

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      url, favicon_base::TOUCH_ICON, &icon_mappings));
  if (id2 == icon_mappings[0].icon_id) {
    EXPECT_EQ(id3, icon_mappings[1].icon_id);
  } else {
    EXPECT_EQ(id3, icon_mappings[0].icon_id);
    EXPECT_EQ(id2, icon_mappings[1].icon_id);
  }

  icon_mappings.clear();
  EXPECT_TRUE(
      db.GetIconMappingsForPageURL(url, favicon_base::FAVICON, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(id1, icon_mappings[0].icon_id);
}

TEST_F(ThumbnailDatabaseTest, HasMappingFor) {
  ThumbnailDatabase db(NULL);
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  base::Time time = base::Time::Now();
  favicon_base::FaviconID id1 = db.AddFavicon(GURL("http://google.com"),
                                              favicon_base::FAVICON,
                                              favicon,
                                              time,
                                              gfx::Size());
  EXPECT_NE(id1, 0);

  // Add another type of favicon
  time = base::Time::Now();
  favicon_base::FaviconID id2 =
      db.AddFavicon(GURL("http://www.google.com/icon"),
                    favicon_base::TOUCH_ICON,
                    favicon,
                    time,
                    gfx::Size());
  EXPECT_NE(id2, 0);

  // Add 3rd favicon
  time = base::Time::Now();
  favicon_base::FaviconID id3 =
      db.AddFavicon(GURL("http://www.google.com/icon"),
                    favicon_base::TOUCH_ICON,
                    favicon,
                    time,
                    gfx::Size());
  EXPECT_NE(id3, 0);

  // Add 2 icon mapping
  GURL page_url("http://www.google.com");
  EXPECT_TRUE(db.AddIconMapping(page_url, id1));
  EXPECT_TRUE(db.AddIconMapping(page_url, id2));

  EXPECT_TRUE(db.HasMappingFor(id1));
  EXPECT_TRUE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));

  // Remove all mappings
  db.DeleteIconMappings(page_url);
  EXPECT_FALSE(db.HasMappingFor(id1));
  EXPECT_FALSE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));
}

// Test loading version 3 database.
TEST_F(ThumbnailDatabaseTest, Version3) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v3.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  // Version 3 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 4 database.
TEST_F(ThumbnailDatabaseTest, Version4) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v4.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  // Version 4 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 5 database.
TEST_F(ThumbnailDatabaseTest, Version5) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v5.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  // Version 5 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 6 database.
TEST_F(ThumbnailDatabaseTest, Version6) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v6.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl1,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl2,
                               favicon_base::FAVICON,
                               kIconUrl2,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::TOUCH_ICON,
                               kIconUrl3,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
}

// Test loading version 7 database.
TEST_F(ThumbnailDatabaseTest, Version7) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v7.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl1,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl2,
                               favicon_base::FAVICON,
                               kIconUrl2,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::TOUCH_ICON,
                               kIconUrl3,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
}

// Test loading version 8 database.
TEST_F(ThumbnailDatabaseTest, Version8) {
  std::unique_ptr<ThumbnailDatabase> db = LoadFromGolden("Favicons.v8.sql");
  ASSERT_TRUE(db.get() != NULL);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl1,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl2,
                               favicon_base::FAVICON,
                               kIconUrl2,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::FAVICON,
                               kIconUrl1,
                               kLargeSize,
                               sizeof(kBlob1),
                               kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(),
                               kPageUrl3,
                               favicon_base::TOUCH_ICON,
                               kIconUrl3,
                               kLargeSize,
                               sizeof(kBlob2),
                               kBlob2));
}

TEST_F(ThumbnailDatabaseTest, Recovery) {
  // This code tests the recovery module in concert with Chromium's
  // custom recover virtual table.  Under USE_SYSTEM_SQLITE, this is
  // not available.  This is detected dynamically because corrupt
  // databases still need to be handled, perhaps by Raze(), and the
  // recovery module is an obvious layer to abstract that to.
  // TODO(shess): Handle that case for real!
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database.
  {
    EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v8.sql"));

    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    VerifyTablesAndColumns(&raw_db);
  }

  // Test that the contents make sense after clean open.
  {
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl1,
                                 favicon_base::FAVICON,
                                 kIconUrl1,
                                 kLargeSize,
                                 sizeof(kBlob1),
                                 kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl2,
                                 favicon_base::FAVICON,
                                 kIconUrl2,
                                 kLargeSize,
                                 sizeof(kBlob2),
                                 kBlob2));
  }

  // Corrupt the |icon_mapping.page_url| index by deleting an element
  // from the backing table but not the index.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));
  }
  const char kIndexName[] = "icon_mapping_page_url_idx";
  const char kDeleteSql[] =
      "DELETE FROM icon_mapping WHERE page_url = 'http://yahoo.com/'";
  EXPECT_TRUE(
      sql::test::CorruptTableOrIndex(file_name_, kIndexName, kDeleteSql));

  // Database should be corrupt at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_NE("ok", sql::test::IntegrityCheck(&raw_db));
  }

  // Open the database and access the corrupt index.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Data for kPageUrl2 was deleted, but the index entry remains,
    // this will throw SQLITE_CORRUPT.  The corruption handler will
    // recover the database and poison the handle, so the outer call
    // fails.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }

  // Database should also be recovered at higher levels.
  {
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Now this fails because there is no mapping.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    // Other data was retained by recovery.
    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl1,
                                 favicon_base::FAVICON,
                                 kIconUrl1,
                                 kLargeSize,
                                 sizeof(kBlob1),
                                 kBlob1));
  }

  // Corrupt the database again by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Database should be recovered during open.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));
    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl1,
                                 favicon_base::FAVICON,
                                 kIconUrl1,
                                 kLargeSize,
                                 sizeof(kBlob1),
                                 kBlob1));

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(ThumbnailDatabaseTest, Recovery7) {
  // This code tests the recovery module in concert with Chromium's
  // custom recover virtual table.  Under USE_SYSTEM_SQLITE, this is
  // not available.  This is detected dynamically because corrupt
  // databases still need to be handled, perhaps by Raze(), and the
  // recovery module is an obvious layer to abstract that to.
  // TODO(shess): Handle that case for real!
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database without loading into ThumbnailDatabase
  // (which would upgrade it).
  EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v7.sql"));

  // Corrupt the |icon_mapping.page_url| index by deleting an element
  // from the backing table but not the index.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));
  }
  const char kIndexName[] = "icon_mapping_page_url_idx";
  const char kDeleteSql[] =
      "DELETE FROM icon_mapping WHERE page_url = 'http://yahoo.com/'";
  EXPECT_TRUE(
      sql::test::CorruptTableOrIndex(file_name_, kIndexName, kDeleteSql));

  // Database should be corrupt at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_NE("ok", sql::test::IntegrityCheck(&raw_db));
  }

  // Open the database and access the corrupt index. Note that this upgrades
  // the database.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Data for kPageUrl2 was deleted, but the index entry remains,
    // this will throw SQLITE_CORRUPT.  The corruption handler will
    // recover the database and poison the handle, so the outer call
    // fails.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }

  // Database should also be recovered at higher levels.
  {
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    // Now this fails because there is no mapping.
    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));

    // Other data was retained by recovery.
    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl1,
                                 favicon_base::FAVICON,
                                 kIconUrl1,
                                 kLargeSize,
                                 sizeof(kBlob1),
                                 kBlob1));
  }

  // Corrupt the database again by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Database should be recovered during open.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, NULL));
    EXPECT_TRUE(CheckPageHasIcon(&db,
                                 kPageUrl1,
                                 favicon_base::FAVICON,
                                 kIconUrl1,
                                 kLargeSize,
                                 sizeof(kBlob1),
                                 kBlob1));

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(ThumbnailDatabaseTest, Recovery6) {
  // TODO(shess): See comment at top of Recovery test.
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database without loading into ThumbnailDatabase
  // (which would upgrade it).
  EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v6.sql"));

  // Corrupt the database by adjusting the header.  This form of corruption will
  // cause immediate failures during Open(), before the migration code runs, so
  // the recovery code will run.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Database open should succeed.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // The database should be usable at the SQLite level, with a current schema
  // and no data.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);

    // Version 6 recovery is deprecated, the data should all be gone.
    VerifyDatabaseEmpty(&raw_db);
  }
}

TEST_F(ThumbnailDatabaseTest, Recovery5) {
  // TODO(shess): See comment at top of Recovery test.
  if (!sql::Recovery::FullRecoverySupported())
    return;

  // Create an example database without loading into ThumbnailDatabase
  // (which would upgrade it).
  EXPECT_TRUE(CreateDatabaseFromSQL(file_name_, "Favicons.v5.sql"));

  // Corrupt the database by adjusting the header.  This form of corruption will
  // cause immediate failures during Open(), before the migration code runs, so
  // the recovery code will run.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_FALSE(raw_db.IsSQLValid("PRAGMA integrity_check"));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // Database open should succeed.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // The database should be usable at the SQLite level, with a current schema
  // and no data.
  {
    sql::Connection raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(&raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);

    // Version 5 recovery is deprecated, the data should all be gone.
    VerifyDatabaseEmpty(&raw_db);
  }
}

// Test that various broken schema found in the wild can be opened
// successfully, and result in the correct schema.
TEST_F(ThumbnailDatabaseTest, WildSchema) {
  base::FilePath sql_path;
  EXPECT_TRUE(GetTestDataHistoryDir(&sql_path));
  sql_path = sql_path.AppendASCII("thumbnail_wild");

  base::FileEnumerator fe(
      sql_path, false, base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.sql"));
  for (base::FilePath name = fe.Next(); !name.empty(); name = fe.Next()) {
    SCOPED_TRACE(name.BaseName().AsUTF8Unsafe());
    // Generate a database path based on the golden's basename.
    base::FilePath db_base_name =
        name.BaseName().ReplaceExtension(FILE_PATH_LITERAL("db"));
    base::FilePath db_path = file_name_.DirName().Append(db_base_name);
    ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, name));

    // All schema flaws should be cleaned up by Init().
    // TODO(shess): Differentiate between databases which need Raze()
    // and those which can be salvaged.
    ThumbnailDatabase db(NULL);
    ASSERT_EQ(sql::INIT_OK, db.Init(db_path));

    // Verify that the resulting schema is correct, whether it
    // involved razing the file or fixing things in place.
    VerifyTablesAndColumns(&db.db_);
  }
}

}  // namespace history
