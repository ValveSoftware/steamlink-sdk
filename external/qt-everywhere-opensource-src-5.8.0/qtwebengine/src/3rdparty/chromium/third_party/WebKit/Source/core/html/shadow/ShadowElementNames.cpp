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

#include "core/html/shadow/ShadowElementNames.h"

namespace blink {

namespace ShadowElementNames {

const AtomicString& detailsContent()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("details-content"));
    return name;
}

const AtomicString& detailsSummary()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("details-summary"));
    return name;
}

const AtomicString& detailsMarker()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("details-marker"));
    return name;
}

const AtomicString& dateTimeEdit()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("date-time-edit"));
    return name;
}

const AtomicString& spinButton()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("spin"));
    return name;
}

const AtomicString& clearButton()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("clear"));
    return name;
}

const AtomicString& editingViewPort()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("editing-view-port"));
    return name;
}

const AtomicString& innerEditor()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("inner-editor"));
    return name;
}

const AtomicString& pickerIndicator()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("picker"));
    return name;
}

const AtomicString& placeholder()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("placeholder"));
    return name;
}

const AtomicString& searchDecoration()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("decoration"));
    return name;
}

const AtomicString& sliderThumb()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("thumb"));
    return name;
}

const AtomicString& sliderTrack()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("track"));
    return name;
}

const AtomicString& textFieldContainer()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("text-field-container"));
    return name;
}

const AtomicString& optGroupLabel()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("optgroup-label"));
    return name;
}

} // namespace ShadowElementNames

} // namespace blink
