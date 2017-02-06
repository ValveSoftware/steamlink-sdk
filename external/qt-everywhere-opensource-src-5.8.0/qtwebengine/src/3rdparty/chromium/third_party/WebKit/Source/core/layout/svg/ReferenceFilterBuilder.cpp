/*
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/svg/ReferenceFilterBuilder.h"

#include "core/dom/Element.h"
#include "core/fetch/DocumentResource.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGFilterElement.h"
#include "platform/graphics/filters/FilterOperation.h"

namespace blink {

namespace {

using ResourceReferenceMap = PersistentHeapHashMap<WeakMember<const FilterOperation>, Member<DocumentResourceReference>>;

ResourceReferenceMap& documentResourceReferences()
{
    DEFINE_STATIC_LOCAL(ResourceReferenceMap, documentResourceReferences, ());
    return documentResourceReferences;
}

} // namespace

DocumentResourceReference* ReferenceFilterBuilder::documentResourceReference(const FilterOperation* filterOperation)
{
    return documentResourceReferences().get(filterOperation);
}

void ReferenceFilterBuilder::setDocumentResourceReference(const FilterOperation* filterOperation, DocumentResourceReference* documentResourceReference)
{
    ASSERT(!documentResourceReferences().contains(filterOperation));
    documentResourceReferences().add(filterOperation, documentResourceReference);
}

SVGFilterElement* ReferenceFilterBuilder::resolveFilterReference(const ReferenceFilterOperation& filterOperation, Element& element)
{
    TreeScope* treeScope = &element.treeScope();

    if (DocumentResourceReference* documentResourceRef = documentResourceReference(&filterOperation)) {
        // If we have an SVG document, this is an external reference. Otherwise
        // we look up the referenced node in the current document.
        if (DocumentResource* cachedSVGDocument = documentResourceRef->document())
            treeScope = cachedSVGDocument->document();
    }

    if (!treeScope)
        return nullptr;

    Element* filter = treeScope->getElementById(filterOperation.fragment());

    if (!filter) {
        // Although we did not find the referenced filter, it might exist later
        // in the document.
        treeScope->document().accessSVGExtensions().addPendingResource(filterOperation.fragment(), &element);
        return nullptr;
    }

    if (!isSVGFilterElement(*filter))
        return nullptr;

    return toSVGFilterElement(filter);
}

} // namespace blink
