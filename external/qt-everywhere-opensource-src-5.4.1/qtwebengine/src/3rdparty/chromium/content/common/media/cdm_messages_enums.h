// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_
#define CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_

// Dictates the session type when an EME session is created.
enum CdmHostMsg_CreateSession_ContentType {
  CREATE_SESSION_TYPE_WEBM,
  CREATE_SESSION_TYPE_MP4
};

#endif  // CONTENT_COMMON_MEDIA_CDM_MESSAGES_ENUMS_H_
