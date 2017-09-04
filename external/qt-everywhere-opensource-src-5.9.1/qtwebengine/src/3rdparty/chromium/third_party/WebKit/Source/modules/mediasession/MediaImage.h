// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaImage_h
#define MediaImage_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class MediaImageInit;

class MODULES_EXPORT MediaImage final
    : public GarbageCollectedFinalized<MediaImage>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaImage* create(ExecutionContext*, const MediaImageInit&);

  String src() const;
  String sizes() const;
  String type() const;

  DEFINE_INLINE_TRACE() {}

 private:
  MediaImage(ExecutionContext*, const MediaImageInit&);

  String m_src;
  String m_sizes;
  String m_type;
};

}  // namespace blink

#endif  // MediaImage_h
