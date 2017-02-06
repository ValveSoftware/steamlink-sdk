// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_TYPE_H_
#define CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_TYPE_H_

enum MediaMessageType {
  PaddingMediaMsg = 1,
  FrameMediaMsg,
  AudioConfigMediaMsg,
  VideoConfigMediaMsg,
};

#endif
