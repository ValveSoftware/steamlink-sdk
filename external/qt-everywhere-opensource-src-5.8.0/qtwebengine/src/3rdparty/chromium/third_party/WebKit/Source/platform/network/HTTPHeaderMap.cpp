/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "platform/network/HTTPHeaderMap.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

HTTPHeaderMap::HTTPHeaderMap()
{
}

HTTPHeaderMap::~HTTPHeaderMap()
{
}

std::unique_ptr<CrossThreadHTTPHeaderMapData> HTTPHeaderMap::copyData() const
{
    std::unique_ptr<CrossThreadHTTPHeaderMapData> data = wrapUnique(new CrossThreadHTTPHeaderMapData());
    data->reserveInitialCapacity(size());

    HTTPHeaderMap::const_iterator endIt = end();
    for (HTTPHeaderMap::const_iterator it = begin(); it != endIt; ++it)
        data->uncheckedAppend(std::make_pair(it->key.getString().isolatedCopy(), it->value.getString().isolatedCopy()));

    return data;
}

void HTTPHeaderMap::adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData> data)
{
    clear();
    size_t dataSize = data->size();
    for (size_t index = 0; index < dataSize; ++index) {
        std::pair<String, String>& header = (*data)[index];
        set(AtomicString(header.first), AtomicString(header.second));
    }
}

} // namespace blink
