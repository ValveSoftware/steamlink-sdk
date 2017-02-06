/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/dom/custom/V0CustomElementMicrotaskImportStep.h"

#include "core/dom/custom/V0CustomElementMicrotaskDispatcher.h"
#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"
#include <stdio.h>

namespace blink {

V0CustomElementMicrotaskImportStep::V0CustomElementMicrotaskImportStep(HTMLImportChild* import)
    : m_import(import)
    , m_queue(import->loader()->microtaskQueue())
{
}

V0CustomElementMicrotaskImportStep::~V0CustomElementMicrotaskImportStep()
{
}

void V0CustomElementMicrotaskImportStep::invalidate()
{
    m_queue = V0CustomElementSyncMicrotaskQueue::create();
    m_import.clear();
}

bool V0CustomElementMicrotaskImportStep::shouldWaitForImport() const
{
    return m_import && !m_import->loader()->isDone();
}

void V0CustomElementMicrotaskImportStep::didUpgradeAllCustomElements()
{
    DCHECK(m_queue);
    if (m_import)
        m_import->didFinishUpgradingCustomElements();
}

V0CustomElementMicrotaskStep::Result V0CustomElementMicrotaskImportStep::process()
{
    m_queue->dispatch();
    if (!m_queue->isEmpty() || shouldWaitForImport())
        return Processing;

    didUpgradeAllCustomElements();
    return FinishedProcessing;
}

DEFINE_TRACE(V0CustomElementMicrotaskImportStep)
{
    visitor->trace(m_import);
    visitor->trace(m_queue);
    V0CustomElementMicrotaskStep::trace(visitor);
}

#if !defined(NDEBUG)
void V0CustomElementMicrotaskImportStep::show(unsigned indent)
{
    fprintf(stderr, "%*sImport(wait=%d sync=%d, url=%s)\n", indent, "", shouldWaitForImport(), m_import && m_import->isSync(), m_import ? m_import->url().getString().utf8().data() : "null");
    m_queue->show(indent + 1);
}
#endif

} // namespace blink
