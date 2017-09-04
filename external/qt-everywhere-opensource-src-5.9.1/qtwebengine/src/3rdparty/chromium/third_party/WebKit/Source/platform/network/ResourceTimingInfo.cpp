// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceTimingInfo.h"

#include "platform/CrossThreadCopier.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ResourceTimingInfo> ResourceTimingInfo::adopt(
    std::unique_ptr<CrossThreadResourceTimingInfoData> data) {
  std::unique_ptr<ResourceTimingInfo> info = ResourceTimingInfo::create(
      AtomicString(data->m_type), data->m_initialTime, data->m_isMainResource);
  info->m_originalTimingAllowOrigin =
      AtomicString(data->m_originalTimingAllowOrigin);
  info->m_loadFinishTime = data->m_loadFinishTime;
  info->m_initialURL = data->m_initialURL.copy();
  info->m_finalResponse = ResourceResponse(data->m_finalResponse.get());
  for (auto& responseData : data->m_redirectChain)
    info->m_redirectChain.append(ResourceResponse(responseData.get()));
  info->m_transferSize = data->m_transferSize;
  return info;
}

std::unique_ptr<CrossThreadResourceTimingInfoData>
ResourceTimingInfo::copyData() const {
  std::unique_ptr<CrossThreadResourceTimingInfoData> data =
      wrapUnique(new CrossThreadResourceTimingInfoData);
  data->m_type = m_type.getString().isolatedCopy();
  data->m_originalTimingAllowOrigin =
      m_originalTimingAllowOrigin.getString().isolatedCopy();
  data->m_initialTime = m_initialTime;
  data->m_loadFinishTime = m_loadFinishTime;
  data->m_initialURL = m_initialURL.copy();
  data->m_finalResponse = m_finalResponse.copyData();
  for (const auto& response : m_redirectChain)
    data->m_redirectChain.append(response.copyData());
  data->m_transferSize = m_transferSize;
  data->m_isMainResource = m_isMainResource;
  return data;
}

void ResourceTimingInfo::addRedirect(const ResourceResponse& redirectResponse,
                                     bool crossOrigin) {
  m_redirectChain.append(redirectResponse);
  if (m_hasCrossOriginRedirect)
    return;
  if (crossOrigin) {
    m_hasCrossOriginRedirect = true;
    m_transferSize = 0;
  } else {
    DCHECK_GE(redirectResponse.encodedDataLength(), 0);
    m_transferSize += redirectResponse.encodedDataLength();
  }
}

}  // namespace blink
