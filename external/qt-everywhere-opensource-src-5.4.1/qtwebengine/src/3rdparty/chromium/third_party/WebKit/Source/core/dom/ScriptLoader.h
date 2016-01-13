/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef ScriptLoader_h
#define ScriptLoader_h

#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourcePtr.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ScriptResource;
class ContainerNode;
class Element;
class ScriptLoaderClient;
class ScriptSourceCode;


class ScriptLoader FINAL : private ResourceClient {
public:
    static PassOwnPtr<ScriptLoader> create(Element*, bool createdByParser, bool isEvaluated);
    virtual ~ScriptLoader();

    Element* element() const { return m_element; }

    enum LegacyTypeSupport { DisallowLegacyTypeInTypeAttribute, AllowLegacyTypeInTypeAttribute };
    bool prepareScript(const TextPosition& scriptStartPosition = TextPosition::minimumPosition(), LegacyTypeSupport = DisallowLegacyTypeInTypeAttribute);

    String scriptCharset() const { return m_characterEncoding; }
    String scriptContent() const;
    void executeScript(const ScriptSourceCode&);
    void execute(ScriptResource*);

    // XML parser calls these
    void dispatchLoadEvent();
    void dispatchErrorEvent();
    bool isScriptTypeSupported(LegacyTypeSupport) const;

    bool haveFiredLoadEvent() const { return m_haveFiredLoad; }
    bool willBeParserExecuted() const { return m_willBeParserExecuted; }
    bool readyToBeParserExecuted() const { return m_readyToBeParserExecuted; }
    bool willExecuteWhenDocumentFinishedParsing() const { return m_willExecuteWhenDocumentFinishedParsing; }
    ResourcePtr<ScriptResource> resource() { return m_resource; }

    void setHaveFiredLoadEvent(bool haveFiredLoad) { m_haveFiredLoad = haveFiredLoad; }
    bool isParserInserted() const { return m_parserInserted; }
    bool alreadyStarted() const { return m_alreadyStarted; }
    bool forceAsync() const { return m_forceAsync; }

    // Helper functions used by our parent classes.
    void didNotifySubtreeInsertionsToDocument();
    void childrenChanged();
    void handleSourceAttribute(const String& sourceUrl);
    void handleAsyncAttribute();

private:
    ScriptLoader(Element*, bool createdByParser, bool isEvaluated);

    bool ignoresLoadRequest() const;
    bool isScriptForEventSupported() const;

    bool fetchScript(const String& sourceUrl);
    void stopLoadRequest();

    ScriptLoaderClient* client() const;

    // ResourceClient
    virtual void notifyFinished(Resource*) OVERRIDE;

    Element* m_element;
    ResourcePtr<ScriptResource> m_resource;
    WTF::OrdinalNumber m_startLineNumber;
    bool m_parserInserted : 1;
    bool m_isExternalScript : 1;
    bool m_alreadyStarted : 1;
    bool m_haveFiredLoad : 1;
    bool m_willBeParserExecuted : 1; // Same as "The parser will handle executing the script."
    bool m_readyToBeParserExecuted : 1;
    bool m_willExecuteWhenDocumentFinishedParsing : 1;
    bool m_forceAsync : 1;
    bool m_willExecuteInOrder : 1;
    String m_characterEncoding;
    String m_fallbackCharacterEncoding;
};

ScriptLoader* toScriptLoaderIfPossible(Element*);

inline PassOwnPtr<ScriptLoader> ScriptLoader::create(Element* element, bool createdByParser, bool isEvaluated)
{
    return adoptPtr(new ScriptLoader(element, createdByParser, isEvaluated));
}

}


#endif
