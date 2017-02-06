// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaArtwork_h
#define WebMediaArtwork_h

#include "public/platform/WebString.h"

namespace blink {

// Representation of MediaArtwork interface to the content layer.
struct WebMediaArtwork {
    WebString src;
    WebString sizes;
    WebString type;
};

}

#endif // WebMediaArtwork_h
