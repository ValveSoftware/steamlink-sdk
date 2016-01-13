/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "core/frame/Navigator.h"

#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/NavigatorID.h"
#include "core/frame/Settings.h"
#include "core/loader/CookieJar.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/plugins/DOMMimeTypeArray.h"
#include "core/plugins/DOMPluginArray.h"
#include "platform/Language.h"

#ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB
#define WEBCORE_NAVIGATOR_PRODUCT_SUB "20030107"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB

#ifndef WEBCORE_NAVIGATOR_VENDOR
#define WEBCORE_NAVIGATOR_VENDOR "Google Inc."
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR

#ifndef WEBCORE_NAVIGATOR_VENDOR_SUB
#define WEBCORE_NAVIGATOR_VENDOR_SUB ""
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR_SUB

namespace WebCore {

Navigator::Navigator(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
    ScriptWrappable::init(this);
}

Navigator::~Navigator()
{
}

String Navigator::productSub() const
{
    return WEBCORE_NAVIGATOR_PRODUCT_SUB;
}

String Navigator::vendor() const
{
    return WEBCORE_NAVIGATOR_VENDOR;
}

String Navigator::vendorSub() const
{
    return WEBCORE_NAVIGATOR_VENDOR_SUB;
}

String Navigator::userAgent() const
{
    // If the frame is already detached it no longer has a meaningful useragent.
    if (!m_frame || !m_frame->page())
        return String();

    return m_frame->loader().userAgent(m_frame->document()->url());
}

DOMPluginArray* Navigator::plugins() const
{
    if (!m_plugins)
        m_plugins = DOMPluginArray::create(m_frame);
    return m_plugins.get();
}

DOMMimeTypeArray* Navigator::mimeTypes() const
{
    if (!m_mimeTypes)
        m_mimeTypes = DOMMimeTypeArray::create(m_frame);
    return m_mimeTypes.get();
}

bool Navigator::cookieEnabled() const
{
    if (!m_frame)
        return false;

    Settings* settings = m_frame->settings();
    if (!settings || !settings->cookieEnabled())
        return false;

    return cookiesEnabled(m_frame->document());
}

bool Navigator::javaEnabled() const
{
    if (!m_frame || !m_frame->settings())
        return false;

    if (!m_frame->settings()->javaEnabled())
        return false;

    return true;
}

void Navigator::getStorageUpdates()
{
    // FIXME: Remove this method or rename to yieldForStorageUpdates.
}

Vector<String> Navigator::languages()
{
    Vector<String> languages;

    if (!m_frame || !m_frame->host()) {
        languages.append(defaultLanguage());
        return languages;
    }

    String acceptLanguages = m_frame->host()->chrome().client().acceptLanguages();
    acceptLanguages.split(",", languages);

    // Sanitizing tokens. We could do that more extensively but we should assume
    // that the accept languages are already sane and support BCP47. It is
    // likely a waste of time to make sure the tokens matches that spec here.
    for (size_t i = 0; i < languages.size(); ++i) {
        String& token = languages[i];
        token = token.stripWhiteSpace();
        if (token.length() >= 3 && token[2] == '_')
            token.replace(2, 1, "-");
    }

    return languages;
}

void Navigator::trace(Visitor* visitor)
{
    visitor->trace(m_plugins);
    visitor->trace(m_mimeTypes);
    WillBeHeapSupplementable<Navigator>::trace(visitor);
}

} // namespace WebCore
