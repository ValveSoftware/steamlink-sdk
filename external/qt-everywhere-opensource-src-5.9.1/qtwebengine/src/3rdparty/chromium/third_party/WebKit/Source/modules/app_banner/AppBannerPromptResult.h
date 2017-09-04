// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppBannerPromptResult_h
#define AppBannerPromptResult_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AppBannerPromptResult final
    : public GarbageCollectedFinalized<AppBannerPromptResult>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(AppBannerPromptResult);

 public:
  enum class Outcome { Accepted, Dismissed };

  static AppBannerPromptResult* create(const String& platform,
                                       Outcome outcome) {
    return new AppBannerPromptResult(platform, outcome);
  }

  virtual ~AppBannerPromptResult();

  String platform() const { return m_platform; }
  String outcome() const;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 private:
  AppBannerPromptResult(const String& platform, Outcome);

  String m_platform;
  Outcome m_outcome;
};

}  // namespace blink

#endif  // AppBannerPromptResult_h
