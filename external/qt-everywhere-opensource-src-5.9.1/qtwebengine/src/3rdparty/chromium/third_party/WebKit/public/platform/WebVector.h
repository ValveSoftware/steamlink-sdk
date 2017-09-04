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

#ifndef WebVector_h
#define WebVector_h

#include "WebCommon.h"
#include "base/logging.h"

#include <algorithm>
#include <vector>

namespace blink {

// A simple vector class that wraps a std::vector.
//
// Sample usage:
//
//   void Foo(WebVector<int>& result)
//   {
//       WebVector<int> data(10);
//       for (size_t i = 0; i < data.size(); ++i)
//           data[i] = ...
//       result.swap(data);
//   }
//
// It is also possible to assign from any container that implements begin()
// and end().
//
//   void Foo(const std::vector<std::string>& input)
//   {
//       WebVector<WebCString> cstrings = input;
//       ...
//   }
//
template <typename T>
class WebVector {
 public:
  using value_type = typename std::vector<T>::value_type;
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  ~WebVector() {}

  explicit WebVector(size_t size = 0) : m_data(size) {}

  template <typename U>
  WebVector(const U* values, size_t size) : m_data(values, values + size) {}

  WebVector(const WebVector<T>& other) : m_data(other.m_data) {}

  template <typename C>
  WebVector(const C& other) : m_data(other.begin(), other.end()) {}

  WebVector(WebVector<T>&& other) { swap(other); }

  WebVector& operator=(const WebVector& other) {
    if (this != &other)
      assign(other);
    return *this;
  }

  WebVector& operator=(WebVector&& other) {
    if (this != &other)
      swap(other);
    return *this;
  }

  template <typename C>
  WebVector<T>& operator=(const C& other) {
    if (this != reinterpret_cast<const WebVector<T>*>(&other))
      assign(other);
    return *this;
  }

  template <typename C>
  void assign(const C& other) {
    m_data.assign(other.begin(), other.end());
  }

  template <typename U>
  void assign(const U* values, size_t size) {
    m_data.assign(values, values + size);
  }

  size_t size() const { return m_data.size(); }
  bool isEmpty() const { return m_data.empty(); }

  T& operator[](size_t i) {
    DCHECK_LT(i, m_data.size());
    return m_data[i];
  }

  const T& operator[](size_t i) const {
    DCHECK_LT(i, m_data.size());
    return m_data[i];
  }

  T* data() { return m_data.data(); }
  const T* data() const { return m_data.data(); }

  iterator begin() { return m_data.begin(); }
  iterator end() { return m_data.end(); }
  const_iterator begin() const { return m_data.begin(); }
  const_iterator end() const { return m_data.end(); }

  void swap(WebVector<T>& other) { m_data.swap(other.m_data); }

 private:
  std::vector<T> m_data;
};

}  // namespace blink

#endif
