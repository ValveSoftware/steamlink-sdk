/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/animation/EffectInput.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/DictionaryHelperForBindings.h"
#include "bindings/core/v8/DictionarySequenceOrDictionary.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeComputedStyle.h"
#include "wtf/ASCIICType.h"
#include "wtf/HashSet.h"
#include "wtf/NonCopyingSort.h"

namespace blink {

namespace {

bool compareKeyframes(const RefPtr<StringKeyframe>& a,
                      const RefPtr<StringKeyframe>& b) {
  return a->offset() < b->offset();
}

// Validates the value of |offset| and throws an exception if out of range.
bool checkOffset(double offset,
                 double lastOffset,
                 ExceptionState& exceptionState) {
  // Keyframes with offsets outside the range [0.0, 1.0] are an error.
  if (std::isnan(offset)) {
    exceptionState.throwTypeError("Non numeric offset provided");
    return false;
  }

  if (offset < 0 || offset > 1) {
    exceptionState.throwTypeError("Offsets provided outside the range [0, 1]");
    return false;
  }

  if (offset < lastOffset) {
    exceptionState.throwTypeError(
        "Keyframes with specified offsets are not sorted");
    return false;
  }

  return true;
}

void setKeyframeValue(Element& element,
                      StringKeyframe& keyframe,
                      const String& property,
                      const String& value) {
  StyleSheetContents* styleSheetContents =
      element.document().elementSheet().contents();
  CSSPropertyID cssProperty =
      AnimationInputHelpers::keyframeAttributeToCSSProperty(property,
                                                            element.document());
  if (cssProperty != CSSPropertyInvalid) {
    if (cssProperty == CSSPropertyVariable)
      keyframe.setCSSPropertyValue(AtomicString(property), value,
                                   styleSheetContents);
    else
      keyframe.setCSSPropertyValue(cssProperty, value, styleSheetContents);
    return;
  }
  cssProperty = AnimationInputHelpers::keyframeAttributeToPresentationAttribute(
      property, element);
  if (cssProperty != CSSPropertyInvalid) {
    keyframe.setPresentationAttributeValue(cssProperty, value,
                                           styleSheetContents);
    return;
  }
  const QualifiedName* svgAttribute =
      AnimationInputHelpers::keyframeAttributeToSVGAttribute(property, element);
  if (svgAttribute)
    keyframe.setSVGAttributeValue(*svgAttribute, value);
}

EffectModel* createEffectModelFromKeyframes(
    Element& element,
    const StringKeyframeVector& keyframes,
    ExceptionState& exceptionState) {
  StringKeyframeEffectModel* keyframeEffectModel =
      StringKeyframeEffectModel::create(keyframes,
                                        LinearTimingFunction::shared());
  if (!RuntimeEnabledFeatures::cssAdditiveAnimationsEnabled()) {
    for (const auto& keyframeGroup :
         keyframeEffectModel->getPropertySpecificKeyframeGroups()) {
      PropertyHandle property = keyframeGroup.key;
      if (!property.isCSSProperty())
        continue;

      for (const auto& keyframe : keyframeGroup.value->keyframes()) {
        if (keyframe->isNeutral()) {
          exceptionState.throwDOMException(
              NotSupportedError, "Partial keyframes are not supported.");
          return nullptr;
        }
        if (keyframe->composite() != EffectModel::CompositeReplace) {
          exceptionState.throwDOMException(
              NotSupportedError, "Additive animations are not supported.");
          return nullptr;
        }
      }
    }
  }

  DCHECK(!exceptionState.hadException());
  return keyframeEffectModel;
}

bool exhaustDictionaryIterator(DictionaryIterator& iterator,
                               ExecutionContext* executionContext,
                               ExceptionState& exceptionState,
                               Vector<Dictionary>& result) {
  while (iterator.next(executionContext, exceptionState)) {
    Dictionary dictionary;
    if (!iterator.valueAsDictionary(dictionary, exceptionState)) {
      exceptionState.throwTypeError("Keyframes must be objects.");
      return false;
    }
    result.append(dictionary);
  }
  return !exceptionState.hadException();
}

}  // namespace

// Spec: http://w3c.github.io/web-animations/#processing-a-keyframes-argument
EffectModel* EffectInput::convert(
    Element* element,
    const DictionarySequenceOrDictionary& effectInput,
    ExecutionContext* executionContext,
    ExceptionState& exceptionState) {
  if (effectInput.isNull() || !element)
    return nullptr;

  if (effectInput.isDictionarySequence())
    return convertArrayForm(*element, effectInput.getAsDictionarySequence(),
                            exceptionState);

  const Dictionary& dictionary = effectInput.getAsDictionary();
  DictionaryIterator iterator = dictionary.getIterator(executionContext);
  if (!iterator.isNull()) {
    // TODO(alancutter): Convert keyframes during iteration rather than after to
    // match spec.
    Vector<Dictionary> keyframeDictionaries;
    if (exhaustDictionaryIterator(iterator, executionContext, exceptionState,
                                  keyframeDictionaries))
      return convertArrayForm(*element, keyframeDictionaries, exceptionState);
    return nullptr;
  }

  return convertObjectForm(*element, dictionary, executionContext,
                           exceptionState);
}

EffectModel* EffectInput::convertArrayForm(
    Element& element,
    const Vector<Dictionary>& keyframeDictionaries,
    ExceptionState& exceptionState) {
  StringKeyframeVector keyframes;
  double lastOffset = 0;

  for (const Dictionary& keyframeDictionary : keyframeDictionaries) {
    RefPtr<StringKeyframe> keyframe = StringKeyframe::create();

    Nullable<double> offset;
    if (DictionaryHelper::get(keyframeDictionary, "offset", offset) &&
        !offset.isNull()) {
      if (!checkOffset(offset.get(), lastOffset, exceptionState))
        return nullptr;

      lastOffset = offset.get();
      keyframe->setOffset(offset.get());
    }

    String compositeString;
    DictionaryHelper::get(keyframeDictionary, "composite", compositeString);
    if (compositeString == "add")
      keyframe->setComposite(EffectModel::CompositeAdd);
    // TODO(alancutter): Support "accumulate" keyframe composition.

    String timingFunctionString;
    if (DictionaryHelper::get(keyframeDictionary, "easing",
                              timingFunctionString)) {
      RefPtr<TimingFunction> timingFunction =
          AnimationInputHelpers::parseTimingFunction(
              timingFunctionString, &element.document(), exceptionState);
      if (!timingFunction)
        return nullptr;
      keyframe->setEasing(timingFunction);
    }

    Vector<String> keyframeProperties;
    keyframeDictionary.getPropertyNames(keyframeProperties);
    for (const auto& property : keyframeProperties) {
      if (property == "offset" || property == "composite" ||
          property == "easing") {
        continue;
      }

      Vector<String> values;
      if (DictionaryHelper::get(keyframeDictionary, property, values)) {
        exceptionState.throwTypeError(
            "Lists of values not permitted in array-form list of keyframes");
        return nullptr;
      }

      String value;
      DictionaryHelper::get(keyframeDictionary, property, value);

      setKeyframeValue(element, *keyframe.get(), property, value);
    }
    keyframes.append(keyframe);
  }

  DCHECK(!exceptionState.hadException());

  return createEffectModelFromKeyframes(element, keyframes, exceptionState);
}

static bool getPropertyIndexedKeyframeValues(
    const Dictionary& keyframeDictionary,
    const String& property,
    ExecutionContext* executionContext,
    ExceptionState& exceptionState,
    Vector<String>& result) {
  DCHECK(result.isEmpty());

  // Array of strings.
  if (DictionaryHelper::get(keyframeDictionary, property, result))
    return true;

  Dictionary valuesDictionary;
  if (!keyframeDictionary.get(property, valuesDictionary) ||
      valuesDictionary.isUndefinedOrNull()) {
    // Non-object.
    String value;
    DictionaryHelper::get(keyframeDictionary, property, value);
    result.append(value);
    return true;
  }

  DictionaryIterator iterator = valuesDictionary.getIterator(executionContext);
  if (iterator.isNull()) {
    // Non-iterable object.
    String value;
    DictionaryHelper::get(keyframeDictionary, property, value);
    result.append(value);
    return true;
  }

  // Iterable object.
  while (iterator.next(executionContext, exceptionState)) {
    String value;
    if (!iterator.valueAsString(value)) {
      exceptionState.throwTypeError("Unable to read keyframe value as string.");
      return false;
    }
    result.append(value);
  }
  return !exceptionState.hadException();
}

EffectModel* EffectInput::convertObjectForm(
    Element& element,
    const Dictionary& keyframeDictionary,
    ExecutionContext* executionContext,
    ExceptionState& exceptionState) {
  StringKeyframeVector keyframes;

  String timingFunctionString;
  RefPtr<TimingFunction> timingFunction = nullptr;
  if (DictionaryHelper::get(keyframeDictionary, "easing",
                            timingFunctionString)) {
    timingFunction = AnimationInputHelpers::parseTimingFunction(
        timingFunctionString, &element.document(), exceptionState);
    if (!timingFunction)
      return nullptr;
  }

  Nullable<double> offset;
  if (DictionaryHelper::get(keyframeDictionary, "offset", offset) &&
      !offset.isNull()) {
    if (!checkOffset(offset.get(), 0.0, exceptionState))
      return nullptr;
  }

  String compositeString;
  DictionaryHelper::get(keyframeDictionary, "composite", compositeString);

  Vector<String> keyframeProperties;
  keyframeDictionary.getPropertyNames(keyframeProperties);
  for (const auto& property : keyframeProperties) {
    if (property == "offset" || property == "composite" ||
        property == "easing") {
      continue;
    }

    Vector<String> values;
    if (!getPropertyIndexedKeyframeValues(keyframeDictionary, property,
                                          executionContext, exceptionState,
                                          values))
      return nullptr;

    size_t numKeyframes = values.size();
    for (size_t i = 0; i < numKeyframes; ++i) {
      RefPtr<StringKeyframe> keyframe = StringKeyframe::create();

      if (!offset.isNull())
        keyframe->setOffset(offset.get());
      else if (numKeyframes == 1)
        keyframe->setOffset(1.0);
      else
        keyframe->setOffset(i / (numKeyframes - 1.0));

      if (timingFunction)
        keyframe->setEasing(timingFunction);

      if (compositeString == "add")
        keyframe->setComposite(EffectModel::CompositeAdd);
      // TODO(alancutter): Support "accumulate" keyframe composition.

      setKeyframeValue(element, *keyframe.get(), property, values[i]);
      keyframes.append(keyframe);
    }
  }

  std::sort(keyframes.begin(), keyframes.end(), compareKeyframes);

  DCHECK(!exceptionState.hadException());

  return createEffectModelFromKeyframes(element, keyframes, exceptionState);
}

}  // namespace blink
