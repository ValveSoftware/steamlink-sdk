/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ProcessingInstruction_h
#define ProcessingInstruction_h

#include "core/dom/CharacterData.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/StyleSheetResource.h"
#include "core/fetch/StyleSheetResourceClient.h"

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction FINAL : public CharacterData, private ResourceOwner<StyleSheetResource> {
public:
    static PassRefPtrWillBeRawPtr<ProcessingInstruction> create(Document&, const String& target, const String& data);
    virtual ~ProcessingInstruction();
    virtual void trace(Visitor*) OVERRIDE;

    const String& target() const { return m_target; }

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    const String& localHref() const { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }
    void setCSSStyleSheet(PassRefPtrWillBeRawPtr<CSSStyleSheet>);

    bool isCSS() const { return m_isCSS; }
    bool isXSL() const { return m_isXSL; }

    void didAttributeChanged();
    bool isLoading() const;

private:
    ProcessingInstruction(Document&, const String& target, const String& data);

    virtual String nodeName() const OVERRIDE;
    virtual NodeType nodeType() const OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<Node> cloneNode(bool deep = true) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    bool checkStyleSheet(String& href, String& charset);
    void process(const String& href, const String& charset);

    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*) OVERRIDE;
    virtual void setXSLStyleSheet(const String& href, const KURL& baseURL, const String& sheet) OVERRIDE;

    virtual bool sheetLoaded() OVERRIDE;

    void parseStyleSheet(const String& sheet);
    void clearSheet();

    String m_target;
    String m_localHref;
    String m_title;
    String m_media;
    RefPtrWillBeMember<StyleSheet> m_sheet;
    bool m_loading;
    bool m_alternate;
    bool m_createdByParser;
    bool m_isCSS;
    bool m_isXSL;
};

DEFINE_NODE_TYPE_CASTS(ProcessingInstruction, nodeType() == Node::PROCESSING_INSTRUCTION_NODE);

inline bool isXSLStyleSheet(const Node& node)
{
    return node.nodeType() == Node::PROCESSING_INSTRUCTION_NODE && toProcessingInstruction(node).isXSL();
}

} // namespace WebCore

#endif
