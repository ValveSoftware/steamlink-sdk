// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintSize_h
#define PaintSize_h

namespace blink {

class PaintSize : public GarbageCollectedFinalized<PaintSize>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(PaintSize);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PaintSize* create(IntSize size)
    {
        return new PaintSize(size);
    }
    virtual ~PaintSize() {}

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }

    DEFINE_INLINE_TRACE() { }
private:
    explicit PaintSize(IntSize size) : m_size(size) { }

    IntSize m_size;
};

} // namespace blink

#endif // PaintSize_h
