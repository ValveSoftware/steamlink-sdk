// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/srcobject/HTMLMediaElementSrcObject.h"

#include "core/html/HTMLMediaElement.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/mediastream/MediaStreamDescriptor.h"

namespace blink {

// static
MediaStream* HTMLMediaElementSrcObject::srcObject(HTMLMediaElement& element)
{
    MediaStreamDescriptor* descriptor = element.getSrcObject();
    if (descriptor) {
        MediaStream* stream = toMediaStream(descriptor);
        return stream;
    }

    return nullptr;
}

// static
void HTMLMediaElementSrcObject::setSrcObject(HTMLMediaElement& element, MediaStream* mediaStream)
{
    if (!mediaStream) {
        element.setSrcObject(nullptr);
        return;
    }
    element.setSrcObject(mediaStream->descriptor());
}

} // namespace blink
