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

#ifndef PatternistSDK_XMLWriter_H
#define PatternistSDK_XMLWriter_H

#include "Global.h"

#include <QtXml/QXmlContentHandler>
#include <QtXml/QXmlLexicalHandler>

QT_BEGIN_NAMESPACE

class QIODevice;

namespace QPatternistSDK
{
    /**
     * @short Serializes a stream of SAX events into XML, sent to a QIODevice.
     *
     * XMLWriter is a fast and simple XML serializer which takes care of
     * all the low level details of well-formedness and character escaping, allowing
     * the user to focus on higher level issues and increasing the chances of producing
     * valid, interoperable XML.
     *
     * The content XMLWriter produces is sent to a QIODevice, which is either
     * specified in XMLWriter's constructor or via setDevice(). If writing to
     * the device fails, the content functions such as startElement() returns @c false.
     *
     * XMLWriter sub-classes QXmlContentHandler meaning it can serialize content
     * from any code that produces SAX events. The class can also be used manually,
     * by calling startElement(), endCDATA(), and so forth.
     *
     * XMLWriter cannot be used to serialize multiple documents. One instance per
     * document must be used.
     *
     * XMLWriter takes care of escaping content into character references as necessary. Thus,
     * it should not be done manually. In fact, it would most likely
     * result in invalid XML or an unintended result. XMLWriter always serializes into UTF-8.
     *
     * When compiled in debug mode, XMLWriter contains several tests that helps
     * ensuring that XMLWriter produces valid XML. Some of these tests ensures that:
     *
     * - The @c xmlns and @c xml prefixes are used properly
     * - Content of comments and processing instructions is valid
     * - Element, attribute and DOCTYPE names are sensible
     * - Elements are properly nested and balanced
     * - To some extent that things occur in the proper order. For example, that
     *   the document type definition isn't added inside an element
     * - That namespaces prefixes are declared
     *
     * Not triggering XMLWriter's tests does not guarantee valid XML is produced,
     * but they do help catching common mistakes and some of the corner cases in the
     * specifications. When XMLWriter is compiled in release mode, these tests are not enabled
     * and the error handling in effect is concerning writing to the QIODevice.
     *
     * Often it is of interest to add a note at the beginning of the file communicating
     * it is auto-generated. setMessage() and setAddMessage() provides
     * a convenient way of doing that.
     *
     * Namespace declarations are added with startPrefixMapping(), not by sending attributes
     * with name <tt>xmlns:*</tt> to startElement().
     *
     * @see <a href="http://hsivonen.iki.fi/producing-xml/">HOWTO Avoid Being
     * Called a Bozo When Producing XML</a>
     * @see <a href="http://www.w3.org/TR/REC-xml/">Extensible Markup
     * Language (XML) 1.0 (Third Edition)</a>
     * @see <a href="http://www.w3.org/TR/REC-xml-names/">Namespaces in XML</a>
     * @todo Replace this class with QXmlStreamWriter
     * @author Frans Englich <frans.englich@nokia.com>
     * @ingroup PatternistSDK
     */
    class Q_PATTERNISTSDK_EXPORT XMLWriter : public QXmlContentHandler
                                           , public QXmlLexicalHandler
    {
    public:
        /**
         * Creates a XMLWriter which serializes its received events
         * to @p outStream.
         *
         * @note XMLWriter does not claim ownership of @p outStream. Thus,
         * @p outStream may not be destroyed as long as
         * this XMLWriter instance uses it.
         */
        XMLWriter(QIODevice *outStream = 0);

        virtual ~XMLWriter();

        /**
         * @returns @c true if opening the output device succeeds, otherwise @c false
         */
        virtual bool startDocument();

        /**
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool characters(const QString &ch);

        /**
         * Starts an element with name @p qName and attributes @p atts. The prefix
         * in @p qName must first be declared with startPrefixMapping(), if it has one.
         *
         * A call to startElement() must always at some point be balanced with a call
         * to endElement().
         *
         * To declare namespaces, don't put attributes with name <tt>xmlns:*</tt> in @p atts,
         * but use startPrefixMapping().
         */
        virtual bool startElement(const QString &qName, const QXmlAttributes &atts = QXmlAttributes());

        /**
         *
         * Behaves essentially as startElement(const QString &qName, const QXmlAttributes &atts). This
         * function is used in conjunction with other SAX classes.
         *
         * The call:
         *
         * @code
         * startElement(QString(), QString(), qName, atts);
         * @endcode
         *
         * is equivalent to:
         *
         * @code
         * startElement(qName, atts);
         * @endcode
         *
         * @p namespaceURI and @p localName are not used. This function is
         * used in conjunction with other SAX classes.
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool startElement(const QString &namespaceURI,
                                  const QString &localName,
                                  const QString &qName,
                                  const QXmlAttributes &atts);

        /**
         * Signals the end of an element with name @p qName. @p qName must
         * be supplied.
         *
         * Calls to startElement() and endElement() must always be balanced.
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool endElement(const QString &qName);

        /**
         * Behaves essentially as endElement(const QString &qName). This function
         * is used when XMLWriter is used in SAX code.
         *
         * @p namespaceURI and @p localName are not used.
         *
         * The call:
         *
         * @code
         * endElement(QString(), QString(), qName);
         * @endcode
         *
         * is equivalent to:
         *
         * @code
         * endElement(qName);
         * @endcode
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool endElement(const QString &namespaceURI,
                                const QString &localName,
                                const QString &qName);

        /**
         * A description of an error if it occurred. This is typically
         * QIODevice::errorString(). If no error has occurred, an empty
         * string is returned.
         */
        virtual QString errorString() const;

        /**
         * Starts a CDATA section. Content sent with characters() will not be escaped
         * except for ">" if occurring in "]]>".
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool startCDATA();

        /**
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool endCDATA();

        /**
         * Creates a document type definition.
         *
         * For example, the code snippet:
         *
         * @code
         * writer.startDTD("html", "-//W3C//DTD XHTML 1.0 Strict//EN",
         *                 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd");
         * writer.endDTD();
         * @endcode
         *
         * would create:
         * @verbatim
 <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
@endverbatim
         *
         * @note A system identifier must always be specified, but a public identifier may
         * be left out.
         *
         * A call to startDTD() must be followed by a call to endDTD().
         */
        virtual bool startDTD(const QString &name,
                              const QString &publicId,
                              const QString &systemId);

        /**
         * Apart from closing the DTD, an new line is also added at end.
         */
        virtual bool endDTD();

        /**
         * Creates a processing instruction by name @p target, and content
         * @p data.
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool processingInstruction(const QString &target,
                                           const QString &data);

        /**
         * Declares a namespace which maps @p prefix to @p namespaceURI. For example, the call:
         *
         * @code
         * startPrefixMapping("xhtml", "http://www.w3.org/1999/xhtml");
         * @endcode
         *
         * would result in:
         *
         * @code
         * xmlns="http://www.w3.org/1999/xhtml"
         * @endcode
         */
        virtual bool startPrefixMapping(const QString &prefix,
                                        const QString &namespaceURI);

        /**
         * Creates a comment with content @p ch. @p ch is escaped, there's
         * no need to do it manually. For example, calling comment() with @p ch
         * set to "my comment", results in "<!--my comment-->" in the output.
         *
         * @note if @p ch contains double hyphen("--"), the produced XML will
         * not be well formed.
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool comment(const QString &ch);

        virtual bool startEntity(const QString &name);
        virtual bool endEntity(const QString &name);

        /**
         * Sets the message which is added as a comment if addModificationMessage()
         * is set to @c true. If no message is specified and addModificationMessage()
         * is set to @c true, a default message is used.
         *
         * @see modificationMessage(), setAddMessage()
         */
        virtual void setMessage(const QString &msg);

        /**
         * The message that is added at the beginning of the XML events
         * in a comment node. If no modificationMessage is set via modificationMessage(),
         * and addModificationMessage is set to @c true, this message will be used:
         * "NOTE: This file was automatically generated by [the application name] at
         * [the current date time]. All changes to this file will be lost."
         *
         * @see setMessage()
         */
        virtual QString modificationMessage() const;

        /**
         * Closes the QIODevice XMLWriter writes to.
         */
        virtual bool endDocument();

        /**
         * Serializes @p ch as if it was sent to characters().
         *
         * @returns @c false if failure occurs in writing to the QIODevice, otherwise
         * @c true
         */
        virtual bool ignorableWhitespace(const QString &ch);

        /**
         * This function is not used by XMLWriter, but is implemented
         * in order to satisfy QXmlContentHandler's interface.
         */
        virtual bool endPrefixMapping(const QString &prefix);

        /**
         * This function is not used by XMLWriter, but is implemented
         * in order to satisfy QXmlContentHandler's interface.
         */
        virtual bool skippedEntity(const QString &name);

        /**
         * This function is not used by XMLWriter, but is implemented
         * in order to satisfy QXmlContentHandler's interface.
         */
        virtual void setDocumentLocator(QXmlLocator *);

        /**
         * @returns the device XMLWriter writes its output to.
         * XMLWriter does not own the device.
         */
        virtual QIODevice *device() const;

        /**
         * Sets the QIODevice XMLWriter writes to, to @p device. A device must be specified
         * either via this function or in the constructor before XMLWriter is used.
         *
         * XMLWriter does not claim ownership of @p device.
         */
        virtual void setDevice(QIODevice *device);

        /**
         * Determines whether the modification message should be inserted as a comment
         * before the document element. The message returned by modificationMessage() is used.
         *
         * If @p toggle is @c true, the message will be added, otherwise not.
         */
        virtual void setAddMessage(const bool toggle);

        /**
         * Tells whether a modification message will be added.
         *
         * @see setAddMessage(), modificationMessage()
         */
        virtual bool addModificationMessage() const;

    private:
        Q_DISABLE_COPY(XMLWriter)

        class Private;
        Private *d;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
