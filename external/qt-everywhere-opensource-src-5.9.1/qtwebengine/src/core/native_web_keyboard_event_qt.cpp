/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"
#include <QKeyEvent>

namespace {

// We need to copy |os_event| in NativeWebKeyboardEvent because it is
// queued in RenderWidgetHost and may be passed and used
// RenderViewHostDelegate::HandledKeybardEvent after the original aura
// event is destroyed.
gfx::NativeEvent CopyEvent(gfx::NativeEvent event)
{
    return event ? reinterpret_cast<gfx::NativeEvent>(new QKeyEvent(*reinterpret_cast<QKeyEvent*>(event))) : 0;
}

void DestroyEvent(gfx::NativeEvent event)
{
    delete reinterpret_cast<QKeyEvent*>(event);
}

}  // namespace

using blink::WebKeyboardEvent;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(0),
      skip_in_browser(false)
{
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : os_event(CopyEvent(native_event)),
      skip_in_browser(false)
{
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
    os_event(CopyEvent(other.os_event)),
    skip_in_browser(other.skip_in_browser)
{
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(const NativeWebKeyboardEvent& other) {
    WebKeyboardEvent::operator=(other);
    DestroyEvent(os_event);
    os_event = CopyEvent(other.os_event);
    skip_in_browser = other.skip_in_browser;
    return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
    DestroyEvent(os_event);
}

}  // namespace content
