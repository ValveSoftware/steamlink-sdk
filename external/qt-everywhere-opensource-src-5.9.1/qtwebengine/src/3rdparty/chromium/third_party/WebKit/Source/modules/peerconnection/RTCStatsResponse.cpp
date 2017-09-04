/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/peerconnection/RTCStatsResponse.h"

namespace blink {

RTCStatsResponse* RTCStatsResponse::create() {
  return new RTCStatsResponse();
}

RTCStatsResponse::RTCStatsResponse() {}

RTCLegacyStatsReport* RTCStatsResponse::namedItem(const AtomicString& name) {
  if (m_idmap.find(name) != m_idmap.end())
    return m_result[m_idmap.get(name)];
  return nullptr;
}

void RTCStatsResponse::addStats(const WebRTCLegacyStats& stats) {
  m_result.append(RTCLegacyStatsReport::create(stats.id(), stats.type(),
                                               stats.timestamp()));
  m_idmap.add(stats.id(), m_result.size() - 1);
  RTCLegacyStatsReport* report = m_result[m_result.size() - 1].get();

  for (std::unique_ptr<WebRTCLegacyStatsMemberIterator> member(
           stats.iterator());
       !member->isEnd(); member->next()) {
    report->addStatistic(member->name(), member->valueToString());
  }
}

DEFINE_TRACE(RTCStatsResponse) {
  visitor->trace(m_result);
  RTCStatsResponseBase::trace(visitor);
}

}  // namespace blink
