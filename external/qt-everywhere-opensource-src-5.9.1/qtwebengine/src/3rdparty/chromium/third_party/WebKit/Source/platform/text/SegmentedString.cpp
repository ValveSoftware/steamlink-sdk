/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "platform/text/SegmentedString.h"

namespace blink {

unsigned SegmentedString::length() const {
  unsigned length = m_currentString.length();
  if (isComposite()) {
    Deque<SegmentedSubstring>::const_iterator it = m_substrings.begin();
    Deque<SegmentedSubstring>::const_iterator e = m_substrings.end();
    for (; it != e; ++it)
      length += it->length();
  }
  return length;
}

void SegmentedString::setExcludeLineNumbers() {
  m_currentString.setExcludeLineNumbers();
  if (isComposite()) {
    Deque<SegmentedSubstring>::iterator it = m_substrings.begin();
    Deque<SegmentedSubstring>::iterator e = m_substrings.end();
    for (; it != e; ++it)
      it->setExcludeLineNumbers();
  }
}

void SegmentedString::clear() {
  m_currentChar = 0;
  m_currentString.clear();
  m_numberOfCharactersConsumedPriorToCurrentString = 0;
  m_numberOfCharactersConsumedPriorToCurrentLine = 0;
  m_currentLine = 0;
  m_substrings.clear();
  m_closed = false;
  m_empty = true;
  m_fastPathFlags = NoFastPath;
  m_advanceFunc = &SegmentedString::advanceEmpty;
  m_advanceAndUpdateLineNumberFunc = &SegmentedString::advanceEmpty;
}

void SegmentedString::append(const SegmentedSubstring& s) {
  ASSERT(!m_closed);
  if (!s.length())
    return;

  if (!m_currentString.length()) {
    m_numberOfCharactersConsumedPriorToCurrentString +=
        m_currentString.numberOfCharactersConsumed();
    m_currentString = s;
    updateAdvanceFunctionPointers();
  } else {
    m_substrings.append(s);
  }
  m_empty = false;
}

void SegmentedString::push(UChar c) {
  ASSERT(c);

  // pushIfPossible attempts to rewind the pointer in the SegmentedSubstring,
  // however it will fail if the SegmentedSubstring is empty, or
  // when we prepended some text while consuming a SegmentedSubstring by
  // document.write().
  if (m_currentString.pushIfPossible(c)) {
    m_currentChar = c;
    return;
  }

  prepend(SegmentedString(String(&c, 1)), PrependType::Unconsume);
}

void SegmentedString::prepend(const SegmentedSubstring& s, PrependType type) {
  ASSERT(!s.numberOfCharactersConsumed());
  if (!s.length())
    return;

  // FIXME: We're also ASSERTing that s is a fresh SegmentedSubstring.
  //        The assumption is sufficient for our current use, but we might
  //        need to handle the more elaborate cases in the future.
  m_numberOfCharactersConsumedPriorToCurrentString +=
      m_currentString.numberOfCharactersConsumed();
  if (type == PrependType::Unconsume)
    m_numberOfCharactersConsumedPriorToCurrentString -= s.length();
  if (!m_currentString.length()) {
    m_currentString = s;
    updateAdvanceFunctionPointers();
  } else {
    // Shift our m_currentString into our list.
    m_substrings.prepend(m_currentString);
    m_currentString = s;
    updateAdvanceFunctionPointers();
  }
  m_empty = false;
}

void SegmentedString::close() {
  // Closing a stream twice is likely a coding mistake.
  ASSERT(!m_closed);
  m_closed = true;
}

void SegmentedString::append(const SegmentedString& s) {
  ASSERT(!m_closed);

  append(s.m_currentString);
  if (s.isComposite()) {
    Deque<SegmentedSubstring>::const_iterator it = s.m_substrings.begin();
    Deque<SegmentedSubstring>::const_iterator e = s.m_substrings.end();
    for (; it != e; ++it)
      append(*it);
  }
  m_currentChar =
      m_currentString.length() ? m_currentString.getCurrentChar() : 0;
}

void SegmentedString::prepend(const SegmentedString& s, PrependType type) {
  if (s.isComposite()) {
    Deque<SegmentedSubstring>::const_reverse_iterator it =
        s.m_substrings.rbegin();
    Deque<SegmentedSubstring>::const_reverse_iterator e = s.m_substrings.rend();
    for (; it != e; ++it)
      prepend(*it, type);
  }
  prepend(s.m_currentString, type);
  m_currentChar =
      m_currentString.length() ? m_currentString.getCurrentChar() : 0;
}

void SegmentedString::advanceSubstring() {
  if (isComposite()) {
    m_numberOfCharactersConsumedPriorToCurrentString +=
        m_currentString.numberOfCharactersConsumed();
    m_currentString = m_substrings.takeFirst();
    // If we've previously consumed some characters of the non-current
    // string, we now account for those characters as part of the current
    // string, not as part of "prior to current string."
    m_numberOfCharactersConsumedPriorToCurrentString -=
        m_currentString.numberOfCharactersConsumed();
    updateAdvanceFunctionPointers();
  } else {
    m_currentString.clear();
    m_empty = true;
    m_fastPathFlags = NoFastPath;
    m_advanceFunc = &SegmentedString::advanceEmpty;
    m_advanceAndUpdateLineNumberFunc = &SegmentedString::advanceEmpty;
  }
}

String SegmentedString::toString() const {
  StringBuilder result;
  m_currentString.appendTo(result);
  if (isComposite()) {
    Deque<SegmentedSubstring>::const_iterator it = m_substrings.begin();
    Deque<SegmentedSubstring>::const_iterator e = m_substrings.end();
    for (; it != e; ++it)
      it->appendTo(result);
  }
  return result.toString();
}

void SegmentedString::advance(unsigned count, UChar* consumedCharacters) {
  SECURITY_DCHECK(count <= length());
  for (unsigned i = 0; i < count; ++i) {
    consumedCharacters[i] = currentChar();
    advance();
  }
}

void SegmentedString::advance8() {
  decrementAndCheckLength();
  m_currentChar = m_currentString.incrementAndGetCurrentChar8();
}

void SegmentedString::advance16() {
  decrementAndCheckLength();
  m_currentChar = m_currentString.incrementAndGetCurrentChar16();
}

void SegmentedString::advanceAndUpdateLineNumber8() {
  ASSERT(m_currentString.getCurrentChar() == m_currentChar);
  if (m_currentChar == '\n') {
    ++m_currentLine;
    m_numberOfCharactersConsumedPriorToCurrentLine =
        numberOfCharactersConsumed() + 1;
  }
  decrementAndCheckLength();
  m_currentChar = m_currentString.incrementAndGetCurrentChar8();
}

void SegmentedString::advanceAndUpdateLineNumber16() {
  ASSERT(m_currentString.getCurrentChar() == m_currentChar);
  if (m_currentChar == '\n') {
    ++m_currentLine;
    m_numberOfCharactersConsumedPriorToCurrentLine =
        numberOfCharactersConsumed() + 1;
  }
  decrementAndCheckLength();
  m_currentChar = m_currentString.incrementAndGetCurrentChar16();
}

void SegmentedString::advanceSlowCase() {
  if (m_currentString.length()) {
    m_currentString.decrementLength();
    if (!m_currentString.length())
      advanceSubstring();
  } else if (!isComposite()) {
    m_currentString.clear();
    m_empty = true;
    m_fastPathFlags = NoFastPath;
    m_advanceFunc = &SegmentedString::advanceEmpty;
    m_advanceAndUpdateLineNumberFunc = &SegmentedString::advanceEmpty;
  }
  m_currentChar =
      m_currentString.length() ? m_currentString.getCurrentChar() : 0;
}

void SegmentedString::advanceAndUpdateLineNumberSlowCase() {
  if (m_currentString.length()) {
    if (m_currentString.getCurrentChar() == '\n' &&
        m_currentString.doNotExcludeLineNumbers()) {
      ++m_currentLine;
      // Plus 1 because numberOfCharactersConsumed value hasn't incremented yet;
      // it does with length() decrement below.
      m_numberOfCharactersConsumedPriorToCurrentLine =
          numberOfCharactersConsumed() + 1;
    }
    m_currentString.decrementLength();
    if (!m_currentString.length())
      advanceSubstring();
    else
      m_currentString.incrementAndGetCurrentChar();  // Only need the ++
  } else if (!isComposite()) {
    m_currentString.clear();
    m_empty = true;
    m_fastPathFlags = NoFastPath;
    m_advanceFunc = &SegmentedString::advanceEmpty;
    m_advanceAndUpdateLineNumberFunc = &SegmentedString::advanceEmpty;
  }

  m_currentChar =
      m_currentString.length() ? m_currentString.getCurrentChar() : 0;
}

void SegmentedString::advanceEmpty() {
  ASSERT(!m_currentString.length() && !isComposite());
  m_currentChar = 0;
}

void SegmentedString::updateSlowCaseFunctionPointers() {
  m_fastPathFlags = NoFastPath;
  m_advanceFunc = &SegmentedString::advanceSlowCase;
  m_advanceAndUpdateLineNumberFunc =
      &SegmentedString::advanceAndUpdateLineNumberSlowCase;
}

OrdinalNumber SegmentedString::currentLine() const {
  return OrdinalNumber::fromZeroBasedInt(m_currentLine);
}

OrdinalNumber SegmentedString::currentColumn() const {
  int zeroBasedColumn = numberOfCharactersConsumed() -
                        m_numberOfCharactersConsumedPriorToCurrentLine;
  return OrdinalNumber::fromZeroBasedInt(zeroBasedColumn);
}

void SegmentedString::setCurrentPosition(OrdinalNumber line,
                                         OrdinalNumber columnAftreProlog,
                                         int prologLength) {
  m_currentLine = line.zeroBasedInt();
  m_numberOfCharactersConsumedPriorToCurrentLine =
      numberOfCharactersConsumed() + prologLength -
      columnAftreProlog.zeroBasedInt();
}

}  // namespace blink
