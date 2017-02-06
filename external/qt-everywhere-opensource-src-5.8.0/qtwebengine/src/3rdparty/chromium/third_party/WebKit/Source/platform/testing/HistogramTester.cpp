// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/HistogramTester.h"

#include "base/test/histogram_tester.h"
#include "wtf/PtrUtil.h"

namespace blink {

HistogramTester::HistogramTester() : m_histogramTester(wrapUnique(new base::HistogramTester)) { }

HistogramTester::~HistogramTester() { }

void HistogramTester::expectUniqueSample(const std::string& name, base::HistogramBase::Sample sample, base::HistogramBase::Count count) const
{
    m_histogramTester->ExpectUniqueSample(name, sample, count);
}

void HistogramTester::expectTotalCount(const std::string& name, base::HistogramBase::Count count) const
{
    m_histogramTester->ExpectTotalCount(name, count);
}

} // namespace blink
