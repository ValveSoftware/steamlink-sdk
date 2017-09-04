// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_DANGER_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_DANGER_TYPE_H_

namespace content {

// This enum is also used by histograms.  Do not change the ordering or remove
// items.
enum DownloadDangerType {
  // The download is safe.
  DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS = 0,

  // A dangerous file to the system (e.g.: a pdf or extension from
  // places other than gallery).
  DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,

  // Safebrowsing download service shows this URL leads to malicious file
  // download.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,

  // SafeBrowsing download service shows this file content as being malicious.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,

  // The content of this download may be malicious (e.g., extension is exe but
  // SafeBrowsing has not finished checking the content).
  DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,

  // SafeBrowsing download service checked the contents of the download, but
  // didn't have enough data to determine whether it was malicious.
  DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,

  // The download was evaluated to be one of the other types of danger,
  // but the user told us to go ahead anyway.
  DOWNLOAD_DANGER_TYPE_USER_VALIDATED,

  // SafeBrowsing download service checked the contents of the download and
  // didn't have data on this specific file, but the file was served from a host
  // known to serve mostly malicious content.
  DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,

  // Applications and extensions that modify browser and/or computer settings
  DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_DANGER_TYPE_MAX
};

}

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_DANGER_TYPE_H_
