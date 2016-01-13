/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/MediaQueryListListener.h"

#include "bindings/core/v8/V8MediaQueryList.h"
#include "bindings/v8/V8Callback.h"
#include <v8.h>

namespace WebCore {

MediaQueryListListener::MediaQueryListListener(ScriptState* scriptState, const ScriptValue& function)
    : m_scriptState(scriptState)
    , m_function(function)
{
    ASSERT(m_function.isFunction());
}

void MediaQueryListListener::queryChanged(MediaQueryList* query)
{
    if (m_scriptState->contextIsEmpty())
        return;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Handle<v8::Value> args[] = { toV8(query, m_scriptState->context()->Global(), m_scriptState->isolate()) };
    invokeCallback(m_scriptState.get(), v8::Handle<v8::Function>::Cast(m_function.v8Value()), WTF_ARRAY_LENGTH(args), args);
}

}
