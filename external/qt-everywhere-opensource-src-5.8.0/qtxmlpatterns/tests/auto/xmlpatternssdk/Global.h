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

#ifndef PatternistSDK_Global_H
#define PatternistSDK_Global_H

#include <QString>

#include <private/qitem_p.h>
#include <private/qnamepool_p.h>

#if defined(Q_OS_WIN)
#   ifdef Q_PATTERNISTSDK_BUILDING
        #define Q_PATTERNISTSDK_EXPORT __declspec(dllexport)
    #else
        #define Q_PATTERNISTSDK_EXPORT __declspec(dllimport)
    #endif
#else
    #define Q_PATTERNISTSDK_EXPORT
#endif

/**
 * @short Contains testing utilities for Patternist, interfacing W3C's XQuery Test Suite.
 *
 * @see <a href="http://www.w3.org/XML/Query/test-suite/">XML Query Test Suite</a>
 * @author Frans Englich <frans.englich@nokia.com>
 */
QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Contains global constants.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT Global
    {
    public:

        /**
         * The namespace which the XQTS test case catalog(specified by Catalog.xsd)
         * is in. The namespace is: <tt>http://www.w3.org/2005/02/query-test-XQTSCatalog</tt>
         */
        static const QString xqtsCatalogNS;

        /**
         * The namespace which the XQTS test results collection(specified by XQTSResult.xsd)
         * is in. The namespace is: <tt>http://www.w3.org/2005/02/query-test-XQTSResult</tt>
         */
        static const QString xqtsResultNS;

        /**
         * The organization which created PatternistSDK. It say something
         * in the direction of "Patternist Team", and is used for QSettings and the like.
         */
        static const QString organizationName;

        /**
         * The namespace which W3C's XSL-T test suite resides in.
         */
        static const QString xsltsCatalogNS;

        /**
         * The version of PatternistSDK. The value has currently no other
         * meaning than that larger means older. This version information is supplied to
         * QMainWindow::restoreState() and QMainWindow::saveState().
         */
        static const qint16 versionNumber;

        /**
         * Parses the lexical space of @c xs:boolean,
         * with the exception that the empty string is considered @c false.
         */
        static bool readBoolean(const QString &lexicalSpace);

        static QPatternist::NamePool::Ptr namePool();
        static QXmlNamePool namePoolAsPublic();
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
