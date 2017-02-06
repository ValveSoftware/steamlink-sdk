// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal
}  // namespace base

namespace google_apis {

// About resource represents the account information about the current user.
// https://developers.google.com/drive/v2/reference/about
class AboutResource {
 public:
  AboutResource();
  ~AboutResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AboutResource>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<AboutResource> CreateFrom(const base::Value& value);

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }
  // Returns total number of quota bytes.
  int64_t quota_bytes_total() const { return quota_bytes_total_; }
  // Returns the number of quota bytes used.
  int64_t quota_bytes_used_aggregate() const {
    return quota_bytes_used_aggregate_;
  }
  // Returns root folder ID.
  const std::string& root_folder_id() const { return root_folder_id_; }

  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }
  void set_quota_bytes_total(int64_t quota_bytes_total) {
    quota_bytes_total_ = quota_bytes_total;
  }
  void set_quota_bytes_used_aggregate(int64_t quota_bytes_used_aggregate) {
    quota_bytes_used_aggregate_ = quota_bytes_used_aggregate;
  }
  void set_root_folder_id(const std::string& root_folder_id) {
    root_folder_id_ = root_folder_id;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AboutResourceParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64_t largest_change_id_;
  int64_t quota_bytes_total_;
  int64_t quota_bytes_used_aggregate_;
  std::string root_folder_id_;

  // This class is copyable on purpose.
};

// DriveAppIcon represents an icon for Drive Application.
// https://developers.google.com/drive/v2/reference/apps
class DriveAppIcon {
 public:
  enum IconCategory {
    UNKNOWN,          // Uninitialized state.
    DOCUMENT,         // Icon for a file associated with the app.
    APPLICATION,      // Icon for the application.
    SHARED_DOCUMENT,  // Icon for a shared file associated with the app.
  };

  DriveAppIcon();
  ~DriveAppIcon();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DriveAppIcon>* converter);

  // Creates drive app icon instance from parsed JSON.
  static std::unique_ptr<DriveAppIcon> CreateFrom(const base::Value& value);

  // Category of the icon.
  IconCategory category() const { return category_; }

  // Size in pixels of one side of the icon (icons are always square).
  int icon_side_length() const { return icon_side_length_; }

  // Returns URL for this icon.
  const GURL& icon_url() const { return icon_url_; }

  void set_category(IconCategory category) {
    category_ = category;
  }
  void set_icon_side_length(int icon_side_length) {
    icon_side_length_ = icon_side_length;
  }
  void set_icon_url(const GURL& icon_url) {
    icon_url_ = icon_url;
  }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  // Extracts the icon category from the given string. Returns false and does
  // not change |result| when |scheme| has an unrecognizable value.
  static bool GetIconCategory(const base::StringPiece& category,
                              IconCategory* result);

  friend class base::internal::RepeatedMessageConverter<DriveAppIcon>;
  friend class AppResource;

  IconCategory category_;
  int icon_side_length_;
  GURL icon_url_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppIcon);
};

// AppResource represents a Drive Application.
// https://developers.google.com/drive/v2/reference/apps
class AppResource {
 public:
  ~AppResource();
  AppResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppResource>* converter);

  // Creates app resource from parsed JSON.
  static std::unique_ptr<AppResource> CreateFrom(const base::Value& value);

  // Returns application ID, which is 12-digit decimals (e.g. "123456780123").
  const std::string& application_id() const { return application_id_; }

  // Returns application name.
  const std::string& name() const { return name_; }

  // Returns the name of the type of object this application creates.
  // This is used for displaying in "Create" menu item for this app.
  // If empty, application name is used instead.
  const std::string& object_type() const { return object_type_; }

  // Returns the product ID.
  const std::string& product_id() const { return product_id_; }

  // Returns whether this application supports creating new objects.
  bool supports_create() const { return supports_create_; }

  // Returns whether this application is removable by apps.delete API.
  bool is_removable() const { return removable_; }

  // Returns the create URL, i.e., the URL for opening a new file by the app.
  const GURL& create_url() const { return create_url_; }

  // List of primary mime types supported by this WebApp. Primary status should
  // trigger this WebApp becoming the default handler of file instances that
  // have these mime types.
  const ScopedVector<std::string>& primary_mimetypes() const {
    return primary_mimetypes_;
  }

  // List of secondary mime types supported by this WebApp. Secondary status
  // should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching mime types.
  const ScopedVector<std::string>& secondary_mimetypes() const {
    return secondary_mimetypes_;
  }

  // List of primary file extensions supported by this WebApp. Primary status
  // should trigger this WebApp becoming the default handler of file instances
  // that match these extensions.
  const ScopedVector<std::string>& primary_file_extensions() const {
    return primary_file_extensions_;
  }

  // List of secondary file extensions supported by this WebApp. Secondary
  // status should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching extensions.
  const ScopedVector<std::string>& secondary_file_extensions() const {
    return secondary_file_extensions_;
  }

  // Returns Icons for this application.  An application can have multiple
  // icons for different purpose (application, document, shared document)
  // in several sizes.
  const ScopedVector<DriveAppIcon>& icons() const {
    return icons_;
  }

  void set_application_id(const std::string& application_id) {
    application_id_ = application_id;
  }
  void set_name(const std::string& name) { name_ = name; }
  void set_object_type(const std::string& object_type) {
    object_type_ = object_type;
  }
  void set_product_id(const std::string& id) { product_id_ = id; }
  void set_supports_create(bool supports_create) {
    supports_create_ = supports_create;
  }
  void set_removable(bool removable) { removable_ = removable; }
  void set_primary_mimetypes(
      ScopedVector<std::string> primary_mimetypes) {
    primary_mimetypes_ = std::move(primary_mimetypes);
  }
  void set_secondary_mimetypes(
      ScopedVector<std::string> secondary_mimetypes) {
    secondary_mimetypes_ = std::move(secondary_mimetypes);
  }
  void set_primary_file_extensions(
      ScopedVector<std::string> primary_file_extensions) {
    primary_file_extensions_ = std::move(primary_file_extensions);
  }
  void set_secondary_file_extensions(
      ScopedVector<std::string> secondary_file_extensions) {
    secondary_file_extensions_ = std::move(secondary_file_extensions);
  }
  void set_icons(ScopedVector<DriveAppIcon> icons) {
    icons_ = std::move(icons);
  }
  void set_create_url(const GURL& url) {
    create_url_ = url;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<AppResource>;
  friend class AppList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string application_id_;
  std::string name_;
  std::string object_type_;
  std::string product_id_;
  bool supports_create_;
  bool removable_;
  GURL create_url_;
  ScopedVector<std::string> primary_mimetypes_;
  ScopedVector<std::string> secondary_mimetypes_;
  ScopedVector<std::string> primary_file_extensions_;
  ScopedVector<std::string> secondary_file_extensions_;
  ScopedVector<DriveAppIcon> icons_;

  DISALLOW_COPY_AND_ASSIGN(AppResource);
};

// AppList represents a list of Drive Applications.
// https://developers.google.com/drive/v2/reference/apps/list
class AppList {
 public:
  AppList();
  ~AppList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppList>* converter);

  // Creates app list from parsed JSON.
  static std::unique_ptr<AppList> CreateFrom(const base::Value& value);

  // ETag for this resource.
  const std::string& etag() const { return etag_; }

  // Returns a vector of applications.
  const ScopedVector<AppResource>& items() const { return items_; }

  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_items(ScopedVector<AppResource> items) { items_ = std::move(items); }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AppListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string etag_;
  ScopedVector<AppResource> items_;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

// ParentReference represents a directory.
// https://developers.google.com/drive/v2/reference/parents
class ParentReference {
 public:
  ParentReference();
  ~ParentReference();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ParentReference>* converter);

  // Creates parent reference from parsed JSON.
  static std::unique_ptr<ParentReference> CreateFrom(const base::Value& value);

  // Returns the file id of the reference.
  const std::string& file_id() const { return file_id_; }

  // Returns the URL for the parent in Drive.
  const GURL& parent_link() const { return parent_link_; }

  void set_file_id(const std::string& file_id) { file_id_ = file_id; }
  void set_parent_link(const GURL& parent_link) {
    parent_link_ = parent_link;
  }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
  GURL parent_link_;
};

// FileLabels represents labels for file or folder.
// https://developers.google.com/drive/v2/reference/files
class FileLabels {
 public:
  FileLabels();
  ~FileLabels();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileLabels>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<FileLabels> CreateFrom(const base::Value& value);

  // Whether this file has been trashed.
  bool is_trashed() const { return trashed_; }

  void set_trashed(bool trashed) { trashed_ = trashed; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  bool trashed_;
};

// ImageMediaMetadata represents image metadata for a file.
// https://developers.google.com/drive/v2/reference/files
class ImageMediaMetadata {
 public:
  ImageMediaMetadata();
  ~ImageMediaMetadata();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ImageMediaMetadata>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<ImageMediaMetadata> CreateFrom(
      const base::Value& value);

  // Width of the image in pixels.
  int width() const { return width_; }
  // Height of the image in pixels.
  int height() const { return height_; }
  // Rotation of the image in clockwise degrees.
  int rotation() const { return rotation_; }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }
  void set_rotation(int rotation) { rotation_ = rotation; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int width_;
  int height_;
  int rotation_;
};

// FileResource represents a file or folder metadata in Drive.
// https://developers.google.com/drive/v2/reference/files
class FileResource {
 public:
  // Link to open a file resource on a web app with |app_id|.
  struct OpenWithLink {
    std::string app_id;
    GURL open_url;
  };

  FileResource();
  FileResource(const FileResource& other);
  ~FileResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileResource>* converter);

  // Creates file resource from parsed JSON.
  static std::unique_ptr<FileResource> CreateFrom(const base::Value& value);

  // Returns true if this is a directory.
  // Note: "folder" is used elsewhere in this file to match Drive API reference,
  // but outside this file we use "directory" to match HTML5 filesystem API.
  bool IsDirectory() const;

  // Returns true if this is a hosted document.
  // A hosted document is a document in one of Google Docs formats (Documents,
  // Spreadsheets, Slides, ...) whose content is not exposed via the API. It is
  // available only as |alternate_link()| to the document hosted on the server.
  bool IsHostedDocument() const;

  // Returns file ID.  This is unique in all files in Google Drive.
  const std::string& file_id() const { return file_id_; }

  // Returns ETag for this file.
  const std::string& etag() const { return etag_; }

  // Returns the title of this file.
  const std::string& title() const { return title_; }

  // Returns MIME type of this file.
  const std::string& mime_type() const { return mime_type_; }

  // Returns labels for this file.
  const FileLabels& labels() const { return labels_; }

  // Returns image media metadata for this file.
  const ImageMediaMetadata& image_media_metadata() const {
    return image_media_metadata_;
  }

  // Returns created time of this file.
  const base::Time& created_date() const { return created_date_; }

  // Returns modified time of this file.
  const base::Time& modified_date() const { return modified_date_; }

  // Returns last access time by the user.
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }

  // Returns time when the file was shared with the user.
  const base::Time& shared_with_me_date() const {
    return shared_with_me_date_;
  }

  // Returns the 'shared' attribute of the file.
  bool shared() const { return shared_; }

  // Returns MD5 checksum of this file.
  const std::string& md5_checksum() const { return md5_checksum_; }

  // Returns the size of this file in bytes.
  int64_t file_size() const { return file_size_; }

  // Return the link to open the file in Google editor or viewer.
  // E.g. Google Document, Google Spreadsheet.
  const GURL& alternate_link() const { return alternate_link_; }

  // Returns URL to the share dialog UI.
  const GURL& share_link() const { return share_link_; }

  // Returns parent references (directories) of this file.
  const std::vector<ParentReference>& parents() const { return parents_; }

  // Returns the list of links to open the resource with a web app.
  const std::vector<OpenWithLink>& open_with_links() const {
    return open_with_links_;
  }

  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_title(const std::string& title) {
    title_ = title;
  }
  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }
  FileLabels* mutable_labels() {
    return &labels_;
  }
  ImageMediaMetadata* mutable_image_media_metadata() {
    return &image_media_metadata_;
  }
  void set_created_date(const base::Time& created_date) {
    created_date_ = created_date;
  }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }
  void set_shared_with_me_date(const base::Time& shared_with_me_date) {
    shared_with_me_date_ = shared_with_me_date;
  }
  void set_shared(bool shared) {
    shared_ = shared;
  }
  void set_md5_checksum(const std::string& md5_checksum) {
    md5_checksum_ = md5_checksum;
  }
  void set_file_size(int64_t file_size) { file_size_ = file_size; }
  void set_alternate_link(const GURL& alternate_link) {
    alternate_link_ = alternate_link;
  }
  void set_share_link(const GURL& share_link) {
    share_link_ = share_link;
  }
  std::vector<ParentReference>* mutable_parents() { return &parents_; }
  std::vector<OpenWithLink>* mutable_open_with_links() {
    return &open_with_links_;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<FileResource>;
  friend class ChangeResource;
  friend class FileList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
  std::string etag_;
  std::string title_;
  std::string mime_type_;
  FileLabels labels_;
  ImageMediaMetadata image_media_metadata_;
  base::Time created_date_;
  base::Time modified_date_;
  base::Time last_viewed_by_me_date_;
  base::Time shared_with_me_date_;
  bool shared_;
  std::string md5_checksum_;
  int64_t file_size_;
  GURL alternate_link_;
  GURL share_link_;
  std::vector<ParentReference> parents_;
  std::vector<OpenWithLink> open_with_links_;
};

// FileList represents a collection of files and folders.
// https://developers.google.com/drive/v2/reference/files/list
class FileList {
 public:
  FileList();
  ~FileList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileList>* converter);

  // Returns true if the |value| has kind field for FileList.
  static bool HasFileListKind(const base::Value& value);

  // Creates file list from parsed JSON.
  static std::unique_ptr<FileList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns a set of files in this list.
  const ScopedVector<FileResource>& items() const { return items_; }
  ScopedVector<FileResource>* mutable_items() { return &items_; }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, FileListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  ScopedVector<FileResource> items_;

  DISALLOW_COPY_AND_ASSIGN(FileList);
};

// ChangeResource represents a change in a file.
// https://developers.google.com/drive/v2/reference/changes
class ChangeResource {
 public:
  ChangeResource();
  ~ChangeResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeResource>* converter);

  // Creates change resource from parsed JSON.
  static std::unique_ptr<ChangeResource> CreateFrom(const base::Value& value);

  // Returns change ID for this change.  This is a monotonically increasing
  // number.
  int64_t change_id() const { return change_id_; }

  // Returns a string file ID for corresponding file of the change.
  const std::string& file_id() const { return file_id_; }

  // Returns true if this file is deleted in the change.
  bool is_deleted() const { return deleted_; }

  // Returns FileResource of the file which the change refers to.
  const FileResource* file() const { return file_.get(); }
  FileResource* mutable_file() { return file_.get(); }

  // Returns the time of this modification.
  const base::Time& modification_date() const { return modification_date_; }

  void set_change_id(int64_t change_id) { change_id_ = change_id; }
  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_deleted(bool deleted) {
    deleted_ = deleted;
  }
  void set_file(std::unique_ptr<FileResource> file) { file_ = std::move(file); }
  void set_modification_date(const base::Time& modification_date) {
    modification_date_ = modification_date;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<ChangeResource>;
  friend class ChangeList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64_t change_id_;
  std::string file_id_;
  bool deleted_;
  std::unique_ptr<FileResource> file_;
  base::Time modification_date_;

  DISALLOW_COPY_AND_ASSIGN(ChangeResource);
};

// ChangeList represents a set of changes in the drive.
// https://developers.google.com/drive/v2/reference/changes/list
class ChangeList {
 public:
  ChangeList();
  ~ChangeList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeList>* converter);

  // Returns true if the |value| has kind field for ChangeList.
  static bool HasChangeListKind(const base::Value& value);

  // Creates change list from parsed JSON.
  static std::unique_ptr<ChangeList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }

  // Returns a set of changes in this list.
  const ScopedVector<ChangeResource>& items() const { return items_; }
  ScopedVector<ChangeResource>* mutable_items() { return &items_; }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }
  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, ChangeListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  int64_t largest_change_id_;
  ScopedVector<ChangeResource> items_;

  DISALLOW_COPY_AND_ASSIGN(ChangeList);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
