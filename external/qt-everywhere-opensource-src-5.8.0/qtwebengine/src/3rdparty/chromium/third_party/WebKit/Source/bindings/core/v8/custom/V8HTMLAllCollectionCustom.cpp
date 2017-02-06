/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8HTMLAllCollection.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8NodeList.h"
#include "core/dom/StaticNodeList.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAllCollection.h"

namespace blink {

template<class CallbackInfo>
static v8::Local<v8::Value> getNamedItems(HTMLAllCollection* collection, AtomicString name, const CallbackInfo& info)
{
    HeapVector<Member<Element>> namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size())
        return v8Undefined();

    if (namedItems.size() == 1)
        return toV8(namedItems.at(0).release(), info.Holder(), info.GetIsolate());

    // FIXME: HTML5 specification says this should be a HTMLCollection.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlallcollection
    return toV8(StaticElementList::adopt(namedItems), info.Holder(), info.GetIsolate());
}

template<class CallbackInfo>
static v8::Local<v8::Value> getItem(HTMLAllCollection* collection, v8::Local<v8::Value> argument, const CallbackInfo& info,
    UseCounter::Feature namedFeature, UseCounter::Feature indexedFeature, UseCounter::Feature indexedWithNonNumberFeature)
{
    v8::Local<v8::Uint32> index;
    if (!argument->ToArrayIndex(info.GetIsolate()->GetCurrentContext()).ToLocal(&index)) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), namedFeature);
        TOSTRING_DEFAULT(V8StringResource<>, name, argument, v8::Undefined(info.GetIsolate()));
        v8::Local<v8::Value> result = getNamedItems(collection, name, info);

        if (result.IsEmpty())
            return v8::Undefined(info.GetIsolate());

        return result;
    }

    UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), indexedFeature);
    if (!argument->IsNumber())
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), indexedWithNonNumberFeature);

    Element* result = collection->item(index->Value());
    return toV8(result, info.Holder(), info.GetIsolate());
}

void V8HTMLAllCollection::itemMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::DocumentAllItemNoArguments);
        return;
    }

    HTMLAllCollection* impl = V8HTMLAllCollection::toImpl(info.Holder());
    v8SetReturnValue(info, getItem(impl, info[0], info, UseCounter::DocumentAllItemNamed,
        UseCounter::DocumentAllItemIndexed, UseCounter::DocumentAllItemIndexedWithNonNumber));
}

void V8HTMLAllCollection::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::DocumentAllLegacyCallNoArguments);
        return;
    }

    UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::DocumentAllLegacyCall);

    HTMLAllCollection* impl = V8HTMLAllCollection::toImpl(info.Holder());

    if (info.Length() == 1) {
        v8SetReturnValue(info, getItem(impl, info[0], info, UseCounter::DocumentAllLegacyCallNamed,
            UseCounter::DocumentAllLegacyCallIndexed, UseCounter::DocumentAllLegacyCallIndexedWithNonNumber));
        return;
    }

    UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()), UseCounter::DocumentAllLegacyCallTwoArguments);

    // If there is a second argument it is the index of the item we want.
    TOSTRING_VOID(V8StringResource<>, name, info[0]);
    v8::Local<v8::Uint32> index;
    if (!info[1]->ToArrayIndex(info.GetIsolate()->GetCurrentContext()).ToLocal(&index))
        return;

    if (Node* node = impl->namedItemWithIndex(name, index->Value())) {
        v8SetReturnValueFast(info, node, impl);
        return;
    }
}

} // namespace blink
