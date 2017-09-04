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

#ifndef PatternistSDK_TestResultHandler_H
#define PatternistSDK_TestResultHandler_H

#include <QHash>
#include <QString>
#include <QtXml/QXmlDefaultHandler>

#include "TestResult.h"

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Reads XML in the @c XQTSResult.xsd format, and provides access to
     * the reported results.
     *
     * @author Frans Englich <frans.englich@nokia.com>
     * @ingroup PatternistSDK
     */
    class Q_PATTERNISTSDK_EXPORT TestResultHandler : public QXmlDefaultHandler
    {
    public:
        /**
         * A hash where the key is the class's name, that is <tt>test-case/@@name</tt>,
         * and the value the test's result status.
         */
        typedef QHash<QString, TestResult::Status> Hash;

        /**
         * A hash mapping test-case names to their' comments.
         */
        typedef QHash<QString, QString> CommentHash;

        /**
         * Creates a TestResultHandler that will read @p file when run() is called.
         */
        TestResultHandler();

        /**
         * Performs finalization.
         */
        virtual bool endDocument();

        /**
         * Reads the <tt>test-case</tt> element and its attributes, everything else is ignored.
         */
        virtual bool startElement(const QString &namespaceURI,
                                  const QString &localName,
                                  const QString &qName,
                                  const QXmlAttributes &atts);
        /**
         * @note Do not reimplement this function.
         * @returns the result obtained from reading the XML file.
         */
        Hash result() const;

        CommentHash comments() const;

    private:
        Q_DISABLE_COPY(TestResultHandler)
        Hash        m_result;
        CommentHash m_comments;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
