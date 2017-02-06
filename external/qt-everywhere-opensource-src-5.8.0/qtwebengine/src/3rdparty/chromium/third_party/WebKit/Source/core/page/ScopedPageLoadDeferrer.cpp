/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/page/ScopedPageLoadDeferrer.h"

#include "core/dom/Document.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

unsigned s_deferralCount = 0;

void setDefersLoading(bool isDeferred)
{
    // Make a copy of the collection. Undeferring loads can cause script to run,
    // which would mutate ordinaryPages() in the middle of iteration.
    HeapVector<Member<Page>> pages;
    for (const auto& page : Page::ordinaryPages())
        pages.append(page);
    for (const auto& page : pages)
        page->setDefersLoading(isDeferred);
}

} // namespace

ScopedPageLoadDeferrer::ScopedPageLoadDeferrer()
{
    if (++s_deferralCount > 1)
        return;

    setDefersLoading(true);
    Platform::current()->currentThread()->scheduler()->suspendTimerQueue();
}

ScopedPageLoadDeferrer::~ScopedPageLoadDeferrer()
{
    if (--s_deferralCount > 0)
        return;

    setDefersLoading(false);
    Platform::current()->currentThread()->scheduler()->resumeTimerQueue();
}

bool ScopedPageLoadDeferrer::isActive()
{
    return s_deferralCount > 0;
}

} // namespace blink
