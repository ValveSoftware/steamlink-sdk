// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_ENTRY_KINDS_H_
#define GOOGLE_APIS_DRIVE_DRIVE_ENTRY_KINDS_H_

namespace google_apis {

// DriveEntryKind specifies the kind of a Drive entry.
//
// kEntryKindMap in gdata_wapi_parser.cc should also be updated if you modify
// DriveEntryKind. The compiler will catch if they are not in sync.
enum DriveEntryKind {
  ENTRY_KIND_UNKNOWN,
  // Special entries.
  ENTRY_KIND_ITEM,
  ENTRY_KIND_SITE,
  // Hosted Google document.
  ENTRY_KIND_DOCUMENT,
  ENTRY_KIND_SPREADSHEET,
  ENTRY_KIND_PRESENTATION,
  ENTRY_KIND_DRAWING,
  ENTRY_KIND_TABLE,
  ENTRY_KIND_FORM,
  // Hosted external application document.
  ENTRY_KIND_EXTERNAL_APP,
  // Folders; collections.
  ENTRY_KIND_FOLDER,
  // Regular files.
  ENTRY_KIND_FILE,
  ENTRY_KIND_PDF,

  // This should be the last item.
  ENTRY_KIND_MAX_VALUE,
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_ENTRY_KINDS_H_
