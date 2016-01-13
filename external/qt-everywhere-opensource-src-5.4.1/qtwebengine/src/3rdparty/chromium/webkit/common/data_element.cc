// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/data_element.h"

namespace webkit_common {

DataElement::DataElement()
    : type_(TYPE_UNKNOWN),
      bytes_(NULL),
      offset_(0),
      length_(kuint64max) {
}

DataElement::~DataElement() {}

void DataElement::SetToFilePathRange(
    const base::FilePath& path,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE;
  path_ = path;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToBlobRange(
    const std::string& blob_uuid,
    uint64 offset, uint64 length) {
  type_ = TYPE_BLOB;
  blob_uuid_ = blob_uuid;
  offset_ = offset;
  length_ = length;
}

void DataElement::SetToFileSystemUrlRange(
    const GURL& filesystem_url,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  type_ = TYPE_FILE_FILESYSTEM;
  filesystem_url_ = filesystem_url;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

}  // webkit_common
