/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef StringOperators_h
#define StringOperators_h

#include "wtf/Allocator.h"
#include "wtf/text/StringConcatenate.h"

namespace WTF {

template <typename StringType1, typename StringType2>
class StringAppend final {
  STACK_ALLOCATED();

 public:
  StringAppend(StringType1 string1, StringType2 string2);

  operator String() const;
  operator AtomicString() const;

  unsigned length() const;
  bool is8Bit() const;

  void writeTo(LChar* destination) const;
  void writeTo(UChar* destination) const;

 private:
  const StringType1 m_string1;
  const StringType2 m_string2;
};

template <typename StringType1, typename StringType2>
StringAppend<StringType1, StringType2>::StringAppend(StringType1 string1,
                                                     StringType2 string2)
    : m_string1(string1), m_string2(string2) {}

template <typename StringType1, typename StringType2>
StringAppend<StringType1, StringType2>::operator String() const {
  if (is8Bit()) {
    LChar* buffer;
    RefPtr<StringImpl> result =
        StringImpl::createUninitialized(length(), buffer);
    writeTo(buffer);
    return result.release();
  }
  UChar* buffer;
  RefPtr<StringImpl> result = StringImpl::createUninitialized(length(), buffer);
  writeTo(buffer);
  return result.release();
}

template <typename StringType1, typename StringType2>
StringAppend<StringType1, StringType2>::operator AtomicString() const {
  return AtomicString(static_cast<String>(*this));
}

template <typename StringType1, typename StringType2>
bool StringAppend<StringType1, StringType2>::is8Bit() const {
  StringTypeAdapter<StringType1> adapter1(m_string1);
  StringTypeAdapter<StringType2> adapter2(m_string2);
  return adapter1.is8Bit() && adapter2.is8Bit();
}

template <typename StringType1, typename StringType2>
void StringAppend<StringType1, StringType2>::writeTo(LChar* destination) const {
  ASSERT(is8Bit());
  StringTypeAdapter<StringType1> adapter1(m_string1);
  StringTypeAdapter<StringType2> adapter2(m_string2);
  adapter1.writeTo(destination);
  adapter2.writeTo(destination + adapter1.length());
}

template <typename StringType1, typename StringType2>
void StringAppend<StringType1, StringType2>::writeTo(UChar* destination) const {
  StringTypeAdapter<StringType1> adapter1(m_string1);
  StringTypeAdapter<StringType2> adapter2(m_string2);
  adapter1.writeTo(destination);
  adapter2.writeTo(destination + adapter1.length());
}

template <typename StringType1, typename StringType2>
unsigned StringAppend<StringType1, StringType2>::length() const {
  StringTypeAdapter<StringType1> adapter1(m_string1);
  StringTypeAdapter<StringType2> adapter2(m_string2);
  unsigned total = adapter1.length() + adapter2.length();
  // Guard against overflow.
  RELEASE_ASSERT(total >= adapter1.length() && total >= adapter2.length());
  return total;
}

template <typename StringType1, typename StringType2>
class StringTypeAdapter<StringAppend<StringType1, StringType2>> {
  STACK_ALLOCATED();

 public:
  StringTypeAdapter<StringAppend<StringType1, StringType2>>(
      const StringAppend<StringType1, StringType2>& buffer)
      : m_buffer(buffer) {}

  unsigned length() const { return m_buffer.length(); }
  bool is8Bit() const { return m_buffer.is8Bit(); }

  void writeTo(LChar* destination) const { m_buffer.writeTo(destination); }
  void writeTo(UChar* destination) const { m_buffer.writeTo(destination); }

 private:
  const StringAppend<StringType1, StringType2>& m_buffer;
};

inline StringAppend<const char*, String> operator+(const char* string1,
                                                   const String& string2) {
  return StringAppend<const char*, String>(string1, string2);
}

inline StringAppend<const char*, AtomicString> operator+(
    const char* string1,
    const AtomicString& string2) {
  return StringAppend<const char*, AtomicString>(string1, string2);
}

inline StringAppend<const char*, StringView> operator+(
    const char* string1,
    const StringView& string2) {
  return StringAppend<const char*, StringView>(string1, string2);
}

template <typename U, typename V>
inline StringAppend<const char*, StringAppend<U, V>> operator+(
    const char* string1,
    const StringAppend<U, V>& string2) {
  return StringAppend<const char*, StringAppend<U, V>>(string1, string2);
}

inline StringAppend<const UChar*, String> operator+(const UChar* string1,
                                                    const String& string2) {
  return StringAppend<const UChar*, String>(string1, string2);
}

inline StringAppend<const UChar*, AtomicString> operator+(
    const UChar* string1,
    const AtomicString& string2) {
  return StringAppend<const UChar*, AtomicString>(string1, string2);
}

inline StringAppend<const UChar*, StringView> operator+(
    const UChar* string1,
    const StringView& string2) {
  return StringAppend<const UChar*, StringView>(string1, string2);
}

template <typename U, typename V>
inline StringAppend<const UChar*, StringAppend<U, V>> operator+(
    const UChar* string1,
    const StringAppend<U, V>& string2) {
  return StringAppend<const UChar*, StringAppend<U, V>>(string1, string2);
}

template <typename T>
StringAppend<String, T> operator+(const String& string1, T string2) {
  return StringAppend<String, T>(string1, string2);
}

template <typename T>
StringAppend<AtomicString, T> operator+(const AtomicString& string1,
                                        T string2) {
  return StringAppend<AtomicString, T>(string1, string2);
}

template <typename T>
StringAppend<StringView, T> operator+(const StringView& string1, T string2) {
  return StringAppend<StringView, T>(string1, string2);
}

template <typename U, typename V, typename W>
StringAppend<StringAppend<U, V>, W> operator+(const StringAppend<U, V>& string1,
                                              W string2) {
  return StringAppend<StringAppend<U, V>, W>(string1, string2);
}

}  // namespace WTF

#endif  // StringOperators_h
