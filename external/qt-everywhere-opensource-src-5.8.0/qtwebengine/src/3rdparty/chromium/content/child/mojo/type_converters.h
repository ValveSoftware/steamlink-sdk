// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "mojo/public/cpp/bindings/array.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace mojo {

template <typename T, typename U>
struct TypeConverter<Array<T>, blink::WebVector<U>> {
  static Array<T> Convert(const blink::WebVector<U>& vector) {
    Array<T> array(vector.size());
    for (size_t i = 0; i < vector.size(); ++i)
      array[i] = TypeConverter<T, U>::Convert(vector[i]);
    return std::move(array);
  }
};

template <>
struct TypeConverter<String, blink::WebString> {
  static String Convert(const blink::WebString& web_string) {
    return String(web_string.utf8());
  }
};

}  // namespace mojo
