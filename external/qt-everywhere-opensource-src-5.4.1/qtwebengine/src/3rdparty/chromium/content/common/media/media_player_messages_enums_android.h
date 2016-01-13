// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_ENUMS_ANDROID_H_
#define CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_ENUMS_ANDROID_H_

// Dictates which type of media playback is being initialized.
enum MediaPlayerHostMsg_Initialize_Type {
  MEDIA_PLAYER_TYPE_URL,
  MEDIA_PLAYER_TYPE_MEDIA_SOURCE,
};

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_ENUMS_ANDROID_H_
