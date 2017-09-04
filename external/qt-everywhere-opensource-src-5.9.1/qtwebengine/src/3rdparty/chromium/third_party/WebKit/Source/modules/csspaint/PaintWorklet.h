// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorklet_h
#define PaintWorklet_h

#include "core/workers/Worklet.h"
#include "modules/ModulesExport.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSPaintDefinition;
class CSSPaintImageGeneratorImpl;

class MODULES_EXPORT PaintWorklet final : public Worklet {
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  static PaintWorklet* create(LocalFrame*);
  ~PaintWorklet() override;

  PaintWorkletGlobalScope* workletGlobalScopeProxy() const final;
  CSSPaintDefinition* findDefinition(const String& name);
  void addPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PaintWorklet(LocalFrame*);

  Member<PaintWorkletGlobalScope> m_paintWorkletGlobalScope;
};

}  // namespace blink

#endif  // PaintWorklet_h
