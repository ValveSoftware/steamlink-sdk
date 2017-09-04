/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"

#include "core/dom/ExecutionContext.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "platform/Supplementable.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

DOMFileSystem* DraggedIsolatedFileSystemImpl::getDOMFileSystem(
    DataObject* host,
    ExecutionContext* executionContext,
    const DataObjectItem& item) {
  if (!item.hasFileSystemId())
    return nullptr;
  const String fileSystemId = item.fileSystemId();
  DraggedIsolatedFileSystemImpl* draggedIsolatedFileSystem = from(host);
  if (!draggedIsolatedFileSystem)
    return nullptr;
  auto it = draggedIsolatedFileSystem->m_filesystems.find(fileSystemId);
  if (it != draggedIsolatedFileSystem->m_filesystems.end())
    return it->value;
  return draggedIsolatedFileSystem->m_filesystems
      .add(fileSystemId, DOMFileSystem::createIsolatedFileSystem(
                             executionContext, fileSystemId))
      .storedValue->value;
}

// static
const char* DraggedIsolatedFileSystemImpl::supplementName() {
  ASSERT(isMainThread());
  return "DraggedIsolatedFileSystemImpl";
}

DraggedIsolatedFileSystemImpl* DraggedIsolatedFileSystemImpl::from(
    DataObject* dataObject) {
  return static_cast<DraggedIsolatedFileSystemImpl*>(
      Supplement<DataObject>::from(dataObject, supplementName()));
}

DEFINE_TRACE(DraggedIsolatedFileSystemImpl) {
  visitor->trace(m_filesystems);
  Supplement<DataObject>::trace(visitor);
}

void DraggedIsolatedFileSystemImpl::prepareForDataObject(
    DataObject* dataObject) {
  DraggedIsolatedFileSystemImpl* fileSystem =
      new DraggedIsolatedFileSystemImpl();
  DraggedIsolatedFileSystemImpl::provideTo(
      *dataObject, DraggedIsolatedFileSystemImpl::supplementName(), fileSystem);
}

}  // namespace blink
