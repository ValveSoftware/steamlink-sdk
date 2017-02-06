// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePath_h
#define StylePath_h

#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class CSSValue;
class Path;
class SVGPathByteStream;

class StylePath : public RefCounted<StylePath> {
public:
    static PassRefPtr<StylePath> create(std::unique_ptr<SVGPathByteStream>);
    ~StylePath();

    static StylePath* emptyPath();

    const Path& path() const;
    float length() const;
    bool isClosed() const;

    const SVGPathByteStream& byteStream() const { return *m_byteStream; }

    CSSValue* computedCSSValue() const;

    bool operator==(const StylePath&) const;

private:
    explicit StylePath(std::unique_ptr<SVGPathByteStream>);

    std::unique_ptr<SVGPathByteStream> m_byteStream;
    mutable std::unique_ptr<Path> m_path;
    mutable float m_pathLength;
};

} // namespace blink

#endif // StylePath_h
