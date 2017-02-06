/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "core/fetch/ScriptResource.h"

#include "core/fetch/FetchRequest.h"
#include "core/fetch/IntegrityMetadata.h"
#include "core/fetch/ResourceClientOrObserverWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/SharedBuffer.h"
#include "platform/web_memory_allocator_dump.h"
#include "platform/web_process_memory_dump.h"

namespace blink {

ScriptResource* ScriptResource::fetch(FetchRequest& request, ResourceFetcher* fetcher)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextScript);
    ScriptResource* resource = toScriptResource(fetcher->requestResource(request, ScriptResourceFactory()));
    if (resource && !request.integrityMetadata().isEmpty())
        resource->setIntegrityMetadata(request.integrityMetadata());
    return resource;
}

ScriptResource::ScriptResource(const ResourceRequest& resourceRequest, const ResourceLoaderOptions& options, const String& charset)
    : TextResource(resourceRequest, Script, options, "application/javascript", charset)
    , m_integrityDisposition(ScriptIntegrityDisposition::NotChecked)
{
}

ScriptResource::~ScriptResource()
{
}

void ScriptResource::didAddClient(ResourceClient* client)
{
    ASSERT(ScriptResourceClient::isExpectedType(client));
    Resource::didAddClient(client);
}

void ScriptResource::appendData(const char* data, size_t length)
{
    Resource::appendData(data, length);
    ResourceClientWalker<ScriptResourceClient> walker(m_clients);
    while (ScriptResourceClient* client = walker.next())
        client->notifyAppendData(this);
}

void ScriptResource::onMemoryDump(WebMemoryDumpLevelOfDetail levelOfDetail, WebProcessMemoryDump* memoryDump) const
{
    Resource::onMemoryDump(levelOfDetail, memoryDump);
    const String name = getMemoryDumpName() + "/decoded_script";
    auto dump = memoryDump->createMemoryAllocatorDump(name);
    dump->addScalar("size", "bytes", m_script.currentSizeInBytes());
    memoryDump->addSuballocation(dump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

const CompressibleString& ScriptResource::script()
{
    ASSERT(!isPurgeable());
    ASSERT(isLoaded());

    if (m_script.isNull() && m_data) {
        String script = decodedText();
        m_data.clear();
        // We lie a it here and claim that script counts as encoded data (even though it's really decoded data).
        // That's because the MemoryCache thinks that it can clear out decoded data by calling destroyDecodedData(),
        // but we can't destroy script in destroyDecodedData because that's our only copy of the data!
        setEncodedSize(script.sizeInBytes());
        m_script = CompressibleString(script.impl());
    }

    return m_script;
}

void ScriptResource::destroyDecodedDataForFailedRevalidation()
{
    m_script = CompressibleString();
}

bool ScriptResource::mimeTypeAllowedByNosniff() const
{
    return parseContentTypeOptionsHeader(m_response.httpHeaderField(HTTPNames::X_Content_Type_Options)) != ContentTypeOptionsNosniff || MIMETypeRegistry::isSupportedJavaScriptMIMEType(httpContentType());
}

void ScriptResource::setIntegrityDisposition(ScriptIntegrityDisposition disposition)
{
    ASSERT(disposition != ScriptIntegrityDisposition::NotChecked);
    m_integrityDisposition = disposition;
}
bool ScriptResource::mustRefetchDueToIntegrityMetadata(const FetchRequest& request) const
{
    if (request.integrityMetadata().isEmpty())
        return false;

    return !IntegrityMetadata::setsEqual(m_integrityMetadata, request.integrityMetadata());
}

} // namespace blink
