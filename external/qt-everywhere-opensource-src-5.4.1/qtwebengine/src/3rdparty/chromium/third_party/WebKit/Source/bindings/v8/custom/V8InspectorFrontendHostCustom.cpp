/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/V8InspectorFrontendHost.h"

#include "bindings/core/v8/V8MouseEvent.h"
#include "bindings/v8/V8Binding.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendClient.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "platform/ContextMenu.h"
#include "public/platform/Platform.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

void V8InspectorFrontendHost::platformMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
#if OS(MACOSX)
    v8SetReturnValue(info, v8AtomicString(info.GetIsolate(), "mac"));
#elif OS(WIN)
    v8SetReturnValue(info, v8AtomicString(info.GetIsolate(), "windows"));
#else // Unix-like systems
    v8SetReturnValue(info, v8AtomicString(info.GetIsolate(), "linux"));
#endif
}

void V8InspectorFrontendHost::portMethodCustom(const v8::FunctionCallbackInfo<v8::Value>&)
{
}

static bool populateContextMenuItems(v8::Local<v8::Array>& itemArray, ContextMenu& menu, v8::Isolate* isolate)
{
    for (size_t i = 0; i < itemArray->Length(); ++i) {
        v8::Local<v8::Object> item = v8::Local<v8::Object>::Cast(itemArray->Get(i));
        v8::Local<v8::Value> type = item->Get(v8AtomicString(isolate, "type"));
        v8::Local<v8::Value> id = item->Get(v8AtomicString(isolate, "id"));
        v8::Local<v8::Value> label = item->Get(v8AtomicString(isolate, "label"));
        v8::Local<v8::Value> enabled = item->Get(v8AtomicString(isolate, "enabled"));
        v8::Local<v8::Value> checked = item->Get(v8AtomicString(isolate, "checked"));
        v8::Local<v8::Value> subItems = item->Get(v8AtomicString(isolate, "subItems"));
        if (!type->IsString())
            continue;
        String typeString = toCoreStringWithNullCheck(type.As<v8::String>());
        if (typeString == "separator") {
            ContextMenuItem item(ContextMenuItem(SeparatorType,
                                 ContextMenuItemCustomTagNoAction,
                                 String()));
            menu.appendItem(item);
        } else if (typeString == "subMenu" && subItems->IsArray()) {
            ContextMenu subMenu;
            v8::Local<v8::Array> subItemsArray = v8::Local<v8::Array>::Cast(subItems);
            if (!populateContextMenuItems(subItemsArray, subMenu, isolate))
                return false;
            TOSTRING_DEFAULT(V8StringResource<WithNullCheck>, labelString, label, false);
            ContextMenuItem item(SubmenuType,
                ContextMenuItemCustomTagNoAction,
                labelString,
                &subMenu);
            menu.appendItem(item);
        } else {
            ContextMenuAction typedId = static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + id->ToInt32()->Value());
            TOSTRING_DEFAULT(V8StringResource<WithNullCheck>, labelString, label, false);
            ContextMenuItem menuItem((typeString == "checkbox" ? CheckableActionType : ActionType), typedId, labelString);
            if (checked->IsBoolean())
                menuItem.setChecked(checked->ToBoolean()->Value());
            if (enabled->IsBoolean())
                menuItem.setEnabled(enabled->ToBoolean()->Value());
            menu.appendItem(menuItem);
        }
    }
    return true;
}

void V8InspectorFrontendHost::showContextMenuMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2)
        return;

    v8::Local<v8::Object> eventWrapper = v8::Local<v8::Object>::Cast(info[0]);
    if (!V8MouseEvent::wrapperTypeInfo.equals(toWrapperTypeInfo(eventWrapper)))
        return;

    Event* event = V8Event::toNative(eventWrapper);
    if (!info[1]->IsArray())
        return;

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(info[1]);
    ContextMenu menu;
    if (!populateContextMenuItems(array, menu, info.GetIsolate()))
        return;

    InspectorFrontendHost* frontendHost = V8InspectorFrontendHost::toNative(info.Holder());
    Vector<ContextMenuItem> items = menu.items();
    frontendHost->showContextMenu(event, items);
}

static void histogramEnumeration(const char* name, const v8::FunctionCallbackInfo<v8::Value>& info, int boundaryValue)
{
    if (info.Length() < 1 || !info[0]->IsInt32())
        return;

    int sample = info[0]->ToInt32()->Value();
    if (sample < boundaryValue)
        blink::Platform::current()->histogramEnumeration(name, sample, boundaryValue);
}

void V8InspectorFrontendHost::recordActionTakenMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    histogramEnumeration("DevTools.ActionTaken", info, 100);
}

void V8InspectorFrontendHost::recordPanelShownMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    histogramEnumeration("DevTools.PanelShown", info, 20);
}

} // namespace WebCore

