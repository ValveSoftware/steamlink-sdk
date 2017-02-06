/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef JSONValues_h
#define JSONValues_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class JSONValue;

} // namespace blink

namespace blink {

class JSONArray;
class JSONObject;

class PLATFORM_EXPORT JSONValue : public RefCounted<JSONValue> {
public:
    static const int maxDepth = 1000;

    virtual ~JSONValue() { }

    static PassRefPtr<JSONValue> null()
    {
        return adoptRef(new JSONValue());
    }

    typedef enum {
        TypeNull = 0,
        TypeBoolean,
        TypeNumber,
        TypeString,
        TypeObject,
        TypeArray
    } Type;

    Type getType() const { return m_type; }

    bool isNull() const { return m_type == TypeNull; }

    virtual bool asBoolean(bool* output) const;
    virtual bool asNumber(double* output) const;
    virtual bool asNumber(long* output) const;
    virtual bool asNumber(int* output) const;
    virtual bool asNumber(unsigned long* output) const;
    virtual bool asNumber(unsigned* output) const;
    virtual bool asString(String* output) const;

    String toJSONString() const;
    String toPrettyJSONString() const;
    virtual void writeJSON(StringBuilder* output) const;
    virtual void prettyWriteJSON(StringBuilder* output) const;

    static String quoteString(const String&);

protected:
    JSONValue() : m_type(TypeNull) { }
    explicit JSONValue(Type type) : m_type(type) { }
    virtual void prettyWriteJSONInternal(StringBuilder* output, int depth) const;

private:
    friend class JSONObject;
    friend class JSONArray;

    Type m_type;
};

class PLATFORM_EXPORT JSONBasicValue : public JSONValue {
public:

    static PassRefPtr<JSONBasicValue> create(bool value)
    {
        return adoptRef(new JSONBasicValue(value));
    }

    static PassRefPtr<JSONBasicValue> create(int value)
    {
        return adoptRef(new JSONBasicValue(value));
    }

    static PassRefPtr<JSONBasicValue> create(double value)
    {
        return adoptRef(new JSONBasicValue(value));
    }

    bool asBoolean(bool* output) const override;
    bool asNumber(double* output) const override;
    bool asNumber(long* output) const override;
    bool asNumber(int* output) const override;
    bool asNumber(unsigned long* output) const override;
    bool asNumber(unsigned* output) const override;

    void writeJSON(StringBuilder* output) const override;

private:
    explicit JSONBasicValue(bool value) : JSONValue(TypeBoolean), m_boolValue(value) { }
    explicit JSONBasicValue(int value) : JSONValue(TypeNumber), m_doubleValue((double)value) { }
    explicit JSONBasicValue(double value) : JSONValue(TypeNumber), m_doubleValue(value) { }

    union {
        bool m_boolValue;
        double m_doubleValue;
    };
};

class PLATFORM_EXPORT JSONString : public JSONValue {
public:
    static PassRefPtr<JSONString> create(const String& value)
    {
        return adoptRef(new JSONString(value));
    }

    static PassRefPtr<JSONString> create(const char* value)
    {
        return adoptRef(new JSONString(value));
    }

    bool asString(String* output) const override;

    void writeJSON(StringBuilder* output) const override;

private:
    explicit JSONString(const String& value) : JSONValue(TypeString), m_stringValue(value) { }
    explicit JSONString(const char* value) : JSONValue(TypeString), m_stringValue(value) { }

    String m_stringValue;
};

class PLATFORM_EXPORT JSONObject : public JSONValue {
private:
    typedef HashMap<String, RefPtr<JSONValue>> Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

    static PassRefPtr<JSONObject> create()
    {
        return adoptRef(new JSONObject());
    }

    static PassRefPtr<JSONObject> cast(PassRefPtr<JSONValue> value)
    {
        if (!value || value->getType() != TypeObject)
            return nullptr;
        return adoptRef(static_cast<JSONObject*>(value.leakRef()));
    }

    void writeJSON(StringBuilder* output) const override;

    int size() const { return m_data.size(); }

    void setBoolean(const String& name, bool);
    void setNumber(const String& name, double);
    void setString(const String& name, const String&);
    void setValue(const String& name, PassRefPtr<JSONValue>);
    void setObject(const String& name, PassRefPtr<JSONObject>);
    void setArray(const String& name, PassRefPtr<JSONArray>);

    iterator find(const String& name);
    const_iterator find(const String& name) const;
    bool getBoolean(const String& name, bool* output) const;
    template<class T> bool getNumber(const String& name, T* output) const
    {
        RefPtr<JSONValue> value = get(name);
        if (!value)
            return false;
        return value->asNumber(output);
    }
    bool getString(const String& name, String* output) const;
    PassRefPtr<JSONObject> getObject(const String& name) const;
    PassRefPtr<JSONArray> getArray(const String& name) const;
    PassRefPtr<JSONValue> get(const String& name) const;

    bool booleanProperty(const String& name, bool defaultValue) const;

    void remove(const String& name);

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    ~JSONObject() override;

protected:
    void prettyWriteJSONInternal(StringBuilder* output, int depth) const override;

private:
    JSONObject();

    Dictionary m_data;
    Vector<String> m_order;
};

class PLATFORM_EXPORT JSONArray : public JSONValue {
public:
    typedef Vector<RefPtr<JSONValue>>::iterator iterator;
    typedef Vector<RefPtr<JSONValue>>::const_iterator const_iterator;

    static PassRefPtr<JSONArray> create()
    {
        return adoptRef(new JSONArray());
    }

    static PassRefPtr<JSONArray> cast(PassRefPtr<JSONValue> value)
    {
        if (!value || value->getType() != TypeArray)
            return nullptr;
        return adoptRef(static_cast<JSONArray*>(value.leakRef()));
    }

    ~JSONArray() override;

    void writeJSON(StringBuilder* output) const override;

    void pushBoolean(bool);
    void pushInt(int);
    void pushNumber(double);
    void pushString(const String&);
    void pushValue(PassRefPtr<JSONValue>);
    void pushObject(PassRefPtr<JSONObject>);
    void pushArray(PassRefPtr<JSONArray>);

    PassRefPtr<JSONValue> get(size_t index);
    unsigned length() const { return m_data.size(); }

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

protected:
    void prettyWriteJSONInternal(StringBuilder* output, int depth) const override;

private:
    JSONArray();
    Vector<RefPtr<JSONValue>> m_data;
};

PLATFORM_EXPORT void escapeStringForJSON(const String&, StringBuilder*);
void doubleQuoteStringForJSON(const String&, StringBuilder*);

} // namespace blink

#endif // JSONValues_h
