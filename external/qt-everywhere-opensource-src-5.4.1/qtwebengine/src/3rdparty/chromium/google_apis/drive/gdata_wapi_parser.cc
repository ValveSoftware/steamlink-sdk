// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_parser.h"

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/json/json_value_converter.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/drive/time_util.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace google_apis {

namespace {

// Term values for kSchemeKind category:
const char kTermPrefix[] = "http://schemas.google.com/docs/2007#";

// Node names.
const char kEntryNode[] = "entry";

// Field names.
const char kAuthorField[] = "author";
const char kCategoryField[] = "category";
const char kChangestampField[] = "docs$changestamp.value";
const char kContentField[] = "content";
const char kDeletedField[] = "gd$deleted";
const char kETagField[] = "gd$etag";
const char kEmailField[] = "email.$t";
const char kEntryField[] = "entry";
const char kFeedField[] = "feed";
const char kFeedLinkField[] = "gd$feedLink";
const char kFileNameField[] = "docs$filename.$t";
const char kHrefField[] = "href";
const char kIDField[] = "id.$t";
const char kItemsPerPageField[] = "openSearch$itemsPerPage.$t";
const char kLabelField[] = "label";
const char kLargestChangestampField[] = "docs$largestChangestamp.value";
const char kLastViewedField[] = "gd$lastViewed.$t";
const char kLinkField[] = "link";
const char kMD5Field[] = "docs$md5Checksum.$t";
const char kNameField[] = "name.$t";
const char kPublishedField[] = "published.$t";
const char kRelField[] = "rel";
const char kRemovedField[] = "docs$removed";
const char kResourceIdField[] = "gd$resourceId.$t";
const char kSchemeField[] = "scheme";
const char kSizeField[] = "docs$size.$t";
const char kSrcField[] = "src";
const char kStartIndexField[] = "openSearch$startIndex.$t";
const char kSuggestedFileNameField[] = "docs$suggestedFilename.$t";
const char kTermField[] = "term";
const char kTitleField[] = "title";
const char kTitleTField[] = "title.$t";
const char kTypeField[] = "type";
const char kUpdatedField[] = "updated.$t";

// Link Prefixes
const char kOpenWithPrefix[] = "http://schemas.google.com/docs/2007#open-with-";
const size_t kOpenWithPrefixSize = arraysize(kOpenWithPrefix) - 1;

struct EntryKindMap {
  DriveEntryKind kind;
  const char* entry;
  const char* extension;
};

const EntryKindMap kEntryKindMap[] = {
    { ENTRY_KIND_UNKNOWN,      "unknown",      NULL},
    { ENTRY_KIND_ITEM,         "item",         NULL},
    { ENTRY_KIND_DOCUMENT,     "document",     ".gdoc"},
    { ENTRY_KIND_SPREADSHEET,  "spreadsheet",  ".gsheet"},
    { ENTRY_KIND_PRESENTATION, "presentation", ".gslides" },
    { ENTRY_KIND_DRAWING,      "drawing",      ".gdraw"},
    { ENTRY_KIND_TABLE,        "table",        ".gtable"},
    { ENTRY_KIND_FORM,         "form",         ".gform"},
    { ENTRY_KIND_EXTERNAL_APP, "externalapp",  ".glink"},
    { ENTRY_KIND_SITE,         "site",         NULL},
    { ENTRY_KIND_FOLDER,       "folder",       NULL},
    { ENTRY_KIND_FILE,         "file",         NULL},
    { ENTRY_KIND_PDF,          "pdf",          NULL},
};
COMPILE_ASSERT(arraysize(kEntryKindMap) == ENTRY_KIND_MAX_VALUE,
               EntryKindMap_and_DriveEntryKind_are_not_in_sync);

struct LinkTypeMap {
  Link::LinkType type;
  const char* rel;
};

const LinkTypeMap kLinkTypeMap[] = {
    { Link::LINK_SELF,
      "self" },
    { Link::LINK_NEXT,
      "next" },
    { Link::LINK_PARENT,
      "http://schemas.google.com/docs/2007#parent" },
    { Link::LINK_ALTERNATE,
      "alternate"},
    { Link::LINK_EDIT,
      "edit" },
    { Link::LINK_EDIT_MEDIA,
      "edit-media" },
    { Link::LINK_ALT_EDIT_MEDIA,
      "http://schemas.google.com/docs/2007#alt-edit-media" },
    { Link::LINK_ALT_POST,
      "http://schemas.google.com/docs/2007#alt-post" },
    { Link::LINK_FEED,
      "http://schemas.google.com/g/2005#feed"},
    { Link::LINK_POST,
      "http://schemas.google.com/g/2005#post"},
    { Link::LINK_BATCH,
      "http://schemas.google.com/g/2005#batch"},
    { Link::LINK_THUMBNAIL,
      "http://schemas.google.com/docs/2007/thumbnail"},
    { Link::LINK_RESUMABLE_EDIT_MEDIA,
      "http://schemas.google.com/g/2005#resumable-edit-media"},
    { Link::LINK_RESUMABLE_CREATE_MEDIA,
      "http://schemas.google.com/g/2005#resumable-create-media"},
    { Link::LINK_TABLES_FEED,
      "http://schemas.google.com/spreadsheets/2006#tablesfeed"},
    { Link::LINK_WORKSHEET_FEED,
      "http://schemas.google.com/spreadsheets/2006#worksheetsfeed"},
    { Link::LINK_EMBED,
      "http://schemas.google.com/docs/2007#embed"},
    { Link::LINK_PRODUCT,
      "http://schemas.google.com/docs/2007#product"},
    { Link::LINK_ICON,
      "http://schemas.google.com/docs/2007#icon"},
    { Link::LINK_SHARE,
      "http://schemas.google.com/docs/2007#share"},
};

struct ResourceLinkTypeMap {
  ResourceLink::ResourceLinkType type;
  const char* rel;
};

const ResourceLinkTypeMap kFeedLinkTypeMap[] = {
    { ResourceLink::FEED_LINK_ACL,
      "http://schemas.google.com/acl/2007#accessControlList" },
    { ResourceLink::FEED_LINK_REVISIONS,
      "http://schemas.google.com/docs/2007/revisions" },
};

struct CategoryTypeMap {
  Category::CategoryType type;
  const char* scheme;
};

const CategoryTypeMap kCategoryTypeMap[] = {
    { Category::CATEGORY_KIND, "http://schemas.google.com/g/2005#kind" },
    { Category::CATEGORY_LABEL, "http://schemas.google.com/g/2005/labels" },
};

// Converts |url_string| to |result|.  Always returns true to be used
// for JSONValueConverter::RegisterCustomField method.
// TODO(mukai): make it return false in case of invalid |url_string|.
bool GetGURLFromString(const base::StringPiece& url_string, GURL* result) {
  *result = GURL(url_string.as_string());
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Author implementation

Author::Author() {
}

// static
void Author::RegisterJSONConverter(
    base::JSONValueConverter<Author>* converter) {
  converter->RegisterStringField(kNameField, &Author::name_);
  converter->RegisterStringField(kEmailField, &Author::email_);
}

////////////////////////////////////////////////////////////////////////////////
// Link implementation

Link::Link() : type_(Link::LINK_UNKNOWN) {
}

Link::~Link() {
}

// static
bool Link::GetAppID(const base::StringPiece& rel, std::string* app_id) {
  DCHECK(app_id);
  // Fast return path if the link clearly isn't an OPEN_WITH link.
  if (rel.size() < kOpenWithPrefixSize) {
    app_id->clear();
    return true;
  }

  const std::string kOpenWithPrefixStr(kOpenWithPrefix);
  if (StartsWithASCII(rel.as_string(), kOpenWithPrefixStr, false)) {
    *app_id = rel.as_string().substr(kOpenWithPrefixStr.size());
    return true;
  }

  app_id->clear();
  return true;
}

// static.
bool Link::GetLinkType(const base::StringPiece& rel, Link::LinkType* type) {
  DCHECK(type);
  for (size_t i = 0; i < arraysize(kLinkTypeMap); i++) {
    if (rel == kLinkTypeMap[i].rel) {
      *type = kLinkTypeMap[i].type;
      return true;
    }
  }

  // OPEN_WITH links have extra information at the end of the rel that is unique
  // for each one, so we can't just check the usual map. This check is slightly
  // redundant to provide a quick skip if it's obviously not an OPEN_WITH url.
  if (rel.size() >= kOpenWithPrefixSize &&
      StartsWithASCII(rel.as_string(), kOpenWithPrefix, false)) {
    *type = LINK_OPEN_WITH;
    return true;
  }

  // Let unknown link types through, just report it; if the link type is needed
  // in the future, add it into LinkType and kLinkTypeMap.
  DVLOG(1) << "Ignoring unknown link type for rel " << rel;
  *type = LINK_UNKNOWN;
  return true;
}

// static
void Link::RegisterJSONConverter(base::JSONValueConverter<Link>* converter) {
  converter->RegisterCustomField<Link::LinkType>(kRelField,
                                                 &Link::type_,
                                                 &Link::GetLinkType);
  // We have to register kRelField twice because we extract two different pieces
  // of data from the same rel field.
  converter->RegisterCustomField<std::string>(kRelField,
                                              &Link::app_id_,
                                              &Link::GetAppID);
  converter->RegisterCustomField(kHrefField, &Link::href_, &GetGURLFromString);
  converter->RegisterStringField(kTitleField, &Link::title_);
  converter->RegisterStringField(kTypeField, &Link::mime_type_);
}

////////////////////////////////////////////////////////////////////////////////
// ResourceLink implementation

ResourceLink::ResourceLink() : type_(ResourceLink::FEED_LINK_UNKNOWN) {
}

// static.
bool ResourceLink::GetFeedLinkType(
    const base::StringPiece& rel, ResourceLink::ResourceLinkType* result) {
  for (size_t i = 0; i < arraysize(kFeedLinkTypeMap); i++) {
    if (rel == kFeedLinkTypeMap[i].rel) {
      *result = kFeedLinkTypeMap[i].type;
      return true;
    }
  }
  DVLOG(1) << "Unknown feed link type for rel " << rel;
  return false;
}

// static
void ResourceLink::RegisterJSONConverter(
    base::JSONValueConverter<ResourceLink>* converter) {
  converter->RegisterCustomField<ResourceLink::ResourceLinkType>(
      kRelField, &ResourceLink::type_, &ResourceLink::GetFeedLinkType);
  converter->RegisterCustomField(
      kHrefField, &ResourceLink::href_, &GetGURLFromString);
}

////////////////////////////////////////////////////////////////////////////////
// Category implementation

Category::Category() : type_(CATEGORY_UNKNOWN) {
}

// Converts category.scheme into CategoryType enum.
bool Category::GetCategoryTypeFromScheme(
    const base::StringPiece& scheme, Category::CategoryType* result) {
  for (size_t i = 0; i < arraysize(kCategoryTypeMap); i++) {
    if (scheme == kCategoryTypeMap[i].scheme) {
      *result = kCategoryTypeMap[i].type;
      return true;
    }
  }
  DVLOG(1) << "Unknown feed link type for scheme " << scheme;
  return false;
}

// static
void Category::RegisterJSONConverter(
    base::JSONValueConverter<Category>* converter) {
  converter->RegisterStringField(kLabelField, &Category::label_);
  converter->RegisterCustomField<Category::CategoryType>(
      kSchemeField, &Category::type_, &Category::GetCategoryTypeFromScheme);
  converter->RegisterStringField(kTermField, &Category::term_);
}

const Link* CommonMetadata::GetLinkByType(Link::LinkType type) const {
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == type)
      return links_[i];
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Content implementation

Content::Content() {
}

// static
void Content::RegisterJSONConverter(
    base::JSONValueConverter<Content>* converter) {
  converter->RegisterCustomField(kSrcField, &Content::url_, &GetGURLFromString);
  converter->RegisterStringField(kTypeField, &Content::mime_type_);
}

////////////////////////////////////////////////////////////////////////////////
// CommonMetadata implementation

CommonMetadata::CommonMetadata() {
}

CommonMetadata::~CommonMetadata() {
}

// static
template<typename CommonMetadataDescendant>
void CommonMetadata::RegisterJSONConverter(
    base::JSONValueConverter<CommonMetadataDescendant>* converter) {
  converter->RegisterStringField(kETagField, &CommonMetadata::etag_);
  converter->template RegisterRepeatedMessage<Author>(
      kAuthorField, &CommonMetadata::authors_);
  converter->template RegisterRepeatedMessage<Link>(
      kLinkField, &CommonMetadata::links_);
  converter->template RegisterRepeatedMessage<Category>(
      kCategoryField, &CommonMetadata::categories_);
  converter->template RegisterCustomField<base::Time>(
      kUpdatedField, &CommonMetadata::updated_time_, &util::GetTimeFromString);
}

////////////////////////////////////////////////////////////////////////////////
// ResourceEntry implementation

ResourceEntry::ResourceEntry()
    : kind_(ENTRY_KIND_UNKNOWN),
      file_size_(0),
      deleted_(false),
      removed_(false),
      changestamp_(0),
      image_width_(-1),
      image_height_(-1),
      image_rotation_(-1) {
}

ResourceEntry::~ResourceEntry() {
}

bool ResourceEntry::HasFieldPresent(const base::Value* value,
                                    bool* result) {
  *result = (value != NULL);
  return true;
}

bool ResourceEntry::ParseChangestamp(const base::Value* value,
                                     int64* result) {
  DCHECK(result);
  if (!value) {
    *result = 0;
    return true;
  }

  std::string string_value;
  if (value->GetAsString(&string_value) &&
      base::StringToInt64(string_value, result))
    return true;

  return false;
}

// static
void ResourceEntry::RegisterJSONConverter(
    base::JSONValueConverter<ResourceEntry>* converter) {
  // Inherit the parent registrations.
  CommonMetadata::RegisterJSONConverter(converter);
  converter->RegisterStringField(
      kResourceIdField, &ResourceEntry::resource_id_);
  converter->RegisterStringField(kIDField, &ResourceEntry::id_);
  converter->RegisterStringField(kTitleTField, &ResourceEntry::title_);
  converter->RegisterCustomField<base::Time>(
      kPublishedField, &ResourceEntry::published_time_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kLastViewedField, &ResourceEntry::last_viewed_time_,
      &util::GetTimeFromString);
  converter->RegisterRepeatedMessage(
      kFeedLinkField, &ResourceEntry::resource_links_);
  converter->RegisterNestedField(kContentField, &ResourceEntry::content_);

  // File properties.  If the resource type is not a normal file, then
  // that's no problem because those feed must not have these fields
  // themselves, which does not report errors.
  converter->RegisterStringField(kFileNameField, &ResourceEntry::filename_);
  converter->RegisterStringField(kMD5Field, &ResourceEntry::file_md5_);
  converter->RegisterCustomField<int64>(
      kSizeField, &ResourceEntry::file_size_, &base::StringToInt64);
  converter->RegisterStringField(
      kSuggestedFileNameField, &ResourceEntry::suggested_filename_);
  // Deleted are treated as 'trashed' items on web client side. Removed files
  // are gone for good. We treat both cases as 'deleted' for this client.
  converter->RegisterCustomValueField<bool>(
      kDeletedField, &ResourceEntry::deleted_, &ResourceEntry::HasFieldPresent);
  converter->RegisterCustomValueField<bool>(
      kRemovedField, &ResourceEntry::removed_, &ResourceEntry::HasFieldPresent);
  converter->RegisterCustomValueField<int64>(
      kChangestampField, &ResourceEntry::changestamp_,
      &ResourceEntry::ParseChangestamp);
  // ImageMediaMetadata fields are not supported by WAPI.
}

// static
std::string ResourceEntry::GetHostedDocumentExtension(DriveEntryKind kind) {
  for (size_t i = 0; i < arraysize(kEntryKindMap); i++) {
    if (kEntryKindMap[i].kind == kind) {
      if (kEntryKindMap[i].extension)
        return std::string(kEntryKindMap[i].extension);
      else
        return std::string();
    }
  }
  return std::string();
}

// static
DriveEntryKind ResourceEntry::GetEntryKindFromExtension(
    const std::string& extension) {
  for (size_t i = 0; i < arraysize(kEntryKindMap); ++i) {
    const char* document_extension = kEntryKindMap[i].extension;
    if (document_extension && extension == document_extension)
      return kEntryKindMap[i].kind;
  }
  return ENTRY_KIND_UNKNOWN;
}

// static
int ResourceEntry::ClassifyEntryKindByFileExtension(
    const base::FilePath& file_path) {
#if defined(OS_WIN)
  std::string file_extension = base::WideToUTF8(file_path.Extension());
#else
  std::string file_extension = file_path.Extension();
#endif
  return ClassifyEntryKind(GetEntryKindFromExtension(file_extension));
}

// static
DriveEntryKind ResourceEntry::GetEntryKindFromTerm(
    const std::string& term) {
  if (!StartsWithASCII(term, kTermPrefix, false)) {
    DVLOG(1) << "Unexpected term prefix term " << term;
    return ENTRY_KIND_UNKNOWN;
  }

  std::string type = term.substr(strlen(kTermPrefix));
  for (size_t i = 0; i < arraysize(kEntryKindMap); i++) {
    if (type == kEntryKindMap[i].entry)
      return kEntryKindMap[i].kind;
  }
  DVLOG(1) << "Unknown entry type for term " << term << ", type " << type;
  return ENTRY_KIND_UNKNOWN;
}

// static
int ResourceEntry::ClassifyEntryKind(DriveEntryKind kind) {
  int classes = 0;

  // All DriveEntryKind members are listed here, so the compiler catches if a
  // newly added member is missing here.
  switch (kind) {
    case ENTRY_KIND_UNKNOWN:
    // Special entries.
    case ENTRY_KIND_ITEM:
    case ENTRY_KIND_SITE:
      break;

    // Hosted Google document.
    case ENTRY_KIND_DOCUMENT:
    case ENTRY_KIND_SPREADSHEET:
    case ENTRY_KIND_PRESENTATION:
    case ENTRY_KIND_DRAWING:
    case ENTRY_KIND_TABLE:
    case ENTRY_KIND_FORM:
      classes = KIND_OF_GOOGLE_DOCUMENT | KIND_OF_HOSTED_DOCUMENT;
      break;

    // Hosted external application document.
    case ENTRY_KIND_EXTERNAL_APP:
      classes = KIND_OF_EXTERNAL_DOCUMENT | KIND_OF_HOSTED_DOCUMENT;
      break;

    // Folders, collections.
    case ENTRY_KIND_FOLDER:
      classes = KIND_OF_FOLDER;
      break;

    // Regular files.
    case ENTRY_KIND_FILE:
    case ENTRY_KIND_PDF:
      classes = KIND_OF_FILE;
      break;

    case ENTRY_KIND_MAX_VALUE:
      NOTREACHED();
  }

  return classes;
}

void ResourceEntry::FillRemainingFields() {
  // Set |kind_| and |labels_| based on the |categories_| in the class.
  // JSONValueConverter does not have the ability to catch an element in a list
  // based on a predicate.  Thus we need to iterate over |categories_| and
  // find the elements to set these fields as a post-process.
  for (size_t i = 0; i < categories_.size(); ++i) {
    const Category* category = categories_[i];
    if (category->type() == Category::CATEGORY_KIND)
      kind_ = GetEntryKindFromTerm(category->term());
    else if (category->type() == Category::CATEGORY_LABEL)
      labels_.push_back(category->label());
  }
}

// static
scoped_ptr<ResourceEntry> ResourceEntry::ExtractAndParse(
    const base::Value& value) {
  const base::DictionaryValue* as_dict = NULL;
  const base::DictionaryValue* entry_dict = NULL;
  if (value.GetAsDictionary(&as_dict) &&
      as_dict->GetDictionary(kEntryField, &entry_dict)) {
    return ResourceEntry::CreateFrom(*entry_dict);
  }
  return scoped_ptr<ResourceEntry>();
}

// static
scoped_ptr<ResourceEntry> ResourceEntry::CreateFrom(const base::Value& value) {
  base::JSONValueConverter<ResourceEntry> converter;
  scoped_ptr<ResourceEntry> entry(new ResourceEntry());
  if (!converter.Convert(value, entry.get())) {
    DVLOG(1) << "Invalid resource entry!";
    return scoped_ptr<ResourceEntry>();
  }

  entry->FillRemainingFields();
  return entry.Pass();
}

// static
std::string ResourceEntry::GetEntryNodeName() {
  return kEntryNode;
}

////////////////////////////////////////////////////////////////////////////////
// ResourceList implementation

ResourceList::ResourceList()
    : start_index_(0),
      items_per_page_(0),
      largest_changestamp_(0) {
}

ResourceList::~ResourceList() {
}

// static
void ResourceList::RegisterJSONConverter(
    base::JSONValueConverter<ResourceList>* converter) {
  // inheritance
  CommonMetadata::RegisterJSONConverter(converter);
  // TODO(zelidrag): Once we figure out where these will be used, we should
  // check for valid start_index_ and items_per_page_ values.
  converter->RegisterCustomField<int>(
      kStartIndexField, &ResourceList::start_index_, &base::StringToInt);
  converter->RegisterCustomField<int>(
      kItemsPerPageField, &ResourceList::items_per_page_, &base::StringToInt);
  converter->RegisterStringField(kTitleTField, &ResourceList::title_);
  converter->RegisterRepeatedMessage(kEntryField, &ResourceList::entries_);
  converter->RegisterCustomField<int64>(
     kLargestChangestampField, &ResourceList::largest_changestamp_,
     &base::StringToInt64);
}

bool ResourceList::Parse(const base::Value& value) {
  base::JSONValueConverter<ResourceList> converter;
  if (!converter.Convert(value, this)) {
    DVLOG(1) << "Invalid resource list!";
    return false;
  }

  ScopedVector<ResourceEntry>::iterator iter = entries_.begin();
  while (iter != entries_.end()) {
    ResourceEntry* entry = (*iter);
    entry->FillRemainingFields();
    ++iter;
  }
  return true;
}

// static
scoped_ptr<ResourceList> ResourceList::ExtractAndParse(
    const base::Value& value) {
  const base::DictionaryValue* as_dict = NULL;
  const base::DictionaryValue* feed_dict = NULL;
  if (value.GetAsDictionary(&as_dict) &&
      as_dict->GetDictionary(kFeedField, &feed_dict)) {
    return ResourceList::CreateFrom(*feed_dict);
  }
  return scoped_ptr<ResourceList>();
}

// static
scoped_ptr<ResourceList> ResourceList::CreateFrom(const base::Value& value) {
  scoped_ptr<ResourceList> feed(new ResourceList());
  if (!feed->Parse(value)) {
    DVLOG(1) << "Invalid resource list!";
    return scoped_ptr<ResourceList>();
  }

  return feed.Pass();
}

bool ResourceList::GetNextFeedURL(GURL* url) const {
  DCHECK(url);
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == Link::LINK_NEXT) {
      *url = links_[i]->href();
      return true;
    }
  }
  return false;
}

void ResourceList::ReleaseEntries(std::vector<ResourceEntry*>* entries) {
  entries_.release(entries);
}

}  // namespace google_apis
