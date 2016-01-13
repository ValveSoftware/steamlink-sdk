// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceLocation_h
#define TraceLocation_h

// This is intentionally similar to base/location.h
// that we could easily replace usage of TraceLocation
// with base::Location after merging into Chromium.

namespace WebCore {

class TraceLocation {
public:
    // Currenetly only store the bits used in Blink, base::Location stores more.
    // These char*s are not copied and must live for the duration of the program.
    TraceLocation(const char* functionName, const char* fileName)
        : m_functionName(functionName)
        , m_fileName(fileName)
    { }

    TraceLocation()
        : m_functionName("unknown")
        , m_fileName("unknown")
    { }

    const char* functionName() const { return m_functionName; }
    const char* fileName() const { return m_fileName; }

private:
    const char* m_functionName;
    const char* m_fileName;
};

#define FROM_HERE WebCore::TraceLocation(__FUNCTION__, __FILE__)

}

#endif // TraceLocation_h
