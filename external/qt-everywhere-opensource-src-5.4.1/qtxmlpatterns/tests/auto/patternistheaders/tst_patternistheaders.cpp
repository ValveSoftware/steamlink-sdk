/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

/*!
 \class tst_PatternistHeaders
 \internal
 \short
 \since 4.4
 \brief Tests that the expected headers are available for Patternist.

 This test is essentially a compilation test. It includes all the headers that are available for
 Patternist, and ensures it compiles.

 This attempts to capture regressions in header generation.
 */
class tst_PatternistHeaders : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void run() const;
};

void tst_PatternistHeaders::run() const
{
    /* This is a dummy, essentially. It only exists such that QTestLib
     * considers everything fine. */
}

/* If you've added a header, you need to add it four times. Twice in this list,
 * and twice in the one below. Alphabetic order. */
#include <QAbstractMessageHandler>
#include <qabstractmessagehandler.h>
#include <QAbstractUriResolver>
#include <qabstracturiresolver.h>
#include <QAbstractXmlNodeModel>
#include <qabstractxmlnodemodel.h>
#include <QAbstractXmlReceiver>
#include <qabstractxmlreceiver.h>
#include <QSimpleXmlNodeModel>
#include <qsimplexmlnodemodel.h>
#include <QSourceLocation>
#include <qsourcelocation.h>
#include <QtXmlPatterns>
#include <QXmlItem>
#include <QXmlName>
#include <qxmlname.h>
#include <QXmlNamePool>
#include <qxmlnamepool.h>
#include <QXmlNodeModelIndex>
#include <QXmlQuery>
#include <qxmlquery.h>
#include <QXmlResultItems>
#include <qxmlresultitems.h>
#include <QXmlSchema>
#include <qxmlschema.h>
#include <QXmlSchemaValidator>
#include <qxmlschemavalidator.h>
#include <QXmlSerializer>
#include <qxmlserializer.h>

/* Same again, this time with QtXmlPatterns prepended. Alphabetic order. */

#include <QtXmlPatterns/QAbstractMessageHandler>
#include <QtXmlPatterns/qabstractmessagehandler.h>
#include <QtXmlPatterns/QAbstractUriResolver>
#include <QtXmlPatterns/qabstracturiresolver.h>
#include <QtXmlPatterns/QAbstractXmlNodeModel>
#include <QtXmlPatterns/qabstractxmlnodemodel.h>
#include <QtXmlPatterns/QAbstractXmlReceiver>
#include <QtXmlPatterns/qabstractxmlreceiver.h>
#include <QtXmlPatterns/QSimpleXmlNodeModel>
#include <QtXmlPatterns/qsimplexmlnodemodel.h>
#include <QtXmlPatterns/QSourceLocation>
#include <QtXmlPatterns/qsourcelocation.h>
#include <QtXmlPatterns/QtXmlPatterns>
#include <QtXmlPatterns/QXmlItem>
#include <QtXmlPatterns/QXmlName>
#include <QtXmlPatterns/qxmlname.h>
#include <QtXmlPatterns/QXmlNamePool>
#include <QtXmlPatterns/qxmlnamepool.h>
#include <QtXmlPatterns/QXmlNodeModelIndex>
#include <QtXmlPatterns/QXmlQuery>
#include <QtXmlPatterns/qxmlquery.h>
#include <QtXmlPatterns/QXmlResultItems>
#include <QtXmlPatterns/qxmlresultitems.h>
#include <QtXmlPatterns/QXmlSchema>
#include <QtXmlPatterns/qxmlschema.h>
#include <QtXmlPatterns/QXmlSchemaValidator>
#include <QtXmlPatterns/qxmlschemavalidator.h>
#include <QtXmlPatterns/QXmlSerializer>
#include <QtXmlPatterns/qxmlserializer.h>

QTEST_MAIN(tst_PatternistHeaders)

#include "tst_patternistheaders.moc"

// vim: et:ts=4:sw=4:sts=4
