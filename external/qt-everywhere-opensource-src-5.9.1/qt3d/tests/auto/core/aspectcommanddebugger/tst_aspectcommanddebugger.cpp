/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtTest/QTest>
#include <Qt3DCore/private/aspectcommanddebugger_p.h>

#ifdef QT3D_JOBS_RUN_STATS
using namespace Qt3DCore::Debug;
#endif

class tst_AspectCommandDebugger : public QObject
{
    Q_OBJECT

private slots:
#ifdef QT3D_JOBS_RUN_STATS
    void checkReadBufferInitialState()
    {
        // GIVEN
        AspectCommandDebugger::ReadBuffer buffer;

        // THEN
        QCOMPARE(buffer.startIdx, 0);
        QCOMPARE(buffer.endIdx, 0);
    }

    void checkReadBufferInsert()
    {
        // GIVEN
        AspectCommandDebugger::ReadBuffer buffer;

        // WHEN
        QByteArray fakeData(1024, '8');
        buffer.insert(fakeData);

        // THEN
        QCOMPARE(buffer.startIdx, 0);
        QCOMPARE(buffer.endIdx, 1024);
        QCOMPARE(buffer.buffer.size(), 1024);
        QCOMPARE(fakeData, QByteArray(buffer.buffer.constData(), 1024));

        // WHEN
        QByteArray hugeFakeData(1024 * 16, '.');
        buffer.insert(hugeFakeData);

        // THEN
        QCOMPARE(buffer.startIdx, 0);
        QCOMPARE(buffer.endIdx, 1024 + 16 * 1024);
        QCOMPARE(fakeData, QByteArray(buffer.buffer.constData(), 1024));
        QCOMPARE(hugeFakeData, QByteArray(buffer.buffer.constData() + 1024, 1024 * 16));
    }

    void checkBufferTrim()
    {
        // GIVEN
        AspectCommandDebugger::ReadBuffer buffer;
        QByteArray fakeData(1024, '9');
        QByteArray hugeFakeData(1024 * 16, '.');
        buffer.insert(fakeData);
        buffer.insert(hugeFakeData);

        // WHEN
        buffer.startIdx += 1024;
        buffer.trim();

        // THEN
        QCOMPARE(buffer.size(), 16 * 1024);
        QCOMPARE(hugeFakeData, QByteArray(buffer.buffer.constData(), 1024 * 16));
    }
#endif
};

QTEST_APPLESS_MAIN(tst_AspectCommandDebugger)

#include "tst_aspectcommanddebugger.moc"
