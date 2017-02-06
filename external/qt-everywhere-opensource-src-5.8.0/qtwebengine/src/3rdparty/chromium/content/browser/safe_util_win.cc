// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <shobjidl.h>
#include <wininet.h>

#include "content/browser/safe_util_win.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "ui/base/win/shell.h"
#include "url/gurl.h"

namespace content {
namespace {

// Sets the Zone Identifier on the file to "Internet" (3). Returns true if the
// function succeeds, false otherwise. A failure is expected on system where
// the Zone Identifier is not supported, like a machine with a FAT32 filesystem.
// This function does not invoke Windows Attachment Execution Services.
//
// |full_path| is the path to the downloaded file.
bool SetInternetZoneIdentifierDirectly(const base::FilePath& full_path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  std::wstring path = full_path.value() + L":Zone.Identifier";
  HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, kShare, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return false;

  static const char kIdentifier[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
  // Don't include trailing null in data written.
  static const DWORD kIdentifierSize = arraysize(kIdentifier) - 1;
  DWORD written = 0;
  BOOL result = WriteFile(file, kIdentifier, kIdentifierSize, &written, NULL);
  BOOL flush_result = FlushFileBuffers(file);
  CloseHandle(file);

  if (!result || !flush_result || written != kIdentifierSize) {
    NOTREACHED();
    return false;
  }

  return true;
}

}  // namespace

HRESULT AVScanFile(const base::FilePath& full_path,
                   const std::string& source_url,
                   const std::string& referrer_url,
                   const GUID& client_guid) {
  base::win::ScopedComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = attachment_services.CreateInstance(CLSID_AttachmentServices);

  if (FAILED(hr)) {
    // The thread must have COM initialized.
    DCHECK_NE(CO_E_NOTINITIALIZED, hr);

    // We don't have Attachment Execution Services, it must be a pre-XP.SP2
    // Windows installation, or the thread does not have COM initialized. Try to
    // set the zone information directly. Failure is not considered an error.
    SetInternetZoneIdentifierDirectly(full_path);
    return hr;
  }

  // Note that it is mandatory to check the return values from here on out. If
  // setting one of the parameters fails, it could leave the object in a state
  // where the final Save() call will also fail.

  if (!IsEqualGUID(client_guid, GUID_NULL)) {
    hr = attachment_services->SetClientGuid(client_guid);
    if (FAILED(hr))
      return hr;
  }

  hr = attachment_services->SetLocalPath(full_path.value().c_str());
  if (FAILED(hr))
    return hr;

  // The source URL could be empty if it was not a valid URL or was not HTTP/S.
  // If so, user "about:internet" as a fallback URL. The latter is known to
  // reliably map to the Internet zone.
  //
  // In addition, URLs that are longer than INTERNET_MAX_URL_LENGTH are also
  // known to cause problems for URLMon. Hence also use "about:internet" in
  // these cases. See http://crbug.com/601538.
  hr = attachment_services->SetSource(
      source_url.empty() || source_url.size() > INTERNET_MAX_URL_LENGTH
          ? L"about:internet"
          : base::UTF8ToWide(source_url).c_str());
  if (FAILED(hr))
    return hr;

  // Only set referrer if one is present and shorter than
  // INTERNET_MAX_URL_LENGTH. Also, the source_url is authoritative for
  // determining the relative danger of |full_path| so we don't consider it an
  // error if we have to skip the |referrer_url|.
  if (!referrer_url.empty() && referrer_url.size() < INTERNET_MAX_URL_LENGTH) {
    hr = attachment_services->SetReferrer(
        base::UTF8ToWide(referrer_url).c_str());
    if (FAILED(hr))
      return false;
  }

  // A failure in the Save() call below could result in the downloaded file
  // being deleted.
  return attachment_services->Save();
}

}  // namespace content
