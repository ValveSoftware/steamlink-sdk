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

#include "rep_preprocessortest_merged.h"

#include <QTest>

class tst_RepCodeGenerator : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testPreprocessorTestFile();
};

void tst_RepCodeGenerator::testPreprocessorTestFile()
{
    // could be a compile-time test, but let's just do something here
    QVERIFY(PREPROCESSORTEST_MACRO);
}

QTEST_APPLESS_MAIN(tst_RepCodeGenerator)

#include "tst_repcodegenerator.moc"
