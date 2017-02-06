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

#ifndef WebNode_h
#define WebNode_h

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"
#include "../platform/WebString.h"
#include "../platform/WebVector.h"

namespace blink {

class Node;
class WebAXObject;
class WebDocument;
class WebElement;
class WebElementCollection;
class WebPluginContainer;

// Provides access to some properties of a DOM node.
// Note that the class design requires that neither this class nor any of its subclasses have any virtual
// methods (other than the destructor), so that it is possible to safely static_cast an instance of one
// class to the appropriate subclass based on the actual type of the wrapped blink::Node. For the same
// reason, subclasses must not add any additional data members.
class WebNode {
public:
    virtual ~WebNode() { reset(); }

    WebNode() { }
    WebNode(const WebNode& n) { assign(n); }
    WebNode& operator=(const WebNode& n)
    {
        assign(n);
        return *this;
    }

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebNode&);

    BLINK_EXPORT bool equals(const WebNode&) const;
    // Required for using WebNodes in std maps.  Note the order used is
    // arbitrary and should not be expected to have any specific meaning.
    BLINK_EXPORT bool lessThan(const WebNode&) const;

    bool isNull() const { return m_private.isNull(); }

    BLINK_EXPORT WebNode parentNode() const;
    BLINK_EXPORT WebString nodeValue() const;
    BLINK_EXPORT WebDocument document() const;
    BLINK_EXPORT WebNode firstChild() const;
    BLINK_EXPORT WebNode lastChild() const;
    BLINK_EXPORT WebNode previousSibling() const;
    BLINK_EXPORT WebNode nextSibling() const;
    BLINK_EXPORT bool hasChildNodes() const;
    BLINK_EXPORT bool isLink() const;
    BLINK_EXPORT bool isDocumentNode() const;
    BLINK_EXPORT bool isDocumentTypeNode() const;
    BLINK_EXPORT bool isCommentNode() const;
    BLINK_EXPORT bool isTextNode() const;
    BLINK_EXPORT bool isFocusable() const;
    BLINK_EXPORT bool isContentEditable() const;
    BLINK_EXPORT bool isElementNode() const;
    BLINK_EXPORT void simulateClick();

    // The argument should be lower-cased.
    BLINK_EXPORT WebElementCollection getElementsByHTMLTagName(const WebString&) const;

    // https://developer.mozilla.org/en-US/docs/Web/API/Document/querySelector
    // If the JS API would have thrown this returns null instead.
    BLINK_EXPORT WebElement querySelector(const WebString& selector) const;

    BLINK_EXPORT bool focused() const;

    BLINK_EXPORT WebPluginContainer* pluginContainer() const;

    BLINK_EXPORT bool isInsideFocusableElementOrARIAWidget() const;
    BLINK_EXPORT WebAXObject accessibilityObject();

    template<typename T> T to();
    template<typename T> const T toConst() const;

#if BLINK_IMPLEMENTATION
    BLINK_EXPORT static WebPluginContainer* pluginContainerFromNode(const Node*);

    BLINK_EXPORT WebNode(Node*);
    BLINK_EXPORT WebNode& operator=(Node*);
    BLINK_EXPORT operator Node*() const;

    template<typename T> T* unwrap()
    {
        return static_cast<T*>(m_private.get());
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private.get());
    }
#endif

protected:
    WebPrivatePtr<Node> m_private;
};

#define DECLARE_WEB_NODE_TYPE_CASTS(type) \
template<> \
BLINK_EXPORT type WebNode::to<type>(); \
template<> \
BLINK_EXPORT const type WebNode::toConst<type>() const;

#if BLINK_IMPLEMENTATION
#define DEFINE_WEB_NODE_TYPE_CASTS(type, predicate) \
template<> \
type WebNode::to<type>() { \
    ASSERT_WITH_SECURITY_IMPLICATION(isNull() || (predicate)); \
    type result; \
    result.WebNode::assign(*this); \
    return result; \
} \
template<> \
const type WebNode::toConst<type>() const { \
    ASSERT_WITH_SECURITY_IMPLICATION(isNull() || (predicate)); \
    type result; \
    result.WebNode::assign(*this); \
    return result; \
}
#endif

inline bool operator==(const WebNode& a, const WebNode& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebNode& a, const WebNode& b)
{
    return !(a == b);
}

inline bool operator<(const WebNode& a, const WebNode& b)
{
    return a.lessThan(b);
}

} // namespace blink

#endif
