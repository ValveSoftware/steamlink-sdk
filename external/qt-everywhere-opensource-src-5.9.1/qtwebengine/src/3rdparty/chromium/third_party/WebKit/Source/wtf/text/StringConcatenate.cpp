/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "wtf/text/StringConcatenate.h"

#include "wtf/text/StringImpl.h"

// This macro is helpful for testing how many intermediate Strings are created
// while evaluating an expression containing operator+.
#ifndef WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING
#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() ((void)0)
#endif

void WTF::StringTypeAdapter<char*>::writeTo(LChar* destination) const {
  for (unsigned i = 0; i < m_length; ++i)
    destination[i] = static_cast<LChar>(m_buffer[i]);
}

void WTF::StringTypeAdapter<char*>::writeTo(UChar* destination) const {
  for (unsigned i = 0; i < m_length; ++i) {
    unsigned char c = m_buffer[i];
    destination[i] = c;
  }
}

WTF::StringTypeAdapter<LChar*>::StringTypeAdapter(LChar* buffer)
    : m_buffer(buffer), m_length(strlen(reinterpret_cast<char*>(buffer))) {}

void WTF::StringTypeAdapter<LChar*>::writeTo(LChar* destination) const {
  memcpy(destination, m_buffer, m_length * sizeof(LChar));
}

void WTF::StringTypeAdapter<LChar*>::writeTo(UChar* destination) const {
  StringImpl::copyChars(destination, m_buffer, m_length);
}

WTF::StringTypeAdapter<const UChar*>::StringTypeAdapter(const UChar* buffer)
    : m_buffer(buffer), m_length(lengthOfNullTerminatedString(buffer)) {}

void WTF::StringTypeAdapter<const UChar*>::writeTo(UChar* destination) const {
  memcpy(destination, m_buffer, m_length * sizeof(UChar));
}

WTF::StringTypeAdapter<const char*>::StringTypeAdapter(const char* buffer)
    : m_buffer(buffer), m_length(strlen(buffer)) {}

void WTF::StringTypeAdapter<const char*>::writeTo(LChar* destination) const {
  memcpy(destination, m_buffer, static_cast<size_t>(m_length) * sizeof(LChar));
}

void WTF::StringTypeAdapter<const char*>::writeTo(UChar* destination) const {
  for (unsigned i = 0; i < m_length; ++i) {
    unsigned char c = m_buffer[i];
    destination[i] = c;
  }
}

WTF::StringTypeAdapter<const LChar*>::StringTypeAdapter(const LChar* buffer)
    : m_buffer(buffer),
      m_length(strlen(reinterpret_cast<const char*>(buffer))) {}

void WTF::StringTypeAdapter<const LChar*>::writeTo(LChar* destination) const {
  memcpy(destination, m_buffer, static_cast<size_t>(m_length) * sizeof(LChar));
}

void WTF::StringTypeAdapter<const LChar*>::writeTo(UChar* destination) const {
  StringImpl::copyChars(destination, m_buffer, m_length);
}

void WTF::StringTypeAdapter<StringView>::writeTo(LChar* destination) const {
  DCHECK(is8Bit());
  StringImpl::copyChars(destination, m_view.characters8(), m_view.length());
  WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
}

void WTF::StringTypeAdapter<StringView>::writeTo(UChar* destination) const {
  if (is8Bit())
    StringImpl::copyChars(destination, m_view.characters8(), m_view.length());
  else
    StringImpl::copyChars(destination, m_view.characters16(), m_view.length());
  WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
}
