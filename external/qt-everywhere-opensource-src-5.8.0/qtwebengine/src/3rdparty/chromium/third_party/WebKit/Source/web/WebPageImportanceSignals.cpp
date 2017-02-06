// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebPageImportanceSignals.h"

#include "platform/Histogram.h"
#include "public/web/WebViewClient.h"

namespace blink {

void WebPageImportanceSignals::reset()
{
    m_hadFormInteraction = false;
    m_issuedNonGetFetchFromScript = false;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::setHadFormInteraction()
{
    m_hadFormInteraction = true;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::setIssuedNonGetFetchFromScript()
{
    m_issuedNonGetFetchFromScript = true;
    if (m_observer)
        m_observer->pageImportanceSignalsChanged();
}

void WebPageImportanceSignals::onCommitLoad()
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, hadFormInteractionHistogram, ("PageImportanceSignals.HadFormInteraction.OnCommitLoad", 2));
    hadFormInteractionHistogram.count(m_hadFormInteraction);

    DEFINE_STATIC_LOCAL(EnumerationHistogram, issuedNonGetHistogram, ("PageImportanceSignals.IssuedNonGetFetchFromScript.OnCommitLoad", 2));
    issuedNonGetHistogram.count(m_issuedNonGetFetchFromScript);

    reset();
}

} // namespace blink
