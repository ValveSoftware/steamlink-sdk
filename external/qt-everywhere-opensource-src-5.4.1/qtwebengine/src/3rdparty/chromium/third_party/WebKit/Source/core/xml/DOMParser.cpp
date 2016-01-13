/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
#include "core/xml/DOMParser.h"

#include "core/dom/DOMImplementation.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<Document> DOMParser::parseFromString(const String& str, const String& contentType, ExceptionState& exceptionState)
{
    // HTML5 is very explicit about which types we're allowed to support here:
    // http://domparsing.spec.whatwg.org/#the-domparser-interface
    if (contentType != "text/html"
        && contentType != "text/xml"
        && contentType != "application/xml"
        && contentType != "application/xhtml+xml"
        && contentType != "image/svg+xml") {
        exceptionState.throwTypeError("Unsupported mime-type specified.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<Document> doc = DOMImplementation::createDocument(contentType, 0, KURL(), false);
    doc->setContent(str);
    return doc.release();
}

} // namespace WebCore
