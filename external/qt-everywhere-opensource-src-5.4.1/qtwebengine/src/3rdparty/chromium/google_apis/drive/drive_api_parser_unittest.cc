// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_parser.h"

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "google_apis/drive/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

// Test about resource parsing.
TEST(DriveAPIParserTest, AboutResourceParser) {
  std::string error;
  scoped_ptr<base::Value> document = test_util::LoadJSONFile(
      "drive/about.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AboutResource> resource(new AboutResource());
  EXPECT_TRUE(resource->Parse(*document));

  EXPECT_EQ("0AIv7G8yEYAWHUk9123", resource->root_folder_id());
  EXPECT_EQ(5368709120LL, resource->quota_bytes_total());
  EXPECT_EQ(1073741824LL, resource->quota_bytes_used());
  EXPECT_EQ(8177LL, resource->largest_change_id());
}

// Test app list parsing.
TEST(DriveAPIParserTest, AppListParser) {
  std::string error;
  scoped_ptr<base::Value> document = test_util::LoadJSONFile(
      "drive/applist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AppList> applist(new AppList);
  EXPECT_TRUE(applist->Parse(*document));

  EXPECT_EQ("\"Jm4BaSnCWNND-noZsHINRqj4ABC/tuqRBw0lvjUdPtc_2msA1tN4XYZ\"",
            applist->etag());
  ASSERT_EQ(2U, applist->items().size());
  // Check Drive app 1
  const AppResource& app1 = *applist->items()[0];
  EXPECT_EQ("123456788192", app1.application_id());
  EXPECT_EQ("Drive app 1", app1.name());
  EXPECT_EQ("", app1.object_type());
  EXPECT_TRUE(app1.supports_create());
  EXPECT_TRUE(app1.is_removable());
  EXPECT_EQ("abcdefghabcdefghabcdefghabcdefgh", app1.product_id());

  ASSERT_EQ(1U, app1.primary_mimetypes().size());
  EXPECT_EQ("application/vnd.google-apps.drive-sdk.123456788192",
            *app1.primary_mimetypes()[0]);

  ASSERT_EQ(2U, app1.secondary_mimetypes().size());
  EXPECT_EQ("text/html", *app1.secondary_mimetypes()[0]);
  EXPECT_EQ("text/plain", *app1.secondary_mimetypes()[1]);

  ASSERT_EQ(2U, app1.primary_file_extensions().size());
  EXPECT_EQ("exe", *app1.primary_file_extensions()[0]);
  EXPECT_EQ("com", *app1.primary_file_extensions()[1]);

  EXPECT_EQ(0U, app1.secondary_file_extensions().size());

  ASSERT_EQ(6U, app1.icons().size());
  const DriveAppIcon& icon1 = *app1.icons()[0];
  EXPECT_EQ(DriveAppIcon::APPLICATION, icon1.category());
  EXPECT_EQ(10, icon1.icon_side_length());
  EXPECT_EQ("http://www.example.com/10.png", icon1.icon_url().spec());

  const DriveAppIcon& icon6 = *app1.icons()[5];
  EXPECT_EQ(DriveAppIcon::SHARED_DOCUMENT, icon6.category());
  EXPECT_EQ(16, icon6.icon_side_length());
  EXPECT_EQ("http://www.example.com/ds16.png", icon6.icon_url().spec());

  EXPECT_EQ("https://www.example.com/createForApp1", app1.create_url().spec());

  // Check Drive app 2
  const AppResource& app2 = *applist->items()[1];
  EXPECT_EQ("876543210000", app2.application_id());
  EXPECT_EQ("Drive app 2", app2.name());
  EXPECT_EQ("", app2.object_type());
  EXPECT_FALSE(app2.supports_create());
  EXPECT_FALSE(app2.is_removable());
  EXPECT_EQ("hgfedcbahgfedcbahgfedcbahgfedcba", app2.product_id());

  ASSERT_EQ(3U, app2.primary_mimetypes().size());
  EXPECT_EQ("image/jpeg", *app2.primary_mimetypes()[0]);
  EXPECT_EQ("image/png", *app2.primary_mimetypes()[1]);
  EXPECT_EQ("application/vnd.google-apps.drive-sdk.876543210000",
            *app2.primary_mimetypes()[2]);

  EXPECT_EQ(0U, app2.secondary_mimetypes().size());
  EXPECT_EQ(0U, app2.primary_file_extensions().size());
  EXPECT_EQ(0U, app2.secondary_file_extensions().size());

  ASSERT_EQ(3U, app2.icons().size());
  const DriveAppIcon& icon2 = *app2.icons()[1];
  EXPECT_EQ(DriveAppIcon::DOCUMENT, icon2.category());
  EXPECT_EQ(10, icon2.icon_side_length());
  EXPECT_EQ("http://www.example.com/d10.png", icon2.icon_url().spec());

  EXPECT_EQ("https://www.example.com/createForApp2", app2.create_url().spec());
}

// Test file list parsing.
TEST(DriveAPIParserTest, FileListParser) {
  std::string error;
  scoped_ptr<base::Value> document = test_util::LoadJSONFile(
      "drive/filelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<FileList> filelist(new FileList);
  EXPECT_TRUE(filelist->Parse(*document));

  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files?pageToken=EAIaggEL"
                 "EgA6egpi96It9mH_____f_8AAP__AAD_okhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8AAP__AP7__n__AP8AokhU-cHLz83KzszMxsjMzs_RyNGJ"
                 "nridyrbHs7u9tv8A__4QZCEiXPTi_wtIgTkAAAAAngnSXUgCDEAAIgsJPgar"
                 "t10AAAAABC"), filelist->next_link());

  ASSERT_EQ(3U, filelist->items().size());
  // Check file 1 (a regular file)
  const FileResource& file1 = *filelist->items()[0];
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", file1.file_id());
  EXPECT_EQ("\"WtRjAPZWbDA7_fkFjc5ojsEvDEF/MTM0MzM2NzgwMDIXYZ\"",
            file1.etag());
  EXPECT_EQ("My first file data", file1.title());
  EXPECT_EQ("application/octet-stream", file1.mime_type());

  EXPECT_FALSE(file1.labels().is_trashed());
  EXPECT_FALSE(file1.shared());

  EXPECT_EQ(640, file1.image_media_metadata().width());
  EXPECT_EQ(480, file1.image_media_metadata().height());
  EXPECT_EQ(90, file1.image_media_metadata().rotation());

  base::Time created_time;
  ASSERT_TRUE(
      util::GetTimeFromString("2012-07-24T08:51:16.570Z", &created_time));
  EXPECT_EQ(created_time, file1.created_date());

  base::Time modified_time;
  ASSERT_TRUE(
      util::GetTimeFromString("2012-07-27T05:43:20.269Z", &modified_time));
  EXPECT_EQ(modified_time, file1.modified_date());

  ASSERT_EQ(1U, file1.parents().size());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", file1.parents()[0].file_id());
  EXPECT_EQ(GURL("https://www.googleapis.com/drive/v2/files/"
                 "0B4v7G8yEYAWHYW1OcExsUVZLABC"),
            file1.parents()[0].parent_link());

  EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", file1.md5_checksum());
  EXPECT_EQ(1000U, file1.file_size());

  EXPECT_EQ(GURL("https://docs.google.com/file/d/"
                 "0B4v7G8yEYAWHUmRrU2lMS2hLABC/edit"),
            file1.alternate_link());
  ASSERT_EQ(1U, file1.open_with_links().size());
  EXPECT_EQ("1234567890", file1.open_with_links()[0].app_id);
  EXPECT_EQ(GURL("http://open_with_link/url"),
            file1.open_with_links()[0].open_url);

  // Check file 2 (a Google Document)
  const FileResource& file2 = *filelist->items()[1];
  EXPECT_EQ("Test Google Document", file2.title());
  EXPECT_EQ("application/vnd.google-apps.document", file2.mime_type());

  EXPECT_TRUE(file2.labels().is_trashed());
  EXPECT_TRUE(file2.shared());

  EXPECT_EQ(-1, file2.image_media_metadata().width());
  EXPECT_EQ(-1, file2.image_media_metadata().height());
  EXPECT_EQ(-1, file2.image_media_metadata().rotation());

  base::Time shared_with_me_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-27T04:54:11.030Z",
                                      &shared_with_me_time));
  EXPECT_EQ(shared_with_me_time, file2.shared_with_me_date());

  EXPECT_EQ(0U, file2.file_size());

  ASSERT_EQ(0U, file2.parents().size());

  EXPECT_EQ(0U, file2.open_with_links().size());

  // Check file 3 (a folder)
  const FileResource& file3 = *filelist->items()[2];
  EXPECT_EQ(0U, file3.file_size());
  EXPECT_EQ("TestFolder", file3.title());
  EXPECT_EQ("application/vnd.google-apps.folder", file3.mime_type());
  ASSERT_TRUE(file3.IsDirectory());
  EXPECT_FALSE(file3.shared());

  ASSERT_EQ(1U, file3.parents().size());
  EXPECT_EQ("0AIv7G8yEYAWHUk9ABC", file3.parents()[0].file_id());
  EXPECT_EQ(0U, file3.open_with_links().size());
}

// Test change list parsing.
TEST(DriveAPIParserTest, ChangeListParser) {
  std::string error;
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("drive/changelist.json");
  ASSERT_TRUE(document.get());

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<ChangeList> changelist(new ChangeList);
  EXPECT_TRUE(changelist->Parse(*document));

  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes?pageToken=8929",
            changelist->next_link().spec());
  EXPECT_EQ(13664, changelist->largest_change_id());

  ASSERT_EQ(4U, changelist->items().size());

  const ChangeResource& change1 = *changelist->items()[0];
  EXPECT_EQ(8421, change1.change_id());
  EXPECT_FALSE(change1.is_deleted());
  EXPECT_EQ("1Pc8jzfU1ErbN_eucMMqdqzY3eBm0v8sxXm_1CtLxABC", change1.file_id());
  EXPECT_EQ(change1.file_id(), change1.file()->file_id());
  EXPECT_FALSE(change1.file()->shared());
  EXPECT_EQ(change1.file()->modified_date(), change1.modification_date());

  const ChangeResource& change2 = *changelist->items()[1];
  EXPECT_EQ(8424, change2.change_id());
  EXPECT_FALSE(change2.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHUmRrU2lMS2hLABC", change2.file_id());
  EXPECT_EQ(change2.file_id(), change2.file()->file_id());
  EXPECT_TRUE(change2.file()->shared());
  EXPECT_EQ(change2.file()->modified_date(), change2.modification_date());

  const ChangeResource& change3 = *changelist->items()[2];
  EXPECT_EQ(8429, change3.change_id());
  EXPECT_FALSE(change3.is_deleted());
  EXPECT_EQ("0B4v7G8yEYAWHYW1OcExsUVZLABC", change3.file_id());
  EXPECT_EQ(change3.file_id(), change3.file()->file_id());
  EXPECT_FALSE(change3.file()->shared());
  EXPECT_EQ(change3.file()->modified_date(), change3.modification_date());

  // Deleted entry.
  const ChangeResource& change4 = *changelist->items()[3];
  EXPECT_EQ(8430, change4.change_id());
  EXPECT_EQ("ABCv7G8yEYAWHc3Y5X0hMSkJYXYZ", change4.file_id());
  EXPECT_TRUE(change4.is_deleted());
  base::Time modification_time;
  ASSERT_TRUE(util::GetTimeFromString("2012-07-27T12:34:56.789Z",
                                      &modification_time));
  EXPECT_EQ(modification_time, change4.modification_date());
}

TEST(DriveAPIParserTest, HasKind) {
  scoped_ptr<base::Value> change_list_json(
      test_util::LoadJSONFile("drive/changelist.json"));
  scoped_ptr<base::Value> file_list_json(
      test_util::LoadJSONFile("drive/filelist.json"));

  EXPECT_TRUE(ChangeList::HasChangeListKind(*change_list_json));
  EXPECT_FALSE(ChangeList::HasChangeListKind(*file_list_json));

  EXPECT_FALSE(FileList::HasFileListKind(*change_list_json));
  EXPECT_TRUE(FileList::HasFileListKind(*file_list_json));
}

}  // namespace google_apis
