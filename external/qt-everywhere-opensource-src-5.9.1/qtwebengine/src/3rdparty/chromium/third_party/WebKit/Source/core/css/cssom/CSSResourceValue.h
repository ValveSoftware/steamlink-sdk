// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSResourceValue_h
#define CSSResourceValue_h

#include "core/css/cssom/CSSStyleValue.h"

#include "core/fetch/Resource.h"

namespace blink {

class CORE_EXPORT CSSResourceValue : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSResourceValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSResourceValue() {}

  const String state() const {
    switch (status()) {
      case Resource::Status::NotStarted:
        return "unloaded";
      case Resource::Status::Pending:
        return "loading";
      case Resource::Status::Cached:
        return "loaded";
      case Resource::Status::LoadError:
      case Resource::Status::DecodeError:
        return "error";
      default:
        NOTREACHED();
        return "";
    }
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { CSSStyleValue::trace(visitor); }

 protected:
  CSSResourceValue() {}

  virtual Resource::Status status() const = 0;
};

}  // namespace blink

#endif  // CSSResourceValue_h
