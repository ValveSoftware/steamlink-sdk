/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright 2014 The Chromium Authors. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/InjectedStyleSheets.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "wtf/HashSet.h"

namespace WebCore {

// static
InjectedStyleSheets& InjectedStyleSheets::instance()
{
    DEFINE_STATIC_LOCAL(InjectedStyleSheets, instance, ());
    return instance;
}

void InjectedStyleSheets::add(const String& source, const Vector<String>& whitelist, StyleInjectionTarget injectIn)
{
    m_entries.append(adoptPtr(new InjectedStyleSheetEntry(source, whitelist, injectIn)));
    invalidateInjectedStyleSheetCacheInAllFrames();
}

void InjectedStyleSheets::removeAll()
{
    m_entries.clear();
    invalidateInjectedStyleSheetCacheInAllFrames();
}

void InjectedStyleSheets::invalidateInjectedStyleSheetCacheInAllFrames()
{
    // Clear our cached sheets and have them just reparse.
    const HashSet<Page*>& pages = Page::ordinaryPages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->isLocalFrame())
                toLocalFrame(frame)->document()->styleEngine()->invalidateInjectedStyleSheetCache();
        }
    }
}

} // namespace WebCore
