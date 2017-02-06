// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Histogram_h
#define Histogram_h

#include "base/metrics/histogram_base.h"
#include "platform/PlatformExport.h"
#include "wtf/CurrentTime.h"
#include <stdint.h>

namespace base {
class HistogramBase;
};

namespace blink {

class PLATFORM_EXPORT CustomCountHistogram {
public:
    CustomCountHistogram(const char* name, base::HistogramBase::Sample min, base::HistogramBase::Sample max, int32_t bucketCount);
    void count(base::HistogramBase::Sample);

protected:
    explicit CustomCountHistogram(base::HistogramBase*);

    base::HistogramBase* m_histogram;
};

class PLATFORM_EXPORT BooleanHistogram : public CustomCountHistogram {
public:
    BooleanHistogram(const char* name);
};

class PLATFORM_EXPORT EnumerationHistogram : public CustomCountHistogram {
public:
    EnumerationHistogram(const char* name, base::HistogramBase::Sample boundaryValue);
};

class PLATFORM_EXPORT SparseHistogram {
public:
    explicit SparseHistogram(const char* name);

    void sample(base::HistogramBase::Sample);

private:
    base::HistogramBase* m_histogram;
};



class PLATFORM_EXPORT ScopedUsHistogramTimer {
public:
    ScopedUsHistogramTimer(CustomCountHistogram& counter)
        : m_startTime(WTF::monotonicallyIncreasingTime()),
        m_counter(counter) {}

    ~ScopedUsHistogramTimer()
    {
        m_counter.count((WTF::monotonicallyIncreasingTime()  - m_startTime) * base::Time::kMicrosecondsPerSecond);
    }

private:
    // In seconds.
    double m_startTime;
    CustomCountHistogram& m_counter;
};

// Use code like this to record time, in microseconds, to execute a block of code:
//
// {
//     SCOPED_BLINK_UMA_HISTOGRAM_TIMER(myUmaStatName)
//     RunMyCode();
// }
// This macro records all times between 0us and 10 seconds.
// Do not change this macro without renaming all metrics that use it!
#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER(name) \
DEFINE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounter, (name, 0, 10000000, 50)); \
ScopedUsHistogramTimer timer(scopedUsCounter);

} // namespace blink

#endif // Histogram_h
