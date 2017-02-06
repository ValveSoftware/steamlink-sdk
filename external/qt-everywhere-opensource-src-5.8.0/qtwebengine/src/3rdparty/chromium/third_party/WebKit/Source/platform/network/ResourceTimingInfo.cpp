// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceTimingInfo.h"

#include "platform/CrossThreadCopier.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<ResourceTimingInfo> ResourceTimingInfo::adopt(std::unique_ptr<CrossThreadResourceTimingInfoData> data)
{
    std::unique_ptr<ResourceTimingInfo> info = ResourceTimingInfo::create(AtomicString(data->m_type), data->m_initialTime, data->m_isMainResource);
    info->m_originalTimingAllowOrigin = AtomicString(data->m_originalTimingAllowOrigin);
    info->m_loadFinishTime = data->m_loadFinishTime;
    info->m_initialRequest = ResourceRequest(data->m_initialRequest.get());
    info->m_finalResponse = ResourceResponse(data->m_finalResponse.get());
    for (auto& responseData : data->m_redirectChain)
        info->m_redirectChain.append(ResourceResponse(responseData.get()));
    return info;
}

std::unique_ptr<CrossThreadResourceTimingInfoData> ResourceTimingInfo::copyData() const
{
    std::unique_ptr<CrossThreadResourceTimingInfoData> data = wrapUnique(new CrossThreadResourceTimingInfoData);
    data->m_type = m_type.getString().isolatedCopy();
    data->m_originalTimingAllowOrigin = m_originalTimingAllowOrigin.getString().isolatedCopy();
    data->m_initialTime = m_initialTime;
    data->m_loadFinishTime = m_loadFinishTime;
    data->m_initialRequest = m_initialRequest.copyData();
    data->m_finalResponse = m_finalResponse.copyData();
    for (const auto& response : m_redirectChain)
        data->m_redirectChain.append(response.copyData());
    data->m_isMainResource = m_isMainResource;
    return data;
}

} // namespace blink
