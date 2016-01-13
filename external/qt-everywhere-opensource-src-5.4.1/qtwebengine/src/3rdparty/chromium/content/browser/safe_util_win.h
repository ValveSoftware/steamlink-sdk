// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SAFE_UTIL_WIN_H_
#define CONTENT_COMMON_SAFE_UTIL_WIN_H_

#include <string>
#include <windows.h>

class GURL;

namespace base {
class FilePath;
}

namespace content {

// Invokes IAttachmentExecute::Save to validate the downloaded file. The call
// may scan the file for viruses and if necessary, annotate it with evidence. As
// a result of the validation, the file may be deleted.  See:
// http://msdn.microsoft.com/en-us/bb776299
//
// If Attachment Execution Services is unavailable, then this function will
// attempt to manually annotate the file with security zone information. A
// failure code will be returned in this case even if the file is sucessfully
// annotated.
//
// IAE::Save() will delete the file if it was found to be blocked by local
// security policy or if it was found to be infected. The call may also delete
// the file due to other failures (http://crbug.com/153212). A failure code will
// be returned in these cases.
//
// Typical return values:
//   S_OK   : The file was okay. If any viruses were found, they were cleaned.
//   E_FAIL : Virus infected.
//   INET_E_SECURITY_PROBLEM : The file was blocked due to security policy.
//
// Any other return value indicates an unexpected error during the scan.
//
// |full_path| : is the path to the downloaded file. This should be the final
//               path of the download. Must be present.
// |source_url|: the source URL for the download. If empty, the source will
//               not be set.
// |client_guid|: the GUID to be set in the IAttachmentExecute client slot.
//                Used to identify the app to the system AV function.
//                If GUID_NULL is passed, no client GUID is set.
HRESULT AVScanFile(const base::FilePath& full_path,
                   const std::string& source_url,
                   const GUID& client_guid);
}  // namespace content

#endif  // CONTENT_COMMON_SAFE_UTIL_WIN_H_
