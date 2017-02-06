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

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QtDebug>
#include <QUrl>
#include <QXmlAttributes>
#include <QXmlSimpleReader>

#include <private/qxmldebug_p.h>
#include "XMLWriter.h"

#include "TestBaseLine.h"

using namespace QPatternistSDK;
using namespace QPatternist;

Q_GLOBAL_STATIC_WITH_ARGS(QRegExp, errorRegExp, (QLatin1String("[A-Z]{4}[0-9]{4}")))

TestBaseLine::TestBaseLine(const Type t) : m_type(t)
{
    Q_ASSERT(errorRegExp()->isValid());
}

TestResult::Status TestBaseLine::scan(const QString &serialized,
                                      const TestBaseLine::List &lines)
{
    Q_ASSERT_X(lines.count() >= 1, Q_FUNC_INFO,
               "At least one base line must be passed, otherwise there's nothing "
               "to compare to.");

    const TestBaseLine::List::const_iterator end(lines.constEnd());
    TestBaseLine::List::const_iterator it(lines.constBegin());
    for(; it != end; ++it)
    {
        const TestResult::Status retval((*it)->verify(serialized));

        if(retval == TestResult::Pass || retval == TestResult::NotTested)
            return retval;
    }

    return TestResult::Fail;
}

TestResult::Status TestBaseLine::scanErrors(const ErrorHandler::Message::List &errors,
                                            const TestBaseLine::List &lines)
{
    pDebug() << "TestBaseLine::scanErrors()";

    /* 1. Find the first error in @p errors that's a Patternist
     * error(not warning and not from Qt) and extract the error code. */
    QString errorCode;

    const ErrorHandler::Message::List::const_iterator end(errors.constEnd());
    ErrorHandler::Message::List::const_iterator it(errors.constBegin());
    for(; it != end; ++it)
    {
        if((*it).type() != QtFatalMsg)
            continue;

        errorCode = QUrl((*it).identifier()).fragment();

        pDebug() << "ERR:" << (*it).description();
        /* This is hackish. We have no way of determining whether a Message
         * is actually issued from Patternist, so we try to narrow it down like this. */
        if(errorRegExp()->exactMatch(errorCode))
            break; /* It's an error code. */
        else
            errorCode.clear();
    }

    pDebug() << "Got error code: " << errorCode;
    /* 2. Loop through @p lines, and for the first base line
     * which is of type ExpectedError and which matches @p errorCode
     * return Pass, otherwise Fail. */
    const TestBaseLine::List::const_iterator blend(lines.constEnd());
    TestBaseLine::List::const_iterator blit(lines.constBegin());
    for(; blit != blend; ++blit)
    {
        const Type t = (*blit)->type();

        if(t == TestBaseLine::ExpectedError)
        {
            const QString d((*blit)->details());
            if(d == errorCode || d == QChar::fromLatin1('*'))
                return TestResult::Pass;
        }
    }

    return TestResult::Fail;
}

void TestBaseLine::toXML(XMLWriter &receiver) const
{
    switch(m_type)
    {
        case XML: /* Fallthrough. */
        case Fragment: /* Fallthrough. */
        case SchemaIsValid: /* Fallthrough. */
        case Text:
        {
            QXmlAttributes inspectAtts;
            inspectAtts.append(QLatin1String("role"), QString(),
                               QLatin1String("role"), QLatin1String("principal"));
            inspectAtts.append(QLatin1String("compare"), QString(),
                               QLatin1String("compare"), displayName(m_type));
            receiver.startElement(QLatin1String("output-file"), inspectAtts);
            receiver.characters(m_details);
            receiver.endElement(QLatin1String("output-file"));
            return;
        }
        case Ignore:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO, "Serializing 'Ignore' is not implemented.");
            return;
        }
        case Inspect:
        {
            QXmlAttributes inspectAtts;
            inspectAtts.append(QLatin1String("role"), QString(),
                               QLatin1String("role"), QLatin1String("principal"));
            inspectAtts.append(QLatin1String("compare"), QString(),
                               QLatin1String("compare"), QLatin1String("Inspect"));
            receiver.startElement(QLatin1String("output-file"), inspectAtts);
            receiver.characters(m_details);
            receiver.endElement(QLatin1String("output-file"));
            return;
        }
        case ExpectedError:
        {
            receiver.startElement(QLatin1String("expected-error"));
            receiver.characters(m_details);
            receiver.endElement(QLatin1String("expected-error"));
            return;
        }
    }
}

bool TestBaseLine::isChildrenDeepEqual(const QDomNodeList &cl1, const QDomNodeList &cl2)
{
    const int len = cl1.length();

    if(len == cl2.length())
    {
        for (int i = 0; i < len; ++i) {
            if(!isDeepEqual(cl1.at(i), cl2.at(i)))
                return false;
        }

        return true;
    }
    else
        return false;
}

bool TestBaseLine::isAttributesEqual(const QDomNamedNodeMap &cl1, const QDomNamedNodeMap &cl2)
{
    const int len = cl1.length();
    pDebug() << "LEN:" << len;

    if(len == cl2.length())
    {
        for (int i1 = 0; i1 < len; ++i1) {
            const QDomNode attr1(cl1.item(i1));
            Q_ASSERT(!attr1.isNull());

            /* This is set if attr1 cannot be found at all in cl2. */
            bool earlyExit = false;

            for (int i2 = 0; i2 < len; ++i2) {
                const QDomNode attr2(cl2.item(i2));
                Q_ASSERT(!attr2.isNull());
                pDebug() << "ATTR1:" << attr1.localName() << attr1.namespaceURI() << attr1.prefix() << attr1.nodeName();
                pDebug() << "ATTR2:" << attr2.localName() << attr2.namespaceURI() << attr2.prefix() << attr2.nodeName();

                if(attr1.localName() == attr2.localName()       &&
                   attr1.namespaceURI() == attr2.namespaceURI() &&
                   attr1.prefix() == attr2.prefix()             &&
                   attr1.nodeName() == attr2.nodeName()         && /* Yes, needed in addition to all the other. */
                   attr1.nodeValue() == attr2.nodeValue())
                {
                    earlyExit = true;
                    break;
                }
            }

            if(!earlyExit)
            {
                /* An attribute was found that doesn't exist in the other list so exit. */
                return false;
            }
        }

        return true;
    }
    else
        return false;
}

bool TestBaseLine::isDeepEqual(const QDomNode &n1, const QDomNode &n2)
{
    if(n1.nodeType() != n2.nodeType())
        return false;

    switch(n1.nodeType())
    {
        case QDomNode::CommentNode:
        /* Fallthrough. */
        case QDomNode::TextNode:
        {
            return static_cast<const QDomCharacterData &>(n1).data() ==
                   static_cast<const QDomCharacterData &>(n2).data();
        }
        case QDomNode::ProcessingInstructionNode:
        {
            return n1.nodeName() == n2.nodeName() &&
                   n1.nodeValue() == n2.nodeValue();
        }
        case QDomNode::DocumentNode:
            return isChildrenDeepEqual(n1.childNodes(), n2.childNodes());
        case QDomNode::ElementNode:
        {
            return n1.localName() == n2.localName()                     &&
                   n1.namespaceURI() == n2.namespaceURI()               &&
                   n1.nodeName() == n2.nodeName()                       && /* Yes, this one is needed in addition to localName(). */
                   isAttributesEqual(n1.attributes(), n2.attributes())  &&
                   isChildrenDeepEqual(n1.childNodes(), n2.childNodes());
        }
        /* Fallthrough all these. */
        case QDomNode::EntityReferenceNode:
        case QDomNode::CDATASectionNode:
        case QDomNode::EntityNode:
        case QDomNode::DocumentTypeNode:
        case QDomNode::DocumentFragmentNode:
        case QDomNode::NotationNode:
        case QDomNode::BaseNode:
        case QDomNode::CharacterDataNode:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO,
                       "An unsupported node type was encountered.");
            return false;
        }
        case QDomNode::AttributeNode:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO,
                       "This should never happen. QDom doesn't allow us to compare DOM attributes "
                       "properly.");
            return false;
        }
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO, "Unhandled QDom::NodeType value.");
            return false;
        }
    }
}

TestResult::Status TestBaseLine::verify(const QString &serializedInput) const
{
    switch(m_type)
    {
        case SchemaIsValid:
        /* Fall through. */
        case Text:
        {
            if(serializedInput == details())
                return TestResult::Pass;
            else
                return TestResult::Fail;
        }
        case Fragment:
        /* Fall through. */
        case XML:
        {
            /* Read the baseline and the serialized input into two QDomDocuments, and compare
             * them deeply. We wrap fragments in a root node such that it is well-formed XML.
             */

            QDomDocument output;
            {
                /* The reason we put things into a QByteArray and then parse it through QXmlSimpleReader, is that
                 * QDomDocument does whitespace stripping when calling setContent(QString). In other words,
                 * this workarounds a bug. */

                QXmlInputSource source;
                source.setData((m_type == XML ? serializedInput : QLatin1String("<r>") +
                                                                  serializedInput +
                                                                  QLatin1String("</r>")).toUtf8());

                QString outputReadingError;

                QXmlSimpleReader reader;
                reader.setFeature(QLatin1String("http://xml.org/sax/features/namespace-prefixes"), true);

                const bool success = output.setContent(&source,
                                                       &reader,
                                                       &outputReadingError);

                if(!success)
                    return TestResult::Fail;

                Q_ASSERT(success);
            }

            QDomDocument baseline;
            {
                QXmlInputSource source;
                source.setData((m_type == XML ? details() : QLatin1String("<r>") +
                                                            details() +
                                                            QLatin1String("</r>")).toUtf8());
                QString baselineReadingError;

                QXmlSimpleReader reader;
                reader.setFeature(QLatin1String("http://xml.org/sax/features/namespace-prefixes"), true);

                const bool success = baseline.setContent(&source,
                                                         &reader,
                                                         &baselineReadingError);

                if(!success)
                    return TestResult::Fail;

                /* This piece of code workaround a bug in QDom, which treats XML prologs as processing
                 * instructions and make them available in the tree as so. */
                if(m_type == XML)
                {
                    /* $doc/r/node() */
                    const QDomNodeList children(baseline.childNodes());
                    const int len = children.length();

                    for(int i = 0; i < len; ++i)
                    {
                        const QDomNode &child = children.at(i);
                        if(child.isProcessingInstruction() && child.nodeName() == QLatin1String("xml"))
                        {
                            baseline.removeChild(child);
                            break;
                        }
                    }
                }

                Q_ASSERT_X(baselineReadingError.isNull(), Q_FUNC_INFO,
                           qPrintable((QLatin1String("Reading the baseline failed: ") + baselineReadingError)));
            }

            if(isDeepEqual(output, baseline))
                return TestResult::Pass;
            else
            {
                pDebug() << "FAILURE:" << output.toString() << "is NOT IDENTICAL to(baseline):" << baseline.toString();
                return TestResult::Fail;
            }
        }
        case Ignore:
            return TestResult::Pass;
        case Inspect:
            return TestResult::NotTested;
        case ExpectedError:
        {
            /* This function is only called for Text/XML/Fragment tests. */
            return TestResult::Fail;
        }
    }
    Q_ASSERT(false);
    return TestResult::Fail;
}

TestBaseLine::Type TestBaseLine::identifierFromString(const QString &string)
{
    /* "html-output: Using an ad hoc tool, it must assert that the document obeys the HTML
     * Output Method as defined in the Serialization specification and section
     * 20 of the XSLT 2.0 specification." We treat it as XML for now, same with
     * xhtml-output. */
    if(string.compare(QLatin1String("XML"), Qt::CaseInsensitive) == 0 ||
       string == QLatin1String("html-output") ||
       string == QLatin1String("xml-output") ||
       string == QLatin1String("xhtml-output"))
        return XML;
    else if(string == QLatin1String("Fragment") || string == QLatin1String("xml-frag"))
        return Fragment;
    else if(string.compare(QLatin1String("Text"), Qt::CaseInsensitive) == 0)
        return Text;
    else if(string == QLatin1String("Ignore"))
        return Ignore;
    else if(string.compare(QLatin1String("Inspect"), Qt::CaseInsensitive) == 0)
        return Inspect;
    else
    {
        Q_ASSERT_X(false, Q_FUNC_INFO,
                   qPrintable(QString::fromLatin1("Invalid string representation for a comparation type: %1").arg(string)));

        return Ignore; /* Silence GCC. */
    }
}

QString TestBaseLine::displayName(const Type id)
{
    switch(id)
    {
        case XML:
            return QLatin1String("XML");
        case Fragment:
            return QLatin1String("Fragment");
        case Text:
            return QLatin1String("Text");
        case Ignore:
            return QLatin1String("Ignore");
        case Inspect:
            return QLatin1String("Inspect");
        case ExpectedError:
            return QLatin1String("ExpectedError");
        case SchemaIsValid:
            return QLatin1String("SchemaIsValid");
    }

    Q_ASSERT(false);
    return QString();
}

QString TestBaseLine::details() const
{
    if(m_type == Ignore) /* We're an error code. */
        return QString();
    if(m_type == ExpectedError) /* We're an error code. */
        return m_details;
    if(m_type == SchemaIsValid) /* We're a schema validation information . */
        return m_details;

    if(m_details.isEmpty())
        return m_details;

    /* m_details is a file name, we open it and return the result. */
    QFile file(QUrl(m_details).toLocalFile());

    QString retval;
    if(!file.exists())
        retval = QString::fromLatin1("%1 does not exist.").arg(file.fileName());
    else if(!QFileInfo(file.fileName()).isFile())
        retval = QString::fromLatin1("%1 is not a file, cannot display it.").arg(file.fileName());
    else if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        retval = QString::fromLatin1("Could not open %1. Likely a permission error.").arg(file.fileName());

    if(retval.isNull())
    {
        /* Scary, we assume the query/baseline is in UTF-8. */
        return QString::fromUtf8(file.readAll());
    }
    else
    {
        /* We had a file error. */
        retval.prepend(QLatin1String("Test-suite harness error: "));
        qCritical() << retval;
        return retval;
    }
}

TestBaseLine::Type TestBaseLine::type() const
{
    return m_type;
}

void TestBaseLine::setDetails(const QString &detailsP)
{
    m_details = detailsP;
}

// vim: et:ts=4:sw=4:sts=4
