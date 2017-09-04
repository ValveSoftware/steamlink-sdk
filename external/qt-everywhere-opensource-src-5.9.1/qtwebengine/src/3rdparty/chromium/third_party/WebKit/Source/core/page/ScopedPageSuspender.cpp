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

#include "core/page/ScopedPageSuspender.h"

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

unsigned s_suspensionCount = 0;

}  // namespace

ScopedPageSuspender::ScopedPageSuspender() {
  if (++s_suspensionCount > 1)
    return;

  setSuspended(true);
  Platform::current()->currentThread()->scheduler()->suspendTimerQueue();
}

ScopedPageSuspender::~ScopedPageSuspender() {
  if (--s_suspensionCount > 0)
    return;

  setSuspended(false);
  Platform::current()->currentThread()->scheduler()->resumeTimerQueue();
}

void ScopedPageSuspender::setSuspended(bool isSuspended) {
  // Make a copy of the collection. Undeferring loads can cause script to run,
  // which would mutate ordinaryPages() in the middle of iteration.
  HeapVector<Member<Page>> pages;
  copyToVector(Page::ordinaryPages(), pages);

  for (const auto& page : pages)
    page->setSuspended(isSuspended);
}

bool ScopedPageSuspender::isActive() {
  return s_suspensionCount > 0;
}

}  // namespace blink
