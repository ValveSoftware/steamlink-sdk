// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/CSSPaintDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/FilteredComputedStylePropertyMap.h"
#include "core/dom/ExecutionContext.h"
#include "core/layout/LayoutObject.h"
#include "modules/csspaint/PaintRenderingContext2D.h"
#include "modules/csspaint/PaintSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/PaintGeneratedImage.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "wtf/PtrUtil.h"

namespace blink {

CSSPaintDefinition* CSSPaintDefinition::create(ScriptState* scriptState, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint, Vector<CSSPropertyID>& nativeInvalidationProperties, Vector<AtomicString>& customInvalidationProperties)
{
    return new CSSPaintDefinition(scriptState, constructor, paint, nativeInvalidationProperties, customInvalidationProperties);
}

CSSPaintDefinition::CSSPaintDefinition(ScriptState* scriptState, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint, Vector<CSSPropertyID>& nativeInvalidationProperties, Vector<AtomicString>& customInvalidationProperties)
    : m_scriptState(scriptState)
    , m_constructor(scriptState->isolate(), constructor)
    , m_paint(scriptState->isolate(), paint)
{
    m_nativeInvalidationProperties.swap(nativeInvalidationProperties);
    m_customInvalidationProperties.swap(customInvalidationProperties);
}

CSSPaintDefinition::~CSSPaintDefinition()
{
}

PassRefPtr<Image> CSSPaintDefinition::paint(const LayoutObject& layoutObject, const IntSize& size)
{
    ScriptState::Scope scope(m_scriptState.get());

    maybeCreatePaintInstance();

    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Object> instance = m_instance.newLocal(isolate);

    // We may have failed to create an instance class, in which case produce an
    // invalid image.
    if (isUndefinedOrNull(instance))
        return nullptr;

    DCHECK(layoutObject.node());

    PaintRenderingContext2D* renderingContext = PaintRenderingContext2D::create(
        ImageBuffer::create(wrapUnique(new RecordingImageBufferSurface(size))));
    PaintSize* paintSize = PaintSize::create(size);
    StylePropertyMap* styleMap = FilteredComputedStylePropertyMap::create(
        CSSComputedStyleDeclaration::create(layoutObject.node()),
        m_nativeInvalidationProperties, m_customInvalidationProperties);

    v8::Local<v8::Value> argv[] = {
        toV8(renderingContext, m_scriptState->context()->Global(), isolate),
        toV8(paintSize, m_scriptState->context()->Global(), isolate),
        toV8(styleMap, m_scriptState->context()->Global(), isolate)
    };

    v8::Local<v8::Function> paint = m_paint.newLocal(isolate);

    v8::TryCatch block(isolate);
    block.SetVerbose(true);

    V8ScriptRunner::callFunction(paint, m_scriptState->getExecutionContext(), instance, 3, argv, isolate);

    // The paint function may have produced an error, in which case produce an
    // invalid image.
    if (block.HasCaught()) {
        return nullptr;
    }

    return PaintGeneratedImage::create(renderingContext->imageBuffer()->getPicture(), size);
}

void CSSPaintDefinition::maybeCreatePaintInstance()
{
    if (m_didCallConstructor)
        return;

    DCHECK(m_instance.isEmpty());

    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Function> constructor = m_constructor.newLocal(isolate);
    DCHECK(!isUndefinedOrNull(constructor));

    v8::Local<v8::Object> paintInstance;
    if (V8ObjectConstructor::newInstance(isolate, constructor).ToLocal(&paintInstance)) {
        m_instance.set(isolate, paintInstance);
    }

    m_didCallConstructor = true;
}

} // namespace blink
