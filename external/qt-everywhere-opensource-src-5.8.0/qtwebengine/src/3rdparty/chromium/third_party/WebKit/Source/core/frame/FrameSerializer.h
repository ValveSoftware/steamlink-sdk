/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * contributors may be used to endorse or promo te products derived from
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

#ifndef FrameSerializer_h
#define FrameSerializer_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class Attribute;
class CSSRule;
class CSSStyleSheet;
class CSSValue;
class Document;
class Element;
class FontResource;
class ImageResource;
class LocalFrame;
class Node;
class LayoutObject;
class Resource;
class SharedBuffer;
class StylePropertySet;

struct SerializedResource;

// This class is used to serialize frame's contents back to text (typically HTML).
// It serializes frame's document and resources such as images and CSS stylesheets.
class CORE_EXPORT FrameSerializer final {
    STACK_ALLOCATED();
public:
    class Delegate {
    public:
        // Controls whether HTML serialization should skip the given attribute.
        virtual bool shouldIgnoreAttribute(const Attribute&)
        {
            return false;
        }

        // Method allowing the Delegate control which URLs are written into the
        // generated html document.
        //
        // When URL of the element needs to be rewritten, this method should
        // return true and populate |rewrittenLink| with a desired value of the
        // html attribute value to be used in place of the original link.
        // (i.e. in place of img.src or iframe.src or object.data).
        //
        // If no link rewriting is desired, this method should return false.
        virtual bool rewriteLink(const Element&, String& rewrittenLink)
        {
            return false;
        }

        // Tells whether to skip serialization of a subresource or CSSStyleSheet
        // with a given URI. Used to deduplicate resources across multiple frames.
        virtual bool shouldSkipResourceWithURL(const KURL&)
        {
            return false;
        }

        // Tells whether to skip serialization of a subresource.
        virtual bool shouldSkipResource(const Resource&)
        {
            return false;
        }
    };

    // Constructs a serializer that will write output to the given vector of
    // SerializedResources and uses the Delegate for controlling some
    // serialization aspects.  Callers need to ensure that both arguments stay
    // alive until the FrameSerializer gets destroyed.
    FrameSerializer(Vector<SerializedResource>&, Delegate&);

    // Initiates the serialization of the frame. All serialized content and
    // retrieved resources are added to the Vector passed to the constructor.
    // The first resource in that vector is the frame's serialized content.
    // Subsequent resources are images, css, etc.
    void serializeFrame(const LocalFrame&);

    static String markOfTheWebDeclaration(const KURL&);

private:
    // Serializes the stylesheet back to text and adds it to the resources if URL is not-empty.
    // It also adds any resources included in that stylesheet (including any imported stylesheets and their own resources).
    void serializeCSSStyleSheet(CSSStyleSheet&, const KURL&);

    // Serializes the css rule (including any imported stylesheets), adding referenced resources.
    void serializeCSSRule(CSSRule*);

    bool shouldAddURL(const KURL&);

    void addToResources(const Resource&, PassRefPtr<SharedBuffer>, const KURL&);
    void addImageToResources(ImageResource*, const KURL&);
    void addFontToResources(FontResource*);

    void retrieveResourcesForProperties(const StylePropertySet*, Document&);
    void retrieveResourcesForCSSValue(const CSSValue&, Document&);

    Vector<SerializedResource>* m_resources;
    HashSet<KURL> m_resourceURLs;

    Delegate& m_delegate;
};

} // namespace blink

#endif // FrameSerializer_h
