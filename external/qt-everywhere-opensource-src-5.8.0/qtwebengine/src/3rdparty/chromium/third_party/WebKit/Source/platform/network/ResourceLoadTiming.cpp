// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceLoadTiming.h"

#include "platform/TraceEvent.h"

namespace blink {

ResourceLoadTiming::ResourceLoadTiming()
    : m_requestTime(0)
    , m_proxyStart(0)
    , m_proxyEnd(0)
    , m_dnsStart(0)
    , m_dnsEnd(0)
    , m_connectStart(0)
    , m_connectEnd(0)
    , m_workerStart(0)
    , m_workerReady(0)
    , m_sendStart(0)
    , m_sendEnd(0)
    , m_receiveHeadersEnd(0)
    , m_sslStart(0)
    , m_sslEnd(0)
    , m_pushStart(0)
    , m_pushEnd(0)
{
}

PassRefPtr<ResourceLoadTiming> ResourceLoadTiming::create()
{
    return adoptRef(new ResourceLoadTiming);
}

PassRefPtr<ResourceLoadTiming> ResourceLoadTiming::deepCopy()
{
    RefPtr<ResourceLoadTiming> timing = create();
    timing->m_requestTime = m_requestTime;
    timing->m_proxyStart = m_proxyStart;
    timing->m_proxyEnd = m_proxyEnd;
    timing->m_dnsStart = m_dnsStart;
    timing->m_dnsEnd = m_dnsEnd;
    timing->m_connectStart = m_connectStart;
    timing->m_connectEnd = m_connectEnd;
    timing->m_workerStart = m_workerStart;
    timing->m_workerReady = m_workerReady;
    timing->m_sendStart = m_sendStart;
    timing->m_sendEnd = m_sendEnd;
    timing->m_receiveHeadersEnd = m_receiveHeadersEnd;
    timing->m_sslStart = m_sslStart;
    timing->m_sslEnd = m_sslEnd;
    timing->m_pushStart = m_pushStart;
    timing->m_pushEnd = m_pushEnd;
    return timing.release();
}

bool ResourceLoadTiming::operator==(const ResourceLoadTiming& other) const
{
    return m_requestTime == other.m_requestTime
        && m_proxyStart == other.m_proxyStart
        && m_proxyEnd == other.m_proxyEnd
        && m_dnsStart == other.m_dnsStart
        && m_dnsEnd == other.m_dnsEnd
        && m_connectStart == other.m_connectStart
        && m_connectEnd == other.m_connectEnd
        && m_workerStart == other.m_workerStart
        && m_workerReady == other.m_workerReady
        && m_sendStart == other.m_sendStart
        && m_sendEnd == other.m_sendEnd
        && m_receiveHeadersEnd == other.m_receiveHeadersEnd
        && m_sslStart == other.m_sslStart
        && m_sslEnd == other.m_sslEnd
        && m_pushStart == other.m_pushStart
        && m_pushEnd == other.m_pushEnd;
}

bool ResourceLoadTiming::operator!=(const ResourceLoadTiming& other) const
{
    return !(*this == other);
}

void ResourceLoadTiming::setDnsStart(double dnsStart)
{
    m_dnsStart = dnsStart;
}

void ResourceLoadTiming::setRequestTime(double requestTime)
{
    m_requestTime = requestTime;
}

void ResourceLoadTiming::setProxyStart(double proxyStart)
{
    m_proxyStart = proxyStart;
}

void ResourceLoadTiming::setProxyEnd(double proxyEnd)
{
    m_proxyEnd = proxyEnd;
}

void ResourceLoadTiming::setDnsEnd(double dnsEnd)
{
    m_dnsEnd = dnsEnd;
}

void ResourceLoadTiming::setConnectStart(double connectStart)
{
    m_connectStart = connectStart;
}

void ResourceLoadTiming::setConnectEnd(double connectEnd)
{
    m_connectEnd = connectEnd;
}

void ResourceLoadTiming::setWorkerStart(double workerStart)
{
    m_workerStart = workerStart;
}

void ResourceLoadTiming::setWorkerReady(double workerReady)
{
    m_workerReady = workerReady;
}

void ResourceLoadTiming::setSendStart(double sendStart)
{
    TRACE_EVENT_MARK_WITH_TIMESTAMP0("blink.user_timing", "requestStart", sendStart);
    m_sendStart = sendStart;
}

void ResourceLoadTiming::setSendEnd(double sendEnd)
{
    m_sendEnd = sendEnd;
}

void ResourceLoadTiming::setReceiveHeadersEnd(double receiveHeadersEnd)
{
    m_receiveHeadersEnd = receiveHeadersEnd;
}

void ResourceLoadTiming::setSslStart(double sslStart)
{
    m_sslStart = sslStart;
}

void ResourceLoadTiming::setSslEnd(double sslEnd)
{
    m_sslEnd = sslEnd;
}

void ResourceLoadTiming::setPushStart(double pushStart)
{
    m_pushStart = pushStart;
}

void ResourceLoadTiming::setPushEnd(double pushEnd)
{
    m_pushEnd = pushEnd;
}

double ResourceLoadTiming::calculateMillisecondDelta(double time) const
{
    return time ? (time - m_requestTime) * 1000 : -1;
}

} // namespace blink
