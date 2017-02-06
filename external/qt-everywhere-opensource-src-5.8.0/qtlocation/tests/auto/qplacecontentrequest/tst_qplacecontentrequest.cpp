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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <qplacecontentrequest.h>

QT_USE_NAMESPACE

class tst_QPlaceContentRequest : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceContentRequest();

private Q_SLOTS:
    void contentTest();
    void clearTest();
};

tst_QPlaceContentRequest::tst_QPlaceContentRequest()
{
}

void tst_QPlaceContentRequest::contentTest()
{
    QPlaceContentRequest req;
    QCOMPARE(req.limit(), -1);
    QCOMPARE(req.contentType(), QPlaceContent::NoType);

    //check that we can set the request fields
    req.setLimit(100);
    req.setContentType(QPlaceContent::ImageType);
    QCOMPARE(req.limit(), 100);
    QCOMPARE(req.contentType(), QPlaceContent::ImageType);

    //check that we assignment works correctly
    QPlaceContentRequest otherReq;
    otherReq.setLimit(10);
    otherReq.setContentType(QPlaceContent::ReviewType);
    req = otherReq;
    QCOMPARE(req.limit(), 10);
    QCOMPARE(req.contentType(), QPlaceContent::ReviewType);
    QCOMPARE(req, otherReq);

   //check that comparison will fail if one the fields are different
    req.setLimit(9000);
    QVERIFY(req != otherReq);
}

void tst_QPlaceContentRequest::clearTest()
{
    QPlaceContentRequest req;
    req.setContentType(QPlaceContent::ReviewType);
    req.setLimit(9000);
    req.clear();
    QVERIFY(req.contentType() == QPlaceContent::NoType);
    QVERIFY(req.limit() == -1);
}

QTEST_APPLESS_MAIN(tst_QPlaceContentRequest)

#include "tst_qplacecontentrequest.moc"
