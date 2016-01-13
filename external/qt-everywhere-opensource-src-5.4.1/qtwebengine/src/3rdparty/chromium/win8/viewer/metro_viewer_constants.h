// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_VIEWER_METRO_VIEWER_CONSTANTS_H_
#define WIN8_VIEWER_METRO_VIEWER_CONSTANTS_H_

namespace win8 {

// The name of the IPC channel between the browser process and the metro viewer
// process.
extern const char kMetroViewerIPCChannelName[];

// Tells the viewer process to simply connect back without needing to launch a
// browser process itself.
extern const wchar_t kMetroViewerConnectVerb[];

}  // namespace win8

#endif  // WIN8_VIEWER_METRO_VIEWER_CONSTANTS_H_
