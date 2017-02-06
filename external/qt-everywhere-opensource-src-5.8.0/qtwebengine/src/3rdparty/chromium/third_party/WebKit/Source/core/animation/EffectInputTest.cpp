// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectInput.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/EffectModelOrDictionarySequenceOrDictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <v8.h>

namespace blink {

Element* appendElement(Document& document)
{
    Element* element = document.createElement("foo", ASSERT_NO_EXCEPTION);
    document.documentElement()->appendChild(element);
    return element;
}

TEST(AnimationEffectInputTest, SortedOffsets)
{
    V8TestingScope scope;
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), scope.getExceptionState()));

    Element* element = appendElement(scope.document());
    EffectModel* animationEffect = EffectInput::convert(element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(jsKeyframes), nullptr, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(animationEffect);
    EXPECT_EQ(1.0, keyframeEffect.getFrames()[1]->offset());
}

TEST(AnimationEffectInputTest, UnsortedOffsets)
{
    V8TestingScope scope;
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "width", "0px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "1");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "width", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "offset", "0");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), scope.getExceptionState()));

    Element* element = appendElement(scope.document());
    EffectInput::convert(element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(jsKeyframes), nullptr, scope.getExceptionState());
    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(AnimationEffectInputTest, LooslySorted)
{
    V8TestingScope scope;
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "width", "200px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "width", "0px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe3, scope.isolate(), scope.getExceptionState()));

    Element* element = appendElement(scope.document());
    EffectModel* animationEffect = EffectInput::convert(element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(jsKeyframes), nullptr, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    const KeyframeEffectModelBase& keyframeEffect = *toKeyframeEffectModelBase(animationEffect);
    EXPECT_EQ(1, keyframeEffect.getFrames()[2]->offset());
}

TEST(AnimationEffectInputTest, OutOfOrderWithNullOffsets)
{
    V8TestingScope scope;
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe4 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "height", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "0.5");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "height", "150px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "height", "200px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "offset", "0");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe4, "height", "300px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe4, "offset", "1");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe3, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe4, scope.isolate(), scope.getExceptionState()));

    Element* element = appendElement(scope.document());
    EffectInput::convert(element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(jsKeyframes), nullptr, scope.getExceptionState());
    EXPECT_TRUE(scope.getExceptionState().hadException());
}

TEST(AnimationEffectInputTest, Invalid)
{
    V8TestingScope scope;
    // Not loosely sorted by offset, and there exists a keyframe with null offset.
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe3 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "width", "0px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "1");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "width", "200px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "width", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe3, "offset", "0");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), scope.getExceptionState()));
    jsKeyframes.append(Dictionary(keyframe3, scope.isolate(), scope.getExceptionState()));

    Element* element = appendElement(scope.document());
    EffectInput::convert(element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(jsKeyframes), nullptr, scope.getExceptionState());
    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

} // namespace blink
