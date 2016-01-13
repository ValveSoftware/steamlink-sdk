/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TypeConversions_h
#define TypeConversions_h

#include "wtf/FastMalloc.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class TypeConversions : public RefCountedWillBeGarbageCollectedFinalized<TypeConversions> {
public:
    static PassRefPtrWillBeRawPtr<TypeConversions> create()
    {
        return adoptRefWillBeNoop(new TypeConversions());
    }

    long testLong() { return m_long; }
    void setTestLong(long value) { m_long = value; }
    unsigned long testUnsignedLong() { return m_unsignedLong; }
    void setTestUnsignedLong(unsigned long value) { m_unsignedLong = value; }

    long long testLongLong() { return m_longLong; }
    void setTestLongLong(long long value) { m_longLong = value; }
    unsigned long long testUnsignedLongLong() { return m_unsignedLongLong; }
    void setTestUnsignedLongLong(unsigned long long value) { m_unsignedLongLong = value; }

    int8_t testByte() { return m_byte; }
    void setTestByte(int8_t value) { m_byte = value; }
    uint8_t testOctet() { return m_octet; }
    void setTestOctet(uint8_t value) { m_octet = value; }

    int16_t testShort() { return m_short; }
    void setTestShort(int16_t value) { m_short = value; }
    uint16_t testUnsignedShort() { return m_unsignedShort; }
    void setTestUnsignedShort(uint16_t value) { m_unsignedShort = value; }

    const String& testByteString() const { return m_byteString; }
    void setTestByteString(const String& value) { m_byteString = value; }

    const String& testScalarValueString() const { return m_scalarValueString; }
    void setTestScalarValueString(const String& value) { m_scalarValueString = value; }

    void trace(Visitor*) { }

private:
    TypeConversions()
    {
    }

    long m_long;
    unsigned long m_unsignedLong;
    long long m_longLong;
    unsigned long long m_unsignedLongLong;
    int8_t m_byte;
    uint8_t m_octet;
    int16_t m_short;
    uint16_t m_unsignedShort;
    String m_byteString;
    String m_scalarValueString;
};

} // namespace WebCore

#endif
