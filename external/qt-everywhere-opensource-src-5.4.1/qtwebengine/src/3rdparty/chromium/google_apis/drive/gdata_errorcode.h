// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_GDATA_ERRORCODE_H_
#define GOOGLE_APIS_DRIVE_GDATA_ERRORCODE_H_

#include <string>

namespace google_apis {

// HTTP errors that can be returned by GData service.
enum GDataErrorCode {
  HTTP_SUCCESS               = 200,
  HTTP_CREATED               = 201,
  HTTP_NO_CONTENT            = 204,
  HTTP_FOUND                 = 302,
  HTTP_NOT_MODIFIED          = 304,
  HTTP_RESUME_INCOMPLETE     = 308,
  HTTP_BAD_REQUEST           = 400,
  HTTP_UNAUTHORIZED          = 401,
  HTTP_FORBIDDEN             = 403,
  HTTP_NOT_FOUND             = 404,
  HTTP_CONFLICT              = 409,
  HTTP_GONE                  = 410,
  HTTP_LENGTH_REQUIRED       = 411,
  HTTP_PRECONDITION          = 412,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED       = 501,
  HTTP_BAD_GATEWAY           = 502,
  HTTP_SERVICE_UNAVAILABLE   = 503,
  GDATA_PARSE_ERROR          = -100,
  GDATA_FILE_ERROR           = -101,
  GDATA_CANCELLED            = -102,
  GDATA_OTHER_ERROR          = -103,
  GDATA_NO_CONNECTION        = -104,
  GDATA_NOT_READY            = -105,
  GDATA_NO_SPACE             = -106,
};

// Returns a string representation of GDataErrorCode.
std::string GDataErrorCodeToString(GDataErrorCode error);

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_ERRORCODE_H_
