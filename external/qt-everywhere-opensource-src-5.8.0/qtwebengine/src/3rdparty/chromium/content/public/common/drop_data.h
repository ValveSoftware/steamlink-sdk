// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A struct for managing data being dropped on a WebContents.  This represents
// a union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef CONTENT_PUBLIC_COMMON_DROP_DATA_H_
#define CONTENT_PUBLIC_COMMON_DROP_DATA_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/dragdrop/file_info.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT DropData {
  struct FileSystemFileInfo {
    FileSystemFileInfo() : size(0) {}
    ~FileSystemFileInfo() {}

    GURL url;
    int64_t size;
  };

  enum class Kind {
    STRING = 0,
    FILENAME,
    FILESYSTEMFILE,
    LAST = FILESYSTEMFILE
  };

  struct Metadata {
    Metadata();
    static Metadata CreateForMimeType(const Kind& kind,
                                      const base::string16& mime_type);
    static Metadata CreateForFilePath(const base::FilePath& filename);
    static Metadata CreateForFileSystemUrl(const GURL& file_system_url);
    Metadata(const Metadata& other);
    ~Metadata();

    Kind kind;
    base::string16 mime_type;
    base::FilePath filename;
    GURL file_system_url;
  };

  DropData();
  DropData(const DropData& other);
  ~DropData();

  int view_id = MSG_ROUTING_NONE;

  // Whether this drag originated from a renderer.
  bool did_originate_from_renderer;

  // User is dragging a link into the webview.
  GURL url;
  base::string16 url_title;  // The title associated with |url|.

  // User is dragging a link out-of the webview.
  base::string16 download_metadata;

  // Referrer policy to use when dragging a link out of the webview results in
  // a download.
  blink::WebReferrerPolicy referrer_policy;

  // User is dropping one or more files on the webview. This field is only
  // populated if the drag is not renderer tainted, as this allows File access
  // from web content.
  std::vector<ui::FileInfo> filenames;
  // The mime types of dragged files.
  std::vector<base::string16> file_mime_types;

  // Isolated filesystem ID for the files being dragged on the webview.
  base::string16 filesystem_id;

  // User is dragging files specified with filesystem: URLs.
  std::vector<FileSystemFileInfo> file_system_files;

  // User is dragging plain text into the webview.
  base::NullableString16 text;

  // User is dragging text/html into the webview (e.g., out of Firefox).
  // |html_base_url| is the URL that the html fragment is taken from (used to
  // resolve relative links).  It's ok for |html_base_url| to be empty.
  base::NullableString16 html;
  GURL html_base_url;

  // User is dragging data from the webview (e.g., an image).
  base::string16 file_description_filename;
  std::string file_contents;

  std::map<base::string16, base::string16> custom_data;

  // The key-modifiers present for this update, included here so BrowserPlugin
  // can forward them to the guest renderer.
  // TODO(wjmaclean): This can probably be removed when BrowserPlugin goes
  // away, https://crbug.com/533069.
  int key_modifiers;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_DROP_DATA_H_
