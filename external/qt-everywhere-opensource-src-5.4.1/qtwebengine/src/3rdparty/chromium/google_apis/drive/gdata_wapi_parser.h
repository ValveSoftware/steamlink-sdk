// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "google_apis/drive/drive_entry_kinds.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class DictionaryValue;
class Value;

template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal

}  // namespace base

// Defines data elements of Google Documents API as described in
// http://code.google.com/apis/documents/.
namespace google_apis {

// Defines link (URL) of an entity (document, file, feed...). Each entity could
// have more than one link representing it.
class Link {
 public:
  enum LinkType {
    LINK_UNKNOWN,
    LINK_SELF,
    LINK_NEXT,
    LINK_PARENT,
    LINK_ALTERNATE,
    LINK_EDIT,
    LINK_EDIT_MEDIA,
    LINK_ALT_EDIT_MEDIA,
    LINK_ALT_POST,
    LINK_FEED,
    LINK_POST,
    LINK_BATCH,
    LINK_RESUMABLE_EDIT_MEDIA,
    LINK_RESUMABLE_CREATE_MEDIA,
    LINK_TABLES_FEED,
    LINK_WORKSHEET_FEED,
    LINK_THUMBNAIL,
    LINK_EMBED,
    LINK_PRODUCT,
    LINK_ICON,
    LINK_OPEN_WITH,
    LINK_SHARE,
  };
  Link();
  ~Link();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(base::JSONValueConverter<Link>* converter);

  // Type of the link.
  LinkType type() const { return type_; }

  // URL of the link.
  const GURL& href() const { return href_; }

  // Title of the link.
  const std::string& title() const { return title_; }

  // For OPEN_WITH links, this contains the application ID. For all other link
  // types, it is the empty string.
  const std::string& app_id() const { return app_id_; }

  // Link MIME type.
  const std::string& mime_type() const { return mime_type_; }

  void set_type(LinkType type) { type_ = type; }
  void set_href(const GURL& href) { href_ = href; }
  void set_title(const std::string& title) { title_ = title; }
  void set_app_id(const std::string& app_id) { app_id_ = app_id; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  friend class ResourceEntry;
  // Converts value of link.rel into LinkType. Outputs to |type| and returns
  // true when |rel| has a valid value. Otherwise does nothing and returns
  // false.
  static bool GetLinkType(const base::StringPiece& rel, LinkType* type);

  // Converts value of link.rel to application ID, if there is one embedded in
  // the link.rel field. Outputs to |app_id| and returns true when |rel| has a
  // valid value. Otherwise does nothing and returns false.
  static bool GetAppID(const base::StringPiece& rel, std::string* app_id);

  LinkType type_;
  GURL href_;
  std::string title_;
  std::string app_id_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

// Feed links define links (URLs) to special list of entries (i.e. list of
// previous document revisions).
class ResourceLink {
 public:
  enum ResourceLinkType {
    FEED_LINK_UNKNOWN,
    FEED_LINK_ACL,
    FEED_LINK_REVISIONS,
  };
  ResourceLink();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceLink>* converter);

  // MIME type of the feed.
  ResourceLinkType type() const { return type_; }

  // URL of the feed.
  const GURL& href() const { return href_; }

  void set_type(ResourceLinkType type) { type_ = type; }
  void set_href(const GURL& href) { href_ = href; }

 private:
  friend class ResourceEntry;
  // Converts value of gd$feedLink.rel into ResourceLinkType enum.
  // Outputs to |result| and returns true when |rel| has a valid
  // value.  Otherwise does nothing and returns false.
  static bool GetFeedLinkType(
      const base::StringPiece& rel, ResourceLinkType* result);

  ResourceLinkType type_;
  GURL href_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLink);
};

// Author represents an author of an entity.
class Author {
 public:
  Author();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Author>* converter);

  // Getters.
  const std::string& name() const { return name_; }
  const std::string& email() const { return email_; }

  void set_name(const std::string& name) { name_ = name; }
  void set_email(const std::string& email) { email_ = email; }

 private:
  friend class ResourceEntry;

  std::string name_;
  std::string email_;

  DISALLOW_COPY_AND_ASSIGN(Author);
};

// Entry category.
class Category {
 public:
  enum CategoryType {
    CATEGORY_UNKNOWN,
    CATEGORY_ITEM,
    CATEGORY_KIND,
    CATEGORY_LABEL,
  };

  Category();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Category>* converter);

  // Category label.
  const std::string& label() const { return label_; }

  // Category type.
  CategoryType type() const { return type_; }

  // Category term.
  const std::string& term() const { return term_; }

  void set_label(const std::string& label) { label_ = label; }
  void set_type(CategoryType type) { type_ = type; }
  void set_term(const std::string& term) { term_ = term; }

 private:
  friend class ResourceEntry;
  // Converts category scheme into CategoryType enum. For example,
  // http://schemas.google.com/g/2005#kind => Category::CATEGORY_KIND
  // Returns false and does not change |result| when |scheme| has an
  // unrecognizable value.
  static bool GetCategoryTypeFromScheme(
      const base::StringPiece& scheme, CategoryType* result);

  std::string label_;
  CategoryType type_;
  std::string term_;

  DISALLOW_COPY_AND_ASSIGN(Category);
};

// Content details of a resource: mime-type, url, and so on.
class Content {
 public:
  Content();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Content>* converter);

  // The URL to download the file content.
  // Note that the url can expire, so we'll fetch the latest resource
  // entry before starting a download to get the download URL.
  const GURL& url() const { return url_; }
  const std::string& mime_type() const { return mime_type_; }

  void set_url(const GURL& url) { url_ = url; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  friend class ResourceEntry;

  GURL url_;
  std::string mime_type_;
};

// Base class for feed entries. This class defines fields commonly used by
// various feeds.
class CommonMetadata {
 public:
  CommonMetadata();
  virtual ~CommonMetadata();

  // Returns a link of a given |type| for this entry. If not found, it returns
  // NULL.
  const Link* GetLinkByType(Link::LinkType type) const;

  // Entry update time.
  base::Time updated_time() const { return updated_time_; }

  // Entry ETag.
  const std::string& etag() const { return etag_; }

  // List of entry authors.
  const ScopedVector<Author>& authors() const { return authors_; }

  // List of entry links.
  const ScopedVector<Link>& links() const { return links_; }
  ScopedVector<Link>* mutable_links() { return &links_; }

  // List of entry categories.
  const ScopedVector<Category>& categories() const { return categories_; }

  void set_etag(const std::string& etag) { etag_ = etag; }
  void set_authors(ScopedVector<Author> authors) {
    authors_ = authors.Pass();
  }
  void set_links(ScopedVector<Link> links) {
    links_ = links.Pass();
  }
  void set_categories(ScopedVector<Category> categories) {
    categories_ = categories.Pass();
  }
  void set_updated_time(const base::Time& updated_time) {
    updated_time_ = updated_time;
  }

 protected:
  // Registers the mapping between JSON field names and the members in
  // this class.
  template<typename CommonMetadataDescendant>
  static void RegisterJSONConverter(
      base::JSONValueConverter<CommonMetadataDescendant>* converter);

  std::string etag_;
  ScopedVector<Author> authors_;
  ScopedVector<Link> links_;
  ScopedVector<Category> categories_;
  base::Time updated_time_;

  DISALLOW_COPY_AND_ASSIGN(CommonMetadata);
};

// This class represents a resource entry. A resource is a generic term which
// refers to a file and a directory.
class ResourceEntry : public CommonMetadata {
 public:
  ResourceEntry();
  virtual ~ResourceEntry();

  // Extracts "entry" dictionary from the JSON value, and parse the contents,
  // using CreateFrom(). Returns NULL on failure. The input JSON data, coming
  // from the gdata server, looks like:
  //
  // {
  //   "encoding": "UTF-8",
  //   "entry": { ... },   // This function will extract this and parse.
  //   "version": "1.0"
  // }
  //
  // The caller should delete the returned object.
  static scoped_ptr<ResourceEntry> ExtractAndParse(const base::Value& value);

  // Creates resource entry from parsed JSON Value.  You should call
  // this instead of instantiating JSONValueConverter by yourself
  // because this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation for the details.
  static scoped_ptr<ResourceEntry> CreateFrom(const base::Value& value);

  // Returns name of entry node.
  static std::string GetEntryNodeName();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceEntry>* converter);

  // Sets true to |result| if the field exists.
  // Always returns true even when the field does not exist.
  static bool HasFieldPresent(const base::Value* value, bool* result);

  // Parses |value| as int64 and sets it to |result|. If the field does not
  // exist, sets 0 to |result| as default value.
  // Returns true if |value| is NULL or it is parsed as int64 successfully.
  static bool ParseChangestamp(const base::Value* value, int64* result);

  // The resource ID is used to identify a resource, which looks like:
  // file:d41d8cd98f00b204e9800998ecf8
  const std::string& resource_id() const { return resource_id_; }

  // This is a URL looks like:
  // https://docs.google.com/feeds/id/file%3Ad41d8cd98f00b204e9800998ecf8.
  // The URL is currently not used.
  const std::string& id() const { return id_; }

  DriveEntryKind kind() const { return kind_; }
  const std::string& title() const { return title_; }
  base::Time published_time() const { return published_time_; }
  base::Time last_viewed_time() const { return last_viewed_time_; }
  const std::vector<std::string>& labels() const { return labels_; }

  // The URL to download a file content.
  // Search for 'download_url' in gdata_wapi_requests.h for details.
  const GURL& download_url() const { return content_.url(); }

  const std::string& content_mime_type() const { return content_.mime_type(); }

  // The resource links contain extra links for revisions and access control,
  // etc.  Note that links() contain more basic links like edit URL,
  // alternative URL, etc.
  const ScopedVector<ResourceLink>& resource_links() const {
    return resource_links_;
  }

  // File name (exists only for kinds FILE and PDF).
  const std::string& filename() const { return filename_; }

  // Suggested file name (exists only for kinds FILE and PDF).
  const std::string& suggested_filename() const { return suggested_filename_; }

  // File content MD5 (exists only for kinds FILE and PDF).
  const std::string& file_md5() const { return file_md5_; }

  // File size (exists only for kinds FILE and PDF).
  int64 file_size() const { return file_size_; }

  // True if the file or directory is deleted (applicable to change list only).
  bool deleted() const { return deleted_ || removed_; }

  // Changestamp (exists only for change query results).
  // If not exists, defaults to 0.
  int64 changestamp() const { return changestamp_; }

  // Image width (exists only for images).
  // If doesn't exist, then equals -1.
  int64 image_width() const { return image_width_; }

  // Image height (exists only for images).
  // If doesn't exist, then equals -1.
  int64 image_height() const { return image_height_; }

  // Image rotation in clockwise degrees (exists only for images).
  // If doesn't exist, then equals -1.
  int64 image_rotation() const { return image_rotation_; }

  // The time of this modification.
  // Note: This property is filled only when converted from ChangeResource.
  const base::Time& modification_date() const { return modification_date_; }

  // Text version of resource entry kind. Returns an empty string for
  // unknown entry kind.
  std::string GetEntryKindText() const;

  // Returns preferred file extension for hosted documents. If entry is not
  // a hosted document, this call returns an empty string.
  static std::string GetHostedDocumentExtension(DriveEntryKind kind);

  // True if resource entry is remotely hosted.
  bool is_hosted_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_HOSTED_DOCUMENT) > 0;
  }
  // True if resource entry hosted by Google Documents.
  bool is_google_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_GOOGLE_DOCUMENT) > 0;
  }
  // True if resource entry is hosted by an external application.
  bool is_external_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_EXTERNAL_DOCUMENT) > 0;
  }
  // True if resource entry is a folder (collection).
  bool is_folder() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_FOLDER) > 0;
  }
  // True if resource entry is regular file.
  bool is_file() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_FILE) > 0;
  }
  // True if resource entry can't be mapped to the file system.
  bool is_special() const {
    return !is_file() && !is_folder() && !is_hosted_document();
  }

  // The following constructs are exposed for unit tests.

  // Classes of EntryKind. Used for ClassifyEntryKind().
  enum EntryKindClass {
    KIND_OF_NONE = 0,
    KIND_OF_HOSTED_DOCUMENT = 1,
    KIND_OF_GOOGLE_DOCUMENT = 1 << 1,
    KIND_OF_EXTERNAL_DOCUMENT = 1 << 2,
    KIND_OF_FOLDER = 1 << 3,
    KIND_OF_FILE = 1 << 4,
  };

  // Returns the kind enum corresponding to the extension in form ".xxx".
  static DriveEntryKind GetEntryKindFromExtension(const std::string& extension);

  // Classifies the EntryKind. The returned value is a bitmask of
  // EntryKindClass. For example, DOCUMENT is classified as
  // KIND_OF_HOSTED_DOCUMENT and KIND_OF_GOOGLE_DOCUMENT, hence the returned
  // value is KIND_OF_HOSTED_DOCUMENT | KIND_OF_GOOGLE_DOCUMENT.
  static int ClassifyEntryKind(DriveEntryKind kind);

  // Classifies the EntryKind by the file extension of specific path. The
  // returned value is a bitmask of EntryKindClass. See also ClassifyEntryKind.
  static int ClassifyEntryKindByFileExtension(const base::FilePath& file);

  void set_resource_id(const std::string& resource_id) {
    resource_id_ = resource_id;
  }
  void set_id(const std::string& id) { id_ = id; }
  void set_kind(DriveEntryKind kind) { kind_ = kind; }
  void set_title(const std::string& title) { title_ = title; }
  void set_published_time(const base::Time& published_time) {
    published_time_ = published_time;
  }
  void set_last_viewed_time(const base::Time& last_viewed_time) {
    last_viewed_time_ = last_viewed_time;
  }
  void set_labels(const std::vector<std::string>& labels) {
    labels_ = labels;
  }
  void set_content(const Content& content) {
    content_ = content;
  }
  void set_resource_links(ScopedVector<ResourceLink> resource_links) {
    resource_links_ = resource_links.Pass();
  }
  void set_filename(const std::string& filename) { filename_ = filename; }
  void set_suggested_filename(const std::string& suggested_filename) {
    suggested_filename_ = suggested_filename;
  }
  void set_file_md5(const std::string& file_md5) { file_md5_ = file_md5; }
  void set_file_size(int64 file_size) { file_size_ = file_size; }
  void set_deleted(bool deleted) { deleted_ = deleted; }
  void set_removed(bool removed) { removed_ = removed; }
  void set_changestamp(int64 changestamp) { changestamp_ = changestamp; }
  void set_image_width(int64 image_width) { image_width_ = image_width; }
  void set_image_height(int64 image_height) { image_height_ = image_height; }
  void set_image_rotation(int64 image_rotation) {
    image_rotation_ = image_rotation;
  }
  void set_modification_date(const base::Time& modification_date) {
    modification_date_ = modification_date;
  }

  // Fills the remaining fields where JSONValueConverter cannot catch.
  // Currently, sets |kind_| and |labels_| based on the |categories_| in the
  // class.
  void FillRemainingFields();

 private:
  friend class base::internal::RepeatedMessageConverter<ResourceEntry>;
  friend class ResourceList;
  friend class ResumeUploadRequest;

  // Converts categories.term into DriveEntryKind enum.
  static DriveEntryKind GetEntryKindFromTerm(const std::string& term);
  // Converts |kind| into its text identifier equivalent.
  static const char* GetEntryKindDescription(DriveEntryKind kind);

  std::string resource_id_;
  std::string id_;
  DriveEntryKind kind_;
  std::string title_;
  base::Time published_time_;
  // Last viewed value may be unreliable. See: crbug.com/152628.
  base::Time last_viewed_time_;
  std::vector<std::string> labels_;
  Content content_;
  ScopedVector<ResourceLink> resource_links_;
  // Optional fields for files only.
  std::string filename_;
  std::string suggested_filename_;
  std::string file_md5_;
  int64 file_size_;
  bool deleted_;
  bool removed_;
  int64 changestamp_;
  int64 image_width_;
  int64 image_height_;
  int64 image_rotation_;

  base::Time modification_date_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEntry);
};

// This class represents a list of resource entries with some extra metadata
// such as the root upload URL. The feed is paginated and the rest of the
// feed can be fetched by retrieving the remaining parts of the feed from
// URLs provided by GetNextFeedURL() method.
class ResourceList : public CommonMetadata {
 public:
  ResourceList();
  virtual ~ResourceList();

  // Extracts "feed" dictionary from the JSON value, and parse the contents,
  // using CreateFrom(). Returns NULL on failure. The input JSON data, coming
  // from the gdata server, looks like:
  //
  // {
  //   "encoding": "UTF-8",
  //   "feed": { ... },   // This function will extract this and parse.
  //   "version": "1.0"
  // }
  static scoped_ptr<ResourceList> ExtractAndParse(const base::Value& value);

  // Creates feed from parsed JSON Value.  You should call this
  // instead of instantiating JSONValueConverter by yourself because
  // this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation in ResourceEntry
  // class for the details.
  static scoped_ptr<ResourceList> CreateFrom(const base::Value& value);

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceList>* converter);

  // Returns true and passes|url| of the next feed if the current entry list
  // does not completed this feed.
  bool GetNextFeedURL(GURL* url) const;

  // List of resource entries.
  const ScopedVector<ResourceEntry>& entries() const { return entries_; }
  ScopedVector<ResourceEntry>* mutable_entries() { return &entries_; }

  // Releases entries_ into |entries|. This is a transfer of ownership, so the
  // caller is responsible for deleting the elements of |entries|.
  void ReleaseEntries(std::vector<ResourceEntry*>* entries);

  // Start index of the resource entry list.
  int start_index() const { return start_index_; }

  // Number of items per feed of the resource entry list.
  int items_per_page() const { return items_per_page_; }

  // The largest changestamp. Next time the resource list should be fetched
  // from this changestamp.
  int64 largest_changestamp() const { return largest_changestamp_; }

  // Resource entry list title.
  const std::string& title() { return title_; }

  void set_entries(ScopedVector<ResourceEntry> entries) {
    entries_ = entries.Pass();
  }
  void set_start_index(int start_index) {
    start_index_ = start_index;
  }
  void set_items_per_page(int items_per_page) {
    items_per_page_ = items_per_page;
  }
  void set_title(const std::string& title) {
    title_ = title;
  }
  void set_largest_changestamp(int64 largest_changestamp) {
    largest_changestamp_ = largest_changestamp;
  }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  ScopedVector<ResourceEntry> entries_;
  int start_index_;
  int items_per_page_;
  std::string title_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(ResourceList);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_
