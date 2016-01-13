// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_parser.h"

#include <string>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/drive/test_util.h"
#include "google_apis/drive/time_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

// Test document feed parsing.
TEST(GDataWAPIParserTest, ResourceListJsonParser) {
  std::string error;
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/basic_feed.json");
  ASSERT_TRUE(document.get());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<ResourceList> feed(ResourceList::ExtractAndParse(*document));
  ASSERT_TRUE(feed.get());

  base::Time update_time;
  ASSERT_TRUE(util::GetTimeFromString("2011-12-14T01:03:21.151Z",
                                                   &update_time));

  EXPECT_EQ(1, feed->start_index());
  EXPECT_EQ(1000, feed->items_per_page());
  EXPECT_EQ(update_time, feed->updated_time());

  // Check authors.
  ASSERT_EQ(1U, feed->authors().size());
  EXPECT_EQ("tester", feed->authors()[0]->name());
  EXPECT_EQ("tester@testing.com", feed->authors()[0]->email());

  // Check links.
  ASSERT_EQ(6U, feed->links().size());
  const Link* self_link = feed->GetLinkByType(Link::LINK_SELF);
  ASSERT_TRUE(self_link);
  EXPECT_EQ("https://self_link/", self_link->href().spec());
  EXPECT_EQ("application/atom+xml", self_link->mime_type());

  const Link* resumable_link =
      feed->GetLinkByType(Link::LINK_RESUMABLE_CREATE_MEDIA);
  ASSERT_TRUE(resumable_link);
  EXPECT_EQ("https://resumable_create_media_link/",
            resumable_link->href().spec());
  EXPECT_EQ("application/atom+xml", resumable_link->mime_type());

  // Check entries.
  ASSERT_EQ(4U, feed->entries().size());

  // Check a folder entry.
  const ResourceEntry* folder_entry = feed->entries()[0];
  ASSERT_TRUE(folder_entry);
  EXPECT_EQ(ENTRY_KIND_FOLDER, folder_entry->kind());
  EXPECT_EQ("\"HhMOFgcNHSt7ImBr\"", folder_entry->etag());
  EXPECT_EQ("folder:sub_sub_directory_folder_id", folder_entry->resource_id());
  EXPECT_EQ("https://1_folder_id", folder_entry->id());
  EXPECT_EQ("Entry 1 Title", folder_entry->title());
  base::Time entry1_update_time;
  base::Time entry1_publish_time;
  ASSERT_TRUE(util::GetTimeFromString("2011-04-01T18:34:08.234Z",
                                                   &entry1_update_time));
  ASSERT_TRUE(util::GetTimeFromString("2010-11-07T05:03:54.719Z",
                                                   &entry1_publish_time));
  EXPECT_EQ(entry1_update_time, folder_entry->updated_time());
  EXPECT_EQ(entry1_publish_time, folder_entry->published_time());

  ASSERT_EQ(1U, folder_entry->authors().size());
  EXPECT_EQ("entry_tester", folder_entry->authors()[0]->name());
  EXPECT_EQ("entry_tester@testing.com", folder_entry->authors()[0]->email());
  EXPECT_EQ("https://1_folder_content_url/",
            folder_entry->download_url().spec());
  EXPECT_EQ("application/atom+xml;type=feed",
            folder_entry->content_mime_type());

  ASSERT_EQ(1U, folder_entry->resource_links().size());
  const ResourceLink* feed_link = folder_entry->resource_links()[0];
  ASSERT_TRUE(feed_link);
  ASSERT_EQ(ResourceLink::FEED_LINK_ACL, feed_link->type());

  const Link* entry1_alternate_link =
      folder_entry->GetLinkByType(Link::LINK_ALTERNATE);
  ASSERT_TRUE(entry1_alternate_link);
  EXPECT_EQ("https://1_folder_alternate_link/",
            entry1_alternate_link->href().spec());
  EXPECT_EQ("text/html", entry1_alternate_link->mime_type());

  const Link* entry1_edit_link = folder_entry->GetLinkByType(Link::LINK_EDIT);
  ASSERT_TRUE(entry1_edit_link);
  EXPECT_EQ("https://1_edit_link/", entry1_edit_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_edit_link->mime_type());

  // Check a file entry.
  const ResourceEntry* file_entry = feed->entries()[1];
  ASSERT_TRUE(file_entry);
  EXPECT_EQ(ENTRY_KIND_FILE, file_entry->kind());
  EXPECT_EQ("filename.m4a", file_entry->filename());
  EXPECT_EQ("sugg_file_name.m4a", file_entry->suggested_filename());
  EXPECT_EQ("3b4382ebefec6e743578c76bbd0575ce", file_entry->file_md5());
  EXPECT_EQ(892721, file_entry->file_size());
  const Link* file_parent_link = file_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(file_parent_link);
  EXPECT_EQ("https://file_link_parent/", file_parent_link->href().spec());
  EXPECT_EQ("application/atom+xml", file_parent_link->mime_type());
  EXPECT_EQ("Medical", file_parent_link->title());
  const Link* file_open_with_link =
    file_entry->GetLinkByType(Link::LINK_OPEN_WITH);
  ASSERT_TRUE(file_open_with_link);
  EXPECT_EQ("https://xml_file_entry_open_with_link/",
            file_open_with_link->href().spec());
  EXPECT_EQ("application/atom+xml", file_open_with_link->mime_type());
  EXPECT_EQ("the_app_id", file_open_with_link->app_id());
  EXPECT_EQ(654321, file_entry->changestamp());

  const Link* file_unknown_link = file_entry->GetLinkByType(Link::LINK_UNKNOWN);
  ASSERT_TRUE(file_unknown_link);
  EXPECT_EQ("https://xml_file_fake_entry_open_with_link/",
            file_unknown_link->href().spec());
  EXPECT_EQ("application/atom+xml", file_unknown_link->mime_type());
  EXPECT_EQ("", file_unknown_link->app_id());

  // Check a file entry.
  const ResourceEntry* resource_entry = feed->entries()[2];
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(ENTRY_KIND_DOCUMENT, resource_entry->kind());
  EXPECT_TRUE(resource_entry->is_hosted_document());
  EXPECT_TRUE(resource_entry->is_google_document());
  EXPECT_FALSE(resource_entry->is_external_document());

  // Check an external document entry.
  const ResourceEntry* app_entry = feed->entries()[3];
  ASSERT_TRUE(app_entry);
  EXPECT_EQ(ENTRY_KIND_EXTERNAL_APP, app_entry->kind());
  EXPECT_TRUE(app_entry->is_hosted_document());
  EXPECT_TRUE(app_entry->is_external_document());
  EXPECT_FALSE(app_entry->is_google_document());
}


// Test document feed parsing.
TEST(GDataWAPIParserTest, ResourceEntryJsonParser) {
  std::string error;
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/file_entry.json");
  ASSERT_TRUE(document.get());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<ResourceEntry> entry(ResourceEntry::ExtractAndParse(*document));
  ASSERT_TRUE(entry.get());

  EXPECT_EQ(ENTRY_KIND_FILE, entry->kind());
  EXPECT_EQ("\"HhMOFgxXHit7ImBr\"", entry->etag());
  EXPECT_EQ("file:2_file_resource_id", entry->resource_id());
  EXPECT_EQ("2_file_id", entry->id());
  EXPECT_EQ("File 1.mp3", entry->title());
  base::Time entry1_update_time;
  base::Time entry1_publish_time;
  ASSERT_TRUE(util::GetTimeFromString("2011-12-14T00:40:47.330Z",
                                      &entry1_update_time));
  ASSERT_TRUE(util::GetTimeFromString("2011-12-13T00:40:47.330Z",
                                      &entry1_publish_time));
  EXPECT_EQ(entry1_update_time, entry->updated_time());
  EXPECT_EQ(entry1_publish_time, entry->published_time());

  EXPECT_EQ(1U, entry->authors().size());
  EXPECT_EQ("tester", entry->authors()[0]->name());
  EXPECT_EQ("tester@testing.com", entry->authors()[0]->email());
  EXPECT_EQ("https://file_content_url/",
            entry->download_url().spec());
  EXPECT_EQ("audio/mpeg",
            entry->content_mime_type());

  // Check feed links.
  ASSERT_EQ(1U, entry->resource_links().size());
  const ResourceLink* feed_link_1 = entry->resource_links()[0];
  ASSERT_TRUE(feed_link_1);
  EXPECT_EQ(ResourceLink::FEED_LINK_REVISIONS, feed_link_1->type());

  // Check links.
  ASSERT_EQ(8U, entry->links().size());
  const Link* entry1_alternate_link =
      entry->GetLinkByType(Link::LINK_ALTERNATE);
  ASSERT_TRUE(entry1_alternate_link);
  EXPECT_EQ("https://file_link_alternate/",
            entry1_alternate_link->href().spec());
  EXPECT_EQ("text/html", entry1_alternate_link->mime_type());

  const Link* entry1_edit_link = entry->GetLinkByType(Link::LINK_EDIT_MEDIA);
  ASSERT_TRUE(entry1_edit_link);
  EXPECT_EQ("https://file_edit_media/",
            entry1_edit_link->href().spec());
  EXPECT_EQ("audio/mpeg", entry1_edit_link->mime_type());

  const Link* entry1_self_link = entry->GetLinkByType(Link::LINK_SELF);
  ASSERT_TRUE(entry1_self_link);
  EXPECT_EQ("https://file1_link_self/file%3A2_file_resource_id",
            entry1_self_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_self_link->mime_type());
  EXPECT_EQ("", entry1_self_link->app_id());

  const Link* entry1_open_with_link =
      entry->GetLinkByType(Link::LINK_OPEN_WITH);
  ASSERT_TRUE(entry1_open_with_link);
  EXPECT_EQ("https://entry1_open_with_link/",
            entry1_open_with_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_open_with_link->mime_type());
  EXPECT_EQ("the_app_id", entry1_open_with_link->app_id());

  const Link* entry1_unknown_link = entry->GetLinkByType(Link::LINK_UNKNOWN);
  ASSERT_TRUE(entry1_unknown_link);
  EXPECT_EQ("https://entry1_fake_entry_open_with_link/",
            entry1_unknown_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_unknown_link->mime_type());
  EXPECT_EQ("", entry1_unknown_link->app_id());

  // Check a file properties.
  EXPECT_EQ(ENTRY_KIND_FILE, entry->kind());
  EXPECT_EQ("File 1.mp3", entry->filename());
  EXPECT_EQ("File 1.mp3", entry->suggested_filename());
  EXPECT_EQ("3b4382ebefec6e743578c76bbd0575ce", entry->file_md5());
  EXPECT_EQ(892721, entry->file_size());

  // WAPI doesn't provide image metadata, but these fields are available
  // since this class can wrap data received from Drive API (via a converter).
  EXPECT_EQ(-1, entry->image_width());
  EXPECT_EQ(-1, entry->image_height());
  EXPECT_EQ(-1, entry->image_rotation());
}

TEST(GDataWAPIParserTest, ClassifyEntryKindByFileExtension) {
  EXPECT_EQ(
      ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.gdoc"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.gsheet"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.gslides"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.gdraw"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.gtable"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_EXTERNAL_DOCUMENT |
      ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.glink"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_NONE,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.tar.gz"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_NONE,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test.txt"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_NONE,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath(FILE_PATH_LITERAL("Test"))));
  EXPECT_EQ(
      ResourceEntry::KIND_OF_NONE,
      ResourceEntry::ClassifyEntryKindByFileExtension(
          base::FilePath()));
}

TEST(GDataWAPIParserTest, ResourceEntryClassifyEntryKind) {
  EXPECT_EQ(ResourceEntry::KIND_OF_NONE,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_UNKNOWN));
  EXPECT_EQ(ResourceEntry::KIND_OF_NONE,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_ITEM));
  EXPECT_EQ(ResourceEntry::KIND_OF_NONE,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_SITE));
  EXPECT_EQ(ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_DOCUMENT));
  EXPECT_EQ(ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_SPREADSHEET));
  EXPECT_EQ(ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_PRESENTATION));
  EXPECT_EQ(ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_DRAWING));
  EXPECT_EQ(ResourceEntry::KIND_OF_GOOGLE_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_TABLE));
  EXPECT_EQ(ResourceEntry::KIND_OF_EXTERNAL_DOCUMENT |
            ResourceEntry::KIND_OF_HOSTED_DOCUMENT,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_EXTERNAL_APP));
  EXPECT_EQ(ResourceEntry::KIND_OF_FOLDER,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_FOLDER));
  EXPECT_EQ(ResourceEntry::KIND_OF_FILE,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_FILE));
  EXPECT_EQ(ResourceEntry::KIND_OF_FILE,
            ResourceEntry::ClassifyEntryKind(ENTRY_KIND_PDF));
}

}  // namespace google_apis
