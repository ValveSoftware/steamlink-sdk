// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is overridden to disable exports and imports in libjingle
// between the libpeerconnection and libjingle_webrtc targets.
// TODO(tommi): Remove when a version of libjingle has been rolled in that
// either removes this header file or offers an easy way to turn this off.

#ifndef TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_
#define TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_

#ifndef NON_EXPORTED_BASE
#define NON_EXPORTED_BASE(code) code
#endif  // NON_EXPORTED_BASE

#ifndef WRME_EXPORT
#define WRME_EXPORT
#endif

#endif  // TALK_MEDIA_WEBRTC_WEBRTCEXPORT_H_
