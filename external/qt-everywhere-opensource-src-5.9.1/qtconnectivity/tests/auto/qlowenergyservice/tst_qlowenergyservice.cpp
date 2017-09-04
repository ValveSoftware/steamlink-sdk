/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtTest/QtTest>

#include <QtBluetooth/qlowenergyservice.h>


/*
 * This is a very simple test despite the complexity of QLowEnergyService.
 * It mostly aims to test the static API behaviors of the class. The connection
 * orientated elements are test by the test for QLowEnergyController as it
 * is impossible to test the two classes separately from each other.
 */

class tst_QLowEnergyService : public QObject
{
    Q_OBJECT

private slots:
    void tst_flags();
};

void tst_QLowEnergyService::tst_flags()
{
    QLowEnergyService::ServiceTypes flag1(QLowEnergyService::PrimaryService);
    QLowEnergyService::ServiceTypes flag2(QLowEnergyService::IncludedService);
    QLowEnergyService::ServiceTypes result;

    // test QFlags &operator|=(QFlags f)
    result = flag1 | flag2;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));

    // test QFlags &operator|=(Enum f)
    result = flag1 | QLowEnergyService::IncludedService;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));

    // test Q_DECLARE_OPERATORS_FOR_FLAGS(QLowEnergyService::ServiceTypes)
    result = QLowEnergyService::IncludedService | flag1;
    QVERIFY(result.testFlag(QLowEnergyService::PrimaryService));
    QVERIFY(result.testFlag(QLowEnergyService::IncludedService));
}

QTEST_MAIN(tst_QLowEnergyService)

#include "tst_qlowenergyservice.moc"
