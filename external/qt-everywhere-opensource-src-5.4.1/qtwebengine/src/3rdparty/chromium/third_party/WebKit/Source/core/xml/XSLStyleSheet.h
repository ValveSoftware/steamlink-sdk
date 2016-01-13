/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef XSLStyleSheet_h
#define XSLStyleSheet_h

#include "core/css/StyleSheet.h"
#include "core/dom/ProcessingInstruction.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/PassRefPtr.h"
#include <libxml/tree.h>
#include <libxslt/transform.h>

namespace WebCore {

class ResourceFetcher;
class XSLImportRule;

class XSLStyleSheet FINAL : public StyleSheet {
public:
    static PassRefPtrWillBeRawPtr<XSLStyleSheet> create(XSLImportRule* parentImport, const String& originalURL, const KURL& finalURL)
    {
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        return adoptRefWillBeNoop(new XSLStyleSheet(parentImport, originalURL, finalURL));
    }
    static PassRefPtrWillBeRawPtr<XSLStyleSheet> create(ProcessingInstruction* parentNode, const String& originalURL, const KURL& finalURL)
    {
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        return adoptRefWillBeNoop(new XSLStyleSheet(parentNode, originalURL, finalURL, false));
    }
    static PassRefPtrWillBeRawPtr<XSLStyleSheet> createEmbedded(ProcessingInstruction* parentNode, const KURL& finalURL)
    {
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        return adoptRefWillBeNoop(new XSLStyleSheet(parentNode, finalURL.string(), finalURL, true));
    }

    // Taking an arbitrary node is unsafe, because owner node pointer can become
    // stale. XSLTProcessor ensures that the stylesheet doesn't outlive its
    // parent, in part by not exposing it to JavaScript.
    static PassRefPtrWillBeRawPtr<XSLStyleSheet> createForXSLTProcessor(Node* parentNode, const String& originalURL, const KURL& finalURL)
    {
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        return adoptRefWillBeNoop(new XSLStyleSheet(parentNode, originalURL, finalURL, false));
    }

    virtual ~XSLStyleSheet();

    bool parseString(const String&);

    void checkLoaded();

    const KURL& finalURL() const { return m_finalURL; }

    void loadChildSheets();
    void loadChildSheet(const String& href);

    ResourceFetcher* fetcher();

    Document* ownerDocument();
    virtual XSLStyleSheet* parentStyleSheet() const OVERRIDE { return m_parentStyleSheet; }
    void setParentStyleSheet(XSLStyleSheet*);

    xmlDocPtr document();
    xsltStylesheetPtr compileStyleSheet();
    xmlDocPtr locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri);

    void clearDocuments();

    void markAsProcessed();
    bool processed() const { return m_processed; }

    virtual String type() const OVERRIDE { return "text/xml"; }
    virtual bool disabled() const OVERRIDE { return m_isDisabled; }
    virtual void setDisabled(bool b) OVERRIDE { m_isDisabled = b; }
    virtual Node* ownerNode() const OVERRIDE { return m_ownerNode; }
    virtual String href() const OVERRIDE { return m_originalURL; }
    virtual String title() const OVERRIDE { return emptyString(); }

    virtual void clearOwnerNode() OVERRIDE { m_ownerNode = nullptr; }
    virtual KURL baseURL() const OVERRIDE { return m_finalURL; }
    virtual bool isLoading() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    XSLStyleSheet(Node* parentNode, const String& originalURL, const KURL& finalURL, bool embedded);
    XSLStyleSheet(XSLImportRule* parentImport, const String& originalURL, const KURL& finalURL);

    RawPtrWillBeMember<Node> m_ownerNode;
    String m_originalURL;
    KURL m_finalURL;
    bool m_isDisabled;

    WillBeHeapVector<OwnPtrWillBeMember<XSLImportRule> > m_children;

    bool m_embedded;
    bool m_processed;

    xmlDocPtr m_stylesheetDoc;
    bool m_stylesheetDocTaken;
    bool m_compilationFailed;

    RawPtrWillBeMember<XSLStyleSheet> m_parentStyleSheet;
};

DEFINE_TYPE_CASTS(XSLStyleSheet, StyleSheet, sheet, !sheet->isCSSStyleSheet(), !sheet.isCSSStyleSheet());

} // namespace WebCore

#endif // XSLStyleSheet_h
