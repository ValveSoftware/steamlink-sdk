/*
 *  Copyright (C) 2003, 2006 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *  Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "core/xml/XMLSerializer.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/editing/MarkupAccumulator.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

String XMLSerializer::serializeToString(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwTypeError("Invalid node value.");
        return String();
    }

    MarkupAccumulator accumulator(0, DoNotResolveURLs, nullptr, ForcedXML);
    return accumulator.serializeNodes(*node, IncludeNode);
}

} // namespace WebCore
