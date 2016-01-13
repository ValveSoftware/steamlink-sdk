// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Content application.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_

#include <stddef.h>         // For size_t

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

// The name of the directory under BrowserContext::GetPath where the AppCache is
// put.
CONTENT_EXPORT extern const base::FilePath::CharType kAppCacheDirname[];
// The name of the directory under BrowserContext::GetPath where Pepper plugin
// data is put.
CONTENT_EXPORT extern const base::FilePath::CharType kPepperDataDirname[];

// The MIME type used for the browser plugin.
CONTENT_EXPORT extern const char kBrowserPluginMimeType[];

CONTENT_EXPORT extern const char kFlashPluginName[];
CONTENT_EXPORT extern const char kFlashPluginSwfMimeType[];
CONTENT_EXPORT extern const char kFlashPluginSwfExtension[];
CONTENT_EXPORT extern const char kFlashPluginSwfDescription[];
CONTENT_EXPORT extern const char kFlashPluginSplMimeType[];
CONTENT_EXPORT extern const char kFlashPluginSplExtension[];
CONTENT_EXPORT extern const char kFlashPluginSplDescription[];

CONTENT_EXPORT extern const size_t kMaxRendererProcessCount;

// The maximum number of session history entries per tab.
extern const int kMaxSessionHistoryEntries;

// The maximum number of characters of the document's title that we're willing
// to accept in the browser process.
extern const size_t kMaxTitleChars;

CONTENT_EXPORT extern const size_t kMaxURLDisplayChars;

extern const char kStatsFilename[];
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

// Most sequence numbers are used by a renderer when responding to a browser
// request for histogram data.  This reserved number is used when a renderer
// sends an unprovoked update, such as after a page has been loaded.  Using
// this reserved constant avoids any chance of confusion with a response having
// a browser-supplied sequence number.
CONTENT_EXPORT extern const int kHistogramSynchronizerReservedSequenceNumber;

// Shared constants for the flash hardware video decode field trial.
CONTENT_EXPORT extern const char kFlashHwVideoDecodeFieldTrialName[];
CONTENT_EXPORT extern const char kFlashHwVideoDecodeFieldTrialEnabledName[];

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
