// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompressibleString_h
#define CompressibleString_h

#include "platform/PlatformExport.h"
#include "wtf/RefCounted.h"
#include "wtf/text/Unicode.h"
#include "wtf/text/WTFString.h"

namespace blink {

// TODO(hajimehoshi): Now these classes are in platform/text to use UMA. Move
// them to wtf/text.

class PLATFORM_EXPORT CompressibleStringImpl final : public RefCounted<CompressibleStringImpl> {
    WTF_MAKE_NONCOPYABLE(CompressibleStringImpl);
public:
    static void compressAll();

    CompressibleStringImpl()
        : m_string()
        , m_isCompressed(false)
    {
    }

    explicit CompressibleStringImpl(PassRefPtr<StringImpl>);
    ~CompressibleStringImpl();

    bool isEmpty() const { return originalLength() == 0; }

    bool isCompressed() const { return m_isCompressed; }
    unsigned originalLength() const { return m_string.length(); }
    bool is8Bit() const { return m_string.is8Bit(); }

    unsigned originalContentSizeInBytes() const
    {
        if (is8Bit())
            return originalLength() * sizeof(LChar);
        return originalLength() * sizeof(UChar);
    }

    // TODO(hajimehoshi): Update this once we implement compression.
    unsigned currentSizeInBytes() const
    {
        return originalContentSizeInBytes();
    }

    const String& toString()
    {
        if (UNLIKELY(isCompressed()))
            decompressString();
        return m_string;
    }

    const LChar* characters8()
    {
        return toString().characters8();
    }

    const UChar* characters16()
    {
        return toString().characters16();
    }

    void compressString();
    void decompressString();

private:
    String m_string;
    bool m_isCompressed;
};

class PLATFORM_EXPORT CompressibleString final {
public:
    CompressibleString()
        : m_impl(nullptr)
    {
    }

    CompressibleString(const CompressibleString& rhs)
        : m_impl(rhs.m_impl)
    {
    }

    explicit CompressibleString(PassRefPtr<StringImpl> impl)
        : m_impl(impl ? adoptRef(new CompressibleStringImpl(impl)) : nullptr)
    {
    }

    bool isNull() const { return !m_impl; }
    bool isEmpty() const { return isNull() || m_impl->isEmpty(); }
    unsigned length() const { return m_impl ? m_impl->originalLength() : 0; }

    unsigned currentSizeInBytes() const { return m_impl ? m_impl->currentSizeInBytes() : 0; }

    bool isCompressed() const { return m_impl ? m_impl->isCompressed() : false; }
    bool is8Bit() const { return m_impl ? m_impl->is8Bit() : false; }

    const String& toString() const { return m_impl ? m_impl->toString() : emptyString(); }
    const LChar* characters8() const { return m_impl ? m_impl->characters8() : nullptr; }
    const UChar* characters16() const { return m_impl ? m_impl->characters16() : nullptr; }

    CompressibleStringImpl* impl() const { return m_impl.get(); }

private:
    void compressString() const { m_impl->compressString(); }
    void decompressString() const { m_impl->decompressString(); }

    mutable RefPtr<CompressibleStringImpl> m_impl;
};

} // namespace blink

using blink::CompressibleString;
using blink::CompressibleStringImpl;

#endif
