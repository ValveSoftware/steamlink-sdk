/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef HTMLImportsController_h
#define HTMLImportsController_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Document.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class FetchRequest;
class HTMLImport;
class HTMLImportChild;
class HTMLImportChildClient;
class HTMLImportLoader;
class HTMLImportTreeRoot;
class KURL;

class HTMLImportsController final
    : public GarbageCollected<HTMLImportsController>,
      public TraceWrapperBase {
 public:
  static HTMLImportsController* create(Document& master) {
    return new HTMLImportsController(master);
  }

  HTMLImportTreeRoot* root() const { return m_root; }

  bool shouldBlockScriptExecution(const Document&) const;
  HTMLImportChild* load(HTMLImport* parent,
                        HTMLImportChildClient*,
                        FetchRequest);

  Document* master() const;

  HTMLImportLoader* createLoader();

  size_t loaderCount() const { return m_loaders.size(); }
  HTMLImportLoader* loaderAt(size_t i) const { return m_loaders[i]; }
  HTMLImportLoader* loaderFor(const Document&) const;

  DECLARE_TRACE();

  void dispose();

  DECLARE_TRACE_WRAPPERS();

 private:
  explicit HTMLImportsController(Document&);

  HTMLImportChild* createChild(const KURL&,
                               HTMLImportLoader*,
                               HTMLImport* parent,
                               HTMLImportChildClient*);

  Member<HTMLImportTreeRoot> m_root;
  using LoaderList = HeapVector<Member<HTMLImportLoader>>;
  LoaderList m_loaders;
};

}  // namespace blink

#endif  // HTMLImportsController_h
