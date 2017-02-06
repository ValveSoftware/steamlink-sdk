/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtDebug>

#include <private/qacceltreeresourceloader_p.h>
#include <private/qnetworkaccessdelegator_p.h>

#include "Global.h"
#include "TestBaseLine.h"
#include "TestGroup.h"

#include "TestSuiteHandler.h"

using namespace QPatternistSDK;

static QNetworkAccessManager *s_networkAccessManager = 0;

static void cleanupNetworkAccessManager()
{
    delete s_networkAccessManager;
    s_networkAccessManager = 0;

}
static QNetworkAccessManager *networkAccessManager()
{
    if (!s_networkAccessManager) {
        s_networkAccessManager = new QNetworkAccessManager;
        qAddPostRoutine(cleanupNetworkAccessManager);
    }
    return s_networkAccessManager;
}

TestSuiteHandler::TestSuiteHandler(const QUrl &catalogFile,
                                   const bool useEList) : m_ts(0)
                                                        , m_container(0)
                                                        , m_tc(0)
                                                        , m_baseLine(0)
                                                        , m_catalogFile(catalogFile)
                                                        , m_exclusionList(readExclusionList(useEList))
                                                        , m_isExcluding(false)
{
    Q_ASSERT(!m_catalogFile.isRelative());
}

QStringList TestSuiteHandler::readExclusionList(const bool useExclusionList) const
{
    if(!useExclusionList)
        return QStringList();

    QStringList avoid;

    /* These test groups are for features we don't support.
     *
     * Originally these were stored in a text file pulled in with Qt resources, but
     * it was not possible to get it to link on some HP-UX and Intel-icc platforms. */

    avoid << "SchemaImport";        // The schema import feature
    avoid << "SchemaValidation";    // The validate expression(requires schema import)
    avoid << "StaticTyping";        // Pessimistic static typing checking
    avoid << "TrivialEmbedding";    // XQueryX inside XQuery
    avoid << "XMark";               // We're currently too buggy for running these tests.

    return avoid;
}

bool TestSuiteHandler::startElement(const QString &namespaceURI,
                                    const QString &localName,
                                    const QString &/*qName*/,
                                    const QXmlAttributes &atts)
{
    if(namespaceURI != Global::xqtsCatalogNS)
        return true;
    else if(m_isExcluding)
    {
        if(localName == QLatin1String("test-group"))
        {
            m_testGroupName.push(atts.value(QLatin1String("name")));
            return true;
        }
        else
            return true;
    }

    /* The elements are handled roughly in the order of highest occurrence in the catalog file. */
    if(localName == QLatin1String("test-case"))
    {
        XQTSTestCase *const c = new XQTSTestCase(
                TestCase::scenarioFromString(atts.value(QLatin1String("scenario"))), m_container);

        c->setName(atts.value(QLatin1String("name")));
        c->setCreator(atts.value(QLatin1String("Creator")));
        c->setIsXPath(Global::readBoolean(atts.value(QLatin1String("is-XPath2"))));
        c->setLastModified(QDate::fromString(atts.value(QLatin1String("version-drop")), Qt::ISODate));
        Q_ASSERT(c->lastModified().isNull() || c->lastModified().isValid());

        m_currentQueryPath = m_queryOffset.resolved(QUrl(atts.value(QLatin1String("FilePath"))));
        m_currentBaselinePath = m_baselineOffset.resolved(QUrl(atts.value(QLatin1String("FilePath"))));

        m_container->appendChild(c);
        m_tc = c;
     }
    else if(localName == QLatin1String("query"))
    {
        m_tc->setQueryPath(m_currentQueryPath.resolved(atts.value(QLatin1String("name")) +
                                                       m_xqueryFileExtension));
    }
    else if(localName == QLatin1String("input-file") ||
            localName == QLatin1String("input-URI"))
    {
        m_currentInputVariable = atts.value(QLatin1String("variable"));
    }
    else if(localName == QLatin1String("output-file"))
    {
        m_baseLine = new TestBaseLine(TestBaseLine::identifierFromString(atts.value(QLatin1String("compare"))));
    }
    else if(localName == QLatin1String("expected-error"))
    {
        m_baseLine = new TestBaseLine(TestBaseLine::ExpectedError);
    }
    else if(localName == QLatin1String("test-group"))
    {
        m_testGroupName.push(atts.value(QLatin1String("name")));

        if(m_exclusionList.contains(m_testGroupName.top()))
        {
            /* Ok, this group is supposed to be excluded, we don't
             * insert it into the tree. */
            m_isExcluding = true;
            return true;
        }
        else
        {
            Q_ASSERT(m_container);
            TestGroup *const newGroup = new TestGroup(m_container);
            m_container->appendChild(newGroup);
            m_container = newGroup;
        }
    }
    else if(localName == QLatin1String("source"))
    {
        m_sourceMap.insert(atts.value(QLatin1String("ID")),
                           m_sourceOffset.resolved(QUrl(atts.value(QLatin1String("FileName")))));
    }
    else if(localName == QLatin1String("test-suite"))
    {
        m_ts = new TestSuite();
        m_ts->setVersion(atts.value(QLatin1String("version")));
        m_ts->setDesignDate(QDate::fromString(atts.value(QLatin1String("CatalogDesignDate")), Qt::ISODate));
        Q_ASSERT(m_ts->designDate().isValid());
        m_container = m_ts;

        m_xqueryFileExtension   = atts.value(QLatin1String("XQueryFileExtension"));
        m_queryOffset           = m_catalogFile.resolved(atts.value(QLatin1String("XQueryQueryOffsetPath")));
        m_baselineOffset        = m_catalogFile.resolved(atts.value(QLatin1String("ResultOffsetPath")));
        m_sourceOffset          = m_catalogFile.resolved(atts.value(QLatin1String("SourceOffsetPath")));
    }
    else if(localName == QLatin1String("input-query"))
    {
        m_tcSourceInputs.insert(atts.value(QLatin1String("variable")),
                                ExternalSourceLoader::VariableValue(m_currentQueryPath.resolved(atts.value(QLatin1String("name")) + m_xqueryFileExtension),
                                                                    ExternalSourceLoader::Query));
    }

    return true;
}

bool TestSuiteHandler::endElement(const QString &namespaceURI,
                                  const QString &localName,
                                  const QString &/*qName*/)
{
    if(namespaceURI != Global::xqtsCatalogNS)
        return true;

    if(m_isExcluding)
    {
        if(localName == QLatin1String("test-group"))
        {
            const QString myName(m_testGroupName.pop());

            if(m_exclusionList.contains(myName))
            {
                /* This test-group is being excluded and now we're exiting from it. */
                m_isExcluding = false;
            }
        }

        return true;
    }

    /* The elements are handled roughly in the order of highest occurrence in the catalog file. */
    if(localName == QLatin1String("description"))
    {
        if(m_tc)
        {
            /* We're inside a <test-case>, so the <description> belongs
             * to the test-case. */
            m_tc->setDescription(m_ch.simplified());
        }
        else
            m_container->setDescription(m_ch.simplified());
    }
    else if(localName == QLatin1String("test-case"))
    {
        Q_ASSERT(m_tc->baseLines().count() >= 1);
        Q_ASSERT(m_resourceLoader);
        m_tc->setExternalVariableLoader(QPatternist::ExternalVariableLoader::Ptr
                                                (new ExternalSourceLoader(m_tcSourceInputs,
                                                                          m_resourceLoader)));
        m_tcSourceInputs.clear();

        if(!m_contextItemSource.isEmpty())
        {
            m_tc->setContextItemSource(QUrl(m_sourceMap.value(m_contextItemSource)));
            m_contextItemSource.clear();
        }

        m_tc = 0;
    }
    else if(localName == QLatin1String("output-file"))
    {
        m_baseLine->setDetails(m_currentBaselinePath.resolved(m_ch).toString());
        m_tc->addBaseLine(m_baseLine);
    }
    else if(localName == QLatin1String("input-file"))
    {
        m_tcSourceInputs.insert(m_currentInputVariable, ExternalSourceLoader::VariableValue(m_sourceMap.value(m_ch),
                                                                                            ExternalSourceLoader::Document));
    }
    else if(localName == QLatin1String("expected-error"))
    {
        m_baseLine->setDetails(m_ch);
        m_tc->addBaseLine(m_baseLine);
    }
    else if(localName == QLatin1String("title"))
    {
        /* A bit dangerous, the only element with name title in the vocabulary
         * is the child of GroupInfo */
        m_container->setTitle(m_ch.simplified());
    }
    else if(localName == QLatin1String("test-group"))
    {
        m_testGroupName.pop();
        Q_ASSERT(m_container);
        m_container = static_cast<TestContainer *>(m_container->parent());
        Q_ASSERT(m_container);
    }
    else if(localName == QLatin1String("test-suite"))
    {
        Q_ASSERT(m_container);
        m_container = static_cast<TestContainer *>(m_container->parent());
    }
    else if(localName == QLatin1String("sources"))
    {
        const QPatternist::NetworkAccessDelegator::Ptr networkDelegator(new QPatternist::NetworkAccessDelegator(networkAccessManager(), networkAccessManager()));

        m_resourceLoader = QPatternist::ResourceLoader::Ptr(new QPatternist::AccelTreeResourceLoader(Global::namePool(),
                                                                                                     networkDelegator));

        const ExternalSourceLoader::SourceMap::const_iterator end(m_sourceMap.constEnd());
        ExternalSourceLoader::SourceMap::const_iterator it(m_sourceMap.constBegin());

        for(; it != end; ++it)
            m_resourceLoader->announceDocument(it.value(), QPatternist::ResourceLoader::WillUse);
    }
    else if(localName == QLatin1String("input-URI"))
    {
        m_tcSourceInputs.insert(m_currentInputVariable, ExternalSourceLoader::VariableValue(m_sourceMap.value(m_ch),
                                                                                            ExternalSourceLoader::URI));
    }
    else if(localName == QLatin1String("contextItem"))
        m_contextItemSource = m_ch;

    return true;
}

bool TestSuiteHandler::characters(const QString &ch)
{
    m_ch = ch;
    return true;
}

TestSuite *TestSuiteHandler::testSuite() const
{
    return m_ts;
}

// vim: et:ts=4:sw=4:sts=4

