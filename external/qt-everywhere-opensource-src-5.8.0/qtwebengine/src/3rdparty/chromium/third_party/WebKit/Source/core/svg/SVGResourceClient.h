// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "core/fetch/DocumentResource.h"
#include "core/svg/SVGFilterElement.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class FilterOperations;
class SVGFilterElement;

class CORE_EXPORT SVGResourceClient : public DocumentResourceClient {
    USING_PRE_FINALIZER(SVGResourceClient, clearFilterReferences);
public:
    SVGResourceClient();
    ~SVGResourceClient() override;
    void addFilterReferences(const FilterOperations&, const Document&);
    void clearFilterReferences();

    virtual void filterNeedsInvalidation() = 0;

    void filterWillBeDestroyed(SVGFilterElement*);

    void notifyFinished(Resource*) override;
    String debugName() const override { return "SVGResourceClient"; }

    DECLARE_TRACE();

private:
    HeapHashSet<WeakMember<SVGFilterElement>> m_internalFilterReferences;
    HeapVector<Member<DocumentResource>> m_externalFilterReferences;
};

} // namespace blink

#endif // SVGResourceClient_h
