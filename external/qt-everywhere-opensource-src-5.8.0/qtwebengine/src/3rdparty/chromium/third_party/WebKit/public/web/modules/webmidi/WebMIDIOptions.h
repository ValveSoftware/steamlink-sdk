// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMIDIOptions_h
#define WebMIDIOptions_h

namespace blink {

struct WebMIDIOptions {
    enum class SysexPermission { WithSysex, WithoutSysex };

    explicit WebMIDIOptions(SysexPermission sysex)
        : sysex(sysex)
    {
    }

    const SysexPermission sysex;
    // TODO(crbug.com/502127): Add bool software to follow the latest spec.
};

} // namespace blink

#endif // WebMIDIOptions_h
