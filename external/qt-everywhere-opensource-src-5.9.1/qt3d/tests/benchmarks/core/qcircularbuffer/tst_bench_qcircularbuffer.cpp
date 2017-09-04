/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QObject>
#include <QtTest/QtTest>

#include <Qt3DCore/private/qcircularbuffer_p.h>

using namespace Qt3DCore;

class tst_QCircularBuffer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void benchmarkAppend();
    void benchmarkAt();
};

void tst_QCircularBuffer::benchmarkAppend()
{
    QCircularBuffer<float> c;
    c.setCapacity(100000);
    volatile int confuseTheOptimizer = 90000;
    QBENCHMARK {
        const int count = confuseTheOptimizer;
        for (int i = 0; i < count; i++) {
            c.append(float(i));
        }
    }
    const int count = confuseTheOptimizer;
    volatile float f;
    for (int i = 0; i < count; i++) {
        f = c.at(i);
    }
    Q_UNUSED(f);
}

void tst_QCircularBuffer::benchmarkAt()
{
    QCircularBuffer<float> c;
    c.setCapacity(20000);
    volatile int confuseTheOptimizer = 90000;
    const int count = confuseTheOptimizer;
    for (int i = 0; i < count; i++) {
        c.append(float(i));
    }
    QCOMPARE(c.isLinearised(), false);
    QBENCHMARK {
        const int count = c.size();
        volatile float f;
        for (int i = 0; i < count; i++) {
            f = c.at(i);
        }
        Q_UNUSED(f);
    }
}

QTEST_APPLESS_MAIN(tst_QCircularBuffer)
#include "tst_bench_qcircularbuffer.moc"
