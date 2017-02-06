// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_
#define CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_

// The Initialization Data Type when a request is generated.
// TODO(ddorwin): Replace this with a generic constant. See crbug.com/417440#c9.
enum CdmHostMsg_CreateSession_InitDataType {
  INIT_DATA_TYPE_WEBM,
  INIT_DATA_TYPE_CENC,
  INIT_DATA_TYPE_MAX = INIT_DATA_TYPE_CENC
};

#endif  // CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_
