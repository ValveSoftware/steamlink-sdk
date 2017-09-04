// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMap_h
#define StylePropertyMap_h

#include "bindings/core/v8/CSSStyleValueOrCSSStyleValueSequence.h"
#include "bindings/core/v8/CSSStyleValueOrCSSStyleValueSequenceOrString.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT StylePropertyMap
    : public GarbageCollectedFinalized<StylePropertyMap>,
      public ScriptWrappable,
      public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence> {
  WTF_MAKE_NONCOPYABLE(StylePropertyMap);
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef std::pair<String, CSSStyleValueOrCSSStyleValueSequence>
      StylePropertyMapEntry;

  virtual ~StylePropertyMap() {}

  // Accessors.
  virtual CSSStyleValue* get(const String& propertyName, ExceptionState&);
  virtual CSSStyleValueVector getAll(const String& propertyName,
                                     ExceptionState&);
  virtual bool has(const String& propertyName, ExceptionState&);

  virtual Vector<String> getProperties() = 0;

  // Modifiers.
  void set(const String& propertyName,
           CSSStyleValueOrCSSStyleValueSequenceOrString& item,
           ExceptionState&);
  void append(const String& propertyName,
              CSSStyleValueOrCSSStyleValueSequenceOrString& item,
              ExceptionState&);
  void remove(const String& propertyName, ExceptionState&);

  virtual void set(CSSPropertyID,
                   CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                   ExceptionState&) = 0;
  virtual void append(CSSPropertyID,
                      CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                      ExceptionState&) = 0;
  virtual void remove(CSSPropertyID, ExceptionState&) = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  StylePropertyMap() {}

  virtual CSSStyleValueVector getAllInternal(CSSPropertyID) = 0;
  virtual CSSStyleValueVector getAllInternal(
      AtomicString customPropertyName) = 0;

  virtual HeapVector<StylePropertyMapEntry> getIterationEntries() = 0;
  IterationSource* startIteration(ScriptState*, ExceptionState&) override;
};

}  // namespace blink

#endif
