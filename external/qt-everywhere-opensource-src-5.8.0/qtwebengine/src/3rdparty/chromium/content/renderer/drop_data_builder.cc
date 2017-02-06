// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/drop_data_builder.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "content/public/common/drop_data.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "ui/base/clipboard/clipboard.h"

using blink::WebDragData;
using blink::WebVector;

namespace content {

// static
DropData DropDataBuilder::Build(const WebDragData& drag_data) {
  DropData result;
  result.key_modifiers = drag_data.modifierKeyState();
  result.referrer_policy = blink::WebReferrerPolicyDefault;

  const WebVector<WebDragData::Item>& item_list = drag_data.items();
  for (size_t i = 0; i < item_list.size(); ++i) {
    const WebDragData::Item& item = item_list[i];
    switch (item.storageType) {
      case WebDragData::Item::StorageTypeString: {
        base::string16 str_type(item.stringType);
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeText)) {
          result.text = base::NullableString16(item.stringData, false);
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeURIList)) {
          result.url = blink::WebStringToGURL(item.stringData);
          result.url_title = item.title;
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeDownloadURL)) {
          result.download_metadata = item.stringData;
          break;
        }
        if (base::EqualsASCII(str_type, ui::Clipboard::kMimeTypeHTML)) {
          result.html = base::NullableString16(item.stringData, false);
          result.html_base_url = item.baseURL;
          break;
        }
        result.custom_data.insert(
            std::make_pair(item.stringType, item.stringData));
        break;
      }
      case WebDragData::Item::StorageTypeBinaryData:
        result.file_contents.assign(item.binaryData.data(),
                                    item.binaryData.size());
        result.file_description_filename = item.title;
        break;
      case WebDragData::Item::StorageTypeFilename:
        // TODO(varunjain): This only works on chromeos. Support win/mac/gtk.
        result.filenames.push_back(ui::FileInfo(
            blink::WebStringToFilePath(item.filenameData),
            blink::WebStringToFilePath(item.displayNameData)));
        break;
      case WebDragData::Item::StorageTypeFileSystemFile: {
        DropData::FileSystemFileInfo info;
        info.url = item.fileSystemURL;
        info.size = item.fileSystemFileSize;
        result.file_system_files.push_back(info);
        break;
      }
    }
  }

  return result;
}

}  // namespace content
