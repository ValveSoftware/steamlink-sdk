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

#include <QCoreApplication>
#include <QDateTime>
#include <QIODevice>
#include <QList>
#include <QPair>
#include <QStack>
#include <QtDebug>

#include "XMLWriter.h"

/* Issues:
 * - Switch to Qt's d-pointer semantics, if in Qt.
 * - Remove namespace(PatternistSDK), and change name, if in Qt.
 * - Is it really necessary to pass the tag name to endElement()?
 * - Could it be of interest to let the user control the encoding? Are those cases common
 *   enough to justify support in Qt? Using anything but UTF-8 or UTF-16
 *   means asking for trouble, from an interoperability perspective.
 */

/* Design rationalis, comments:
 *
 * - The class is called XMLWriter to harvest familiarity by being consistent with
 *   Java's XMLWriter class. If XMLWriter is moved to Qt, the name QXmlWriter is perhaps suitable.
 * - The class does not handle indentation because the "do one thing well"-principle is
 *   in use. XMLWriter should be fast and not assume a certain idea of indentation. Indentation
 *   should be implemented in a standalone QXmlContentHandler that performs the indentation and
 *   "has a" QXmlContentHandler which it in addition calls, and by that proxying/piping another
 *   QXmlContentHandler(which most likely is an XMLWriter). Thus, achieving a modularized,
 *   flexibly approach to indentation. A reason is also that indentation is very subjective.
 *   The indenter class should probably be called XMLIndenter/QXmlIndenter.
 * - It could be of interest to implement QXmlDTDHandler such that it would be possible to serialize
 *   DTDs. Must be done before BC becomes significant.
 * - I think the most valuable of this class is its Q_ASSERT tests. Many programmers have severe problems
 *   producing XML, and the tests helps them catching their mistakes. They therefore promote
 *   interoperability. Do not remove them. If any are wrong, fix them instead.
 */

using namespace QPatternistSDK;

/**
 * A namespace binding, prefix/namespace URI.
 */
typedef QPair<QString, QString> NSBinding;
typedef QList<NSBinding> NSBindingList;

class XMLWriter::Private
{
public:
    inline Private(QIODevice *devP) : insideCDATA(false),
                                     addModificationNote(false),
                                     dev(devP)
    {
        hasContentStack.push(true);
    }

#ifdef QT_NO_DEBUG
    inline void validateQName(const QString &) const
    {
    }

    inline void verifyNS(const QString &) const
    {
    }
#else
    /**
     * Simple test of that @p name is an acceptable QName.
     */
    inline void validateQName(const QString &name)
    {
        Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO,
                   "An XML name cannot be empty.");
        Q_ASSERT_X(!name.endsWith(QLatin1Char(':')), Q_FUNC_INFO,
                   "An XML name cannot end with a colon(QLatin1Char(':')).");
        Q_ASSERT_X(!name.contains(QRegExp(QLatin1String("[ \t\n]"))), Q_FUNC_INFO,
                   "An XML name cannot contain whitespace.");
    }

    /**
     * Ensures that the prefix of @p qName is declared.
     */
    inline void verifyNS(const QString &qName) const
    {
        const QString prefix(qName.left(qName.indexOf(QLatin1Char(':'))));

        if(qName.contains(QLatin1Char(':')) && prefix != QLatin1String("xml"))
        {
            bool foundPrefix = false;
            const QStack<NSBindingList>::const_iterator end(namespaceTracker.constEnd());
            QStack<NSBindingList>::const_iterator it(namespaceTracker.constBegin());

            for(; it != end; ++it)
            {
                const NSBindingList::const_iterator lend((*it).constEnd());
                NSBindingList::const_iterator lit((*it).constBegin());

                for(; lit != lend; ++it)
                {
                    if((*lit).first == prefix)
                    {
                        foundPrefix = true;
                        break;
                    }
                }
                if(foundPrefix)
                    break;
            }

            Q_ASSERT_X(foundPrefix, "XMLWriter::startElement()",
                       qPrintable(QString::fromLatin1("The prefix %1 is not declared. All prefixes "
                                          "except 'xml' must be declared.").arg(prefix)));
        }
    }
#endif

    inline QString escapeElementContent(const QString &ch)
    {
        const int l = ch.length();
        QString retval;

        for(int i = 0; i != l; ++i)
        {
            const QChar c(ch.at(i));

            if(c == QLatin1Char(QLatin1Char('&')))
                retval += QLatin1String("&amp;");
            else if(c == QLatin1Char(QLatin1Char('<')))
                retval += QLatin1String("&lt;");
            else
                retval += c;
        }

        return retval;
    }

    inline QString escapeAttributeContent(const QString &ch)
    {
        const int l = ch.length();
        QString retval;

        for(int i = 0; i != l; ++i)
        {
            const QChar c(ch.at(i));

            /* We don't have to escape '\'' because we use '\"' as attribute delimiter. */
            if(c == QLatin1Char('&'))
                retval += QLatin1String("&amp;");
            else if(c == QLatin1Char('<'))
                retval += QLatin1String("&lt;");
            else if(c == QLatin1Char('"'))
                retval += QLatin1String("&quot;");
            else
                retval += c;
        }

        return retval;
    }

    inline QString escapeCDATAContent(const QString &ch)
    {
        const int l = ch.length();
        QString retval;
        qint8 atEnd = 0;

        for(int i = 0; i != l; ++i)
        {
            const QChar c(ch.at(i));

            /* Escape '>' if in "]]>" */
            if(c == QLatin1Char(']'))
            {
                if(atEnd == 0 || atEnd == 1)
                    ++atEnd;
                else
                    atEnd = 0;

                retval += QLatin1Char(']');
            }
            else if(c == QLatin1Char('>'))
            {
                if(atEnd == 2)
                    retval += QLatin1String("&gt;");
                else
                {
                    atEnd = 0;
                    retval += QLatin1Char('>');
                }
            }
            else
                retval += c;
        }

        return retval;
    }

    /**
     * We wrap dev in this function such that we can deploy the Q_ASSERT_X
     * macro in each place it's used.
     */
    inline QIODevice *device() const
    {
        Q_ASSERT_X(dev, Q_FUNC_INFO,
                   "No device specified for XMLWriter; one must be specified with "
                   "setDevice() or via the constructor before XMLWriter can be used.");
        return dev;
    }

    /**
     * @returns true on success, otherwise false
     */
    inline bool serialize(const QString &data)
    {
        const QByteArray utf8(data.toUtf8());

        return device()->write(utf8) == utf8.size();
    }

    /**
     * @returns true on success, otherwise false
     */
    inline bool serialize(const char data)
    {
        return device()->putChar(data);
    }

    /**
     * @returns true on success, otherwise false
     */
    inline bool serialize(const char *data)
    {
        return device()->write(data) == qstrlen(data);
    }

    inline bool hasElementContent() const
    {
        return hasContentStack.top();
    }

    inline void handleElement()
    {
        if(!hasElementContent())
            serialize('>');

        /* This element is content for the parent. */
        hasContentStack.top() = true;
    }

    NSBindingList namespaces;
    bool insideCDATA;
    bool addModificationNote;
    QString msg;
    QIODevice *dev;
    QStack<bool> hasContentStack;
    QString errorString;
    QStack<QString> tags;
    QStack<NSBindingList> namespaceTracker;
};

/**
 * Reduces complexity. The empty else clause is for avoiding mess when macro
 * is used in the 'then' branch of an if clause, which is followed by an else clause.
 */
#define serialize(string) if(!d->serialize(string)) \
                          { \
                              d->errorString = d->device()->errorString(); \
                              return false; \
                          } \
                          else do {} while (false)

XMLWriter::XMLWriter(QIODevice *outStream) : d(new Private(outStream))
{
}

XMLWriter::~XMLWriter()
{
    delete d;
}

bool XMLWriter::startDocument()
{
    if(!device()->isOpen() && !device()->open(QIODevice::WriteOnly))
        return false;

    if(d->addModificationNote)
    {
        if(d->msg.isNull())
        {
            d->msg = QString::fromLatin1("NOTE: This file was automatically generated "
                                         "by %1 at %2. All changes to this file will be lost.")
                                         .arg(QCoreApplication::instance()->applicationName(),
                                              QDateTime::currentDateTime().toString());
        }
        if(!comment(d->msg))
            return false;

        serialize('\n');
    }

    serialize(QLatin1String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));

    return true;
}

bool XMLWriter::startElement(const QString &/*namespaceURI*/,
                             const QString &/*localName*/,
                             const QString &qName,
                             const QXmlAttributes &atts)
{
    return startElement(qName, atts);
}

bool XMLWriter::startElement(const QString &qName,
                             const QXmlAttributes &atts)
{
    Q_ASSERT_X(!d->insideCDATA, Q_FUNC_INFO,
               "Only characters() can be received when inside CDATA.");
    Q_ASSERT_X(!qName.startsWith(QLatin1String("xmlns")), Q_FUNC_INFO,
               "startElement should not be used for declaring prefixes, "
               "use startPrefixMapping() for that.");

    d->validateQName(qName);
    d->verifyNS(qName);

    d->handleElement();

    serialize('<');
    serialize(qName);

    d->tags.push(qName);
    d->namespaceTracker.push(d->namespaces);

    /* Add namespace declarations. */
    const NSBindingList::const_iterator end(d->namespaces.constEnd());
    NSBindingList::const_iterator it(d->namespaces.constBegin());

    for(; it != end; ++it)
    {
        if((*it).first.isEmpty())
            serialize(" xmlns=");
        else
        {
            serialize(" xmlns:");
            serialize((*it).first);
            serialize('=');
        }

        serialize('"');
        serialize(d->escapeElementContent((*it).second));
        serialize('"');
    }
    d->namespaces.clear();

    const int c = atts.count();

    /* Serialize attributes. */
    for(int i = 0; i != c; ++i)
    {
        d->validateQName(atts.qName(i));
        d->verifyNS(atts.qName(i));

        serialize(' ');
        serialize(atts.qName(i));
        serialize("=\"");
        serialize(d->escapeAttributeContent(atts.value(i)));
        serialize('"');
    }

    d->hasContentStack.push(false);
    return true;
}

bool XMLWriter::endElement(const QString &/*namespaceURI*/,
                           const QString &/*localName*/,
                           const QString &qName)
{
    return endElement(qName);
}

bool XMLWriter::endElement(const QString &qName)
{
    Q_ASSERT_X(!d->insideCDATA, Q_FUNC_INFO,
               "Only characters() can be received when inside CDATA.");
    Q_ASSERT_X(d->tags.pop() == qName, Q_FUNC_INFO,
               "The element tags are not balanced, the produced XML is invalid.");

    d->namespaceTracker.pop();

    /* "this" element is content for our parent, so ensure hasElementContent is true. */

    if(d->hasElementContent())
    {
        serialize(QLatin1String("</"));
        serialize(qName);
        serialize('>');
    }
    else
        serialize(QLatin1String("/>"));

    d->hasContentStack.pop();

    return true;
}

bool XMLWriter::startPrefixMapping(const QString &prefix, const QString &uri)
{
    Q_ASSERT_X(!d->insideCDATA, Q_FUNC_INFO,
               "Only characters() can be received when inside CDATA.");
    Q_ASSERT_X(prefix.toLower() != QLatin1String("xml") ||
               (prefix.toLower() == QLatin1String("xml") &&
               (uri == QLatin1String("http://www.w3.org/TR/REC-xml-names/") ||
                                              uri.isEmpty())),
               Q_FUNC_INFO,
               "The prefix 'xml' can only be bound to the namespace "
               "\"http://www.w3.org/TR/REC-xml-names/\".");
    Q_ASSERT_X(prefix.toLower() != QLatin1String("xml") &&
                uri != QLatin1String("http://www.w3.org/TR/REC-xml-names/"),
               Q_FUNC_INFO,
               "The namespace \"http://www.w3.org/TR/REC-xml-names/\" can only be bound to the "
               "\"xml\" prefix.");

    d->namespaces.append(qMakePair(prefix, uri));
    return true;
}

bool XMLWriter::processingInstruction(const QString &target,
                                      const QString &data)
{
    Q_ASSERT_X(target.toLower() != QLatin1String("xml"), Q_FUNC_INFO,
               "A processing instruction cannot have the name xml in any "
               "capitalization, because it is reserved.");
    Q_ASSERT_X(!data.contains(QLatin1String("?>")), Q_FUNC_INFO,
               "The content of a processing instruction cannot contain the string \"?>\".");
    Q_ASSERT_X(!d->insideCDATA, "XMLWriter::processingInstruction()",
               "Only characters() can be received when inside CDATA.");

    d->handleElement();

    serialize(QLatin1String("<?"));
    serialize(target);
    serialize(' ');
    serialize(data);
    serialize(QLatin1String("?>"));
    return true;
}

bool XMLWriter::characters(const QString &ch)
{
    Q_ASSERT_X(d->tags.count() >= 1, Q_FUNC_INFO,
               "Text nodes can only appear inside elements(no elements sent).");
    d->handleElement();

    if(d->insideCDATA)
        serialize(d->escapeCDATAContent(ch));
    else
        serialize(d->escapeElementContent(ch));

    return true;
}

bool XMLWriter::comment(const QString &ch)
{
    Q_ASSERT_X(!d->insideCDATA, Q_FUNC_INFO,
               "Only characters() can be received when inside CDATA.");
    Q_ASSERT_X(!ch.contains(QLatin1String("--")), Q_FUNC_INFO,
               "XML comments may not contain double-hyphens(\"--\").");
    Q_ASSERT_X(!ch.endsWith(QLatin1Char('-')), Q_FUNC_INFO,
               "XML comments cannot end with a hyphen, \"-\"(add a space, for example).");
    /* A comment starting with "<!---" is ok. */

    d->handleElement();

    serialize(QLatin1String("<!--"));
    serialize(ch);
    serialize(QLatin1String("-->"));

    return true;
}

bool XMLWriter::startCDATA()
{
    Q_ASSERT_X(d->insideCDATA, Q_FUNC_INFO,
               "startCDATA() has already been called.");
    Q_ASSERT_X(d->tags.count() >= 1, Q_FUNC_INFO,
               "CDATA sections can only appear inside elements(no elements sent).");
    d->insideCDATA = true;
    serialize(QLatin1String("<![CDATA["));
    return true;
}

bool XMLWriter::endCDATA()
{
    d->insideCDATA = false;
    serialize("]]>");
    return true;
}

bool XMLWriter::startDTD(const QString &name,
                         const QString &publicId,
                         const QString &systemId)
{
    Q_ASSERT_X(!d->insideCDATA, Q_FUNC_INFO,
               "Only characters() can be received when inside CDATA.");
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO,
               "The DOCTYPE name cannot be empty.");
    Q_ASSERT_X(d->tags.isEmpty() && d->namespaces.isEmpty(), Q_FUNC_INFO,
               "No content such as namespace declarations or elements can be serialized "
               "before the DOCTYPE declaration, the XML is invalid.");
    Q_ASSERT_X(!publicId.contains(QLatin1Char('"')), Q_FUNC_INFO,
               "The PUBLIC ID cannot contain quotes('\"').");
    Q_ASSERT_X(!systemId.contains(QLatin1Char('"')), Q_FUNC_INFO,
               "The SYSTEM ID cannot contain quotes('\"').");

    serialize(QLatin1String("<!DOCTYPE "));
    serialize(name);

    if(!publicId.isEmpty())
    {
        Q_ASSERT_X(!systemId.isEmpty(), Q_FUNC_INFO,
                   "When a public identifier is specified, a system identifier "
                   "must also be specified in order to produce valid XML.");
        serialize(" PUBLIC \"");
        serialize(publicId);
        serialize('"');
    }

    if(!systemId.isEmpty())
    {
        if (publicId.isEmpty()) {
            serialize(" SYSTEM");
        }

        serialize(" \"");
        serialize(systemId);
        serialize('"');
    }

    return true;
}

bool XMLWriter::endDTD()
{
    Q_ASSERT_X(d->tags.isEmpty() && d->namespaces.isEmpty(), Q_FUNC_INFO,
               "Content such as namespace declarations or elements cannot occur inside "
               "the DOCTYPE declaration, the XML is invalid.");
    serialize(QLatin1String(">\n"));
    return true;
}

bool XMLWriter::startEntity(const QString &)
{
    return true;
}

bool XMLWriter::endEntity(const QString &)
{
    return true;
}

void XMLWriter::setMessage(const QString &msg)
{
    d->msg = msg;
}

QString XMLWriter::modificationMessage() const
{
    return d->msg;
}

bool XMLWriter::endDocument()
{
    Q_ASSERT_X(d->tags.isEmpty(), Q_FUNC_INFO,
               "endDocument() called before all elements were closed with endElement().");
    d->device()->close();
    return true;
}

QString XMLWriter::errorString() const
{
    return d->errorString;
}

bool XMLWriter::ignorableWhitespace(const QString &ch)
{
    return characters(ch);
}

bool XMLWriter::endPrefixMapping(const QString &)
{
    /* Again, should we do something with this? */
    return true;
}

bool XMLWriter::skippedEntity(const QString &)
{
    return true;
}

void XMLWriter::setDocumentLocator(QXmlLocator *)
{
}

QIODevice *XMLWriter::device() const
{
    return d->dev;
}

void XMLWriter::setDevice(QIODevice *dev)
{
    d->dev = dev;
}

void XMLWriter::setAddMessage(const bool toggle)
{
    d->addModificationNote = toggle;
}

bool XMLWriter::addModificationMessage() const
{
    return d->addModificationNote;
}

#undef serialize
// vim: et:ts=4:sw=4:sts=4

