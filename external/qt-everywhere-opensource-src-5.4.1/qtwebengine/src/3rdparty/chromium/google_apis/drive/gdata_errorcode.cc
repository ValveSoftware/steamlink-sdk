// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_errorcode.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace google_apis {

std::string GDataErrorCodeToString(GDataErrorCode error) {
  switch (error) {
    case HTTP_SUCCESS:
      return"HTTP_SUCCESS";

    case HTTP_CREATED:
      return"HTTP_CREATED";

    case HTTP_NO_CONTENT:
      return"HTTP_NO_CONTENT";

    case HTTP_FOUND:
      return"HTTP_FOUND";

    case HTTP_NOT_MODIFIED:
      return"HTTP_NOT_MODIFIED";

    case HTTP_RESUME_INCOMPLETE:
      return"HTTP_RESUME_INCOMPLETE";

    case HTTP_BAD_REQUEST:
      return"HTTP_BAD_REQUEST";

    case HTTP_UNAUTHORIZED:
      return"HTTP_UNAUTHORIZED";

    case HTTP_FORBIDDEN:
      return"HTTP_FORBIDDEN";

    case HTTP_NOT_FOUND:
      return"HTTP_NOT_FOUND";

    case HTTP_CONFLICT:
      return"HTTP_CONFLICT";

    case HTTP_GONE:
      return "HTTP_GONE";

    case HTTP_LENGTH_REQUIRED:
      return"HTTP_LENGTH_REQUIRED";

    case HTTP_PRECONDITION:
      return"HTTP_PRECONDITION";

    case HTTP_INTERNAL_SERVER_ERROR:
      return"HTTP_INTERNAL_SERVER_ERROR";

    case HTTP_NOT_IMPLEMENTED:
      return "HTTP_NOT_IMPLEMENTED";

    case HTTP_BAD_GATEWAY:
      return"HTTP_BAD_GATEWAY";

    case HTTP_SERVICE_UNAVAILABLE:
      return"HTTP_SERVICE_UNAVAILABLE";

    case GDATA_PARSE_ERROR:
      return"GDATA_PARSE_ERROR";

    case GDATA_FILE_ERROR:
      return"GDATA_FILE_ERROR";

    case GDATA_CANCELLED:
      return"GDATA_CANCELLED";

    case GDATA_OTHER_ERROR:
      return"GDATA_OTHER_ERROR";

    case GDATA_NO_CONNECTION:
      return"GDATA_NO_CONNECTION";

    case GDATA_NOT_READY:
      return"GDATA_NOT_READY";

    case GDATA_NO_SPACE:
      return"GDATA_NO_SPACE";
  }

  return "UNKNOWN_ERROR_" + base::IntToString(error);
}

}  // namespace google_apis
