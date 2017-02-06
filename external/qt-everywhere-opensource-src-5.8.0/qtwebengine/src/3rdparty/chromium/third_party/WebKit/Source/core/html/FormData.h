/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FormData_h
#define FormData_h

#include "bindings/core/v8/FileOrUSVString.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Forward.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

class Blob;
class HTMLFormElement;

// Typedef from FormData.idl:
typedef FileOrUSVString FormDataEntryValue;

class CORE_EXPORT FormData final : public GarbageCollected<FormData>, public ScriptWrappable, public PairIterable<String, FormDataEntryValue> {
    DEFINE_WRAPPERTYPEINFO();

public:
    static FormData* create(HTMLFormElement* form = 0)
    {
        return new FormData(form);
    }

    static FormData* create(const WTF::TextEncoding& encoding)
    {
        return new FormData(encoding);
    }
    DECLARE_TRACE();

    // FormData IDL interface.
    void append(const String& name, const String& value);
    void append(ExecutionContext*, const String& name, Blob*, const String& filename = String());
    void deleteEntry(const String& name);
    void get(const String& name, FormDataEntryValue& result);
    HeapVector<FormDataEntryValue> getAll(const String& name);
    bool has(const String& name);
    void set(const String& name, const String& value);
    void set(const String& name, Blob*, const String& filename = String());

    // Internal functions.

    const WTF::TextEncoding& encoding() const { return m_encoding; }
    class Entry;
    const HeapVector<Member<const Entry>>& entries() const { return m_entries; }
    size_t size() const { return m_entries.size(); }
    void append(const String& name, int value);
    void append(const String& name, Blob*, const String& filename = String());
    String decode(const CString& data) const;

    PassRefPtr<EncodedFormData> encodeFormData(EncodedFormData::EncodingType = EncodedFormData::FormURLEncoded);
    PassRefPtr<EncodedFormData> encodeMultiPartFormData();

private:
    explicit FormData(const WTF::TextEncoding&);
    explicit FormData(HTMLFormElement*);
    void setEntry(const Entry*);
    CString encodeAndNormalize(const String& key) const;
    IterationSource* startIteration(ScriptState*, ExceptionState&) override;

    WTF::TextEncoding m_encoding;
    // Entry pointers in m_entries never be nullptr.
    HeapVector<Member<const Entry>> m_entries;
};

// Represents entry, which is a pair of a name and a value.
// https://xhr.spec.whatwg.org/#concept-formdata-entry
// Entry objects are immutable.
class FormData::Entry : public GarbageCollectedFinalized<FormData::Entry> {
public:
    Entry(const CString& name, const CString& value) : m_name(name), m_value(value) { }
    Entry(const CString& name, Blob* blob, const String& filename) : m_name(name), m_blob(blob), m_filename(filename) { }
    DECLARE_TRACE();

    bool isString() const { return !m_blob; }
    bool isFile() const { return m_blob; }
    const CString& name() const { return m_name; }
    const CString& value() const { return m_value; }
    Blob* blob() const { return m_blob.get(); }
    File* file() const;
    const String& filename() const { return m_filename; }

private:
    const CString m_name;
    const CString m_value;
    const Member<Blob> m_blob;
    const String m_filename;
};

} // namespace blink

#endif // FormData_h
