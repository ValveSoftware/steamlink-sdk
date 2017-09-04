/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "rep_enums_replica.h"

#include <QTest>

#include <QByteArray>
#include <QDataStream>

class tst_Enums : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testMarshalling();
};

void tst_Enums::testMarshalling()
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::ReadWrite);

    {
        const Qt::DateFormat format1 = Qt::TextDate;
        const Qt::DateFormat format2 = Qt::ISODate;
        const Qt::DateFormat format3 = Qt::SystemLocaleShortDate;
        const Qt::DateFormat format4 = Qt::SystemLocaleLongDate;
        const Qt::DateFormat format5 = Qt::DefaultLocaleShortDate;
        const Qt::DateFormat format6 = Qt::DefaultLocaleLongDate;
        const Qt::DateFormat format7 = Qt::SystemLocaleDate;

        ds << format1 << format2 << format3 << format4 << format5 << format6 << format7;
    }

    ds.device()->seek(0);

    {
        Qt::DateFormat format1, format2, format3, format4, format5, format6, format7;

        ds >> format1 >> format2 >> format3 >> format4 >> format5 >> format6 >> format7;

        QCOMPARE(format1, Qt::TextDate);
        QCOMPARE(format2, Qt::ISODate);
        QCOMPARE(format3, Qt::SystemLocaleShortDate);
        QCOMPARE(format4, Qt::SystemLocaleLongDate);
        QCOMPARE(format5, Qt::DefaultLocaleShortDate);
        QCOMPARE(format6, Qt::DefaultLocaleLongDate);
        QCOMPARE(format7, Qt::SystemLocaleDate);
    }
}

QTEST_APPLESS_MAIN(tst_Enums)

#include "tst_enums.moc"
