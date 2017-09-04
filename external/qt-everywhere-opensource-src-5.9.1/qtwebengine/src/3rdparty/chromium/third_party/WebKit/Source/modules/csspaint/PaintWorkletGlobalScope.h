// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorkletGlobalScope_h
#define PaintWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "modules/ModulesExport.h"
#include "platform/graphics/ImageBuffer.h"

namespace blink {

class CSSPaintDefinition;
class CSSPaintImageGeneratorImpl;
class ExceptionState;

class MODULES_EXPORT PaintWorkletGlobalScope final
    : public MainThreadWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletGlobalScope);

 public:
  static PaintWorkletGlobalScope* create(LocalFrame*,
                                         const KURL&,
                                         const String& userAgent,
                                         PassRefPtr<SecurityOrigin>,
                                         v8::Isolate*);
  ~PaintWorkletGlobalScope() override;
  void dispose() final;

  bool isPaintWorkletGlobalScope() const final { return true; }
  void registerPaint(const String& name,
                     const ScriptValue& ctor,
                     ExceptionState&);

  CSSPaintDefinition* findDefinition(const String& name);
  void addPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);

  DECLARE_VIRTUAL_TRACE();

 private:
  PaintWorkletGlobalScope(LocalFrame*,
                          const KURL&,
                          const String& userAgent,
                          PassRefPtr<SecurityOrigin>,
                          v8::Isolate*);

  typedef HeapHashMap<String, Member<CSSPaintDefinition>> DefinitionMap;
  DefinitionMap m_paintDefinitions;

  // The map of CSSPaintImageGeneratorImpl which are waiting for a
  // CSSPaintDefinition to be registered. The global scope is expected to
  // outlive the generators hence are held onto with a WeakMember.
  typedef HeapHashSet<WeakMember<CSSPaintImageGeneratorImpl>> GeneratorHashSet;
  typedef HeapHashMap<String, Member<GeneratorHashSet>> PendingGeneratorMap;
  PendingGeneratorMap m_pendingGenerators;
};

DEFINE_TYPE_CASTS(PaintWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->isPaintWorkletGlobalScope(),
                  context.isPaintWorkletGlobalScope());

}  // namespace blink

#endif  // PaintWorkletGlobalScope_h
