/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
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
#include <QtTest/qtestcase.h>
#include <QtTest/QSignalSpy>
#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>

#include "private/qdefaultmaskgenerator_p.h"

QT_USE_NAMESPACE

class tst_QDefaultMaskGenerator : public QObject
{
    Q_OBJECT

public:
    tst_QDefaultMaskGenerator();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void tst_randomnessWithoutSeed();
    void tst_randomnessWithSeed();

private:
};



tst_QDefaultMaskGenerator::tst_QDefaultMaskGenerator()
{
}

void tst_QDefaultMaskGenerator::initTestCase()
{
}

void tst_QDefaultMaskGenerator::cleanupTestCase()
{
}

void tst_QDefaultMaskGenerator::init()
{
}

void tst_QDefaultMaskGenerator::cleanup()
{
}

void tst_QDefaultMaskGenerator::tst_randomnessWithoutSeed()
{
    //generate two series of data, and see if they differ
    {
        QDefaultMaskGenerator generator;

        QVector<quint32> series1, series2;
        for (int i = 0; i < 1000; ++i)
            series1 << generator.nextMask();
        for (int i = 0; i < 1000; ++i)
            series2 << generator.nextMask();

        QVERIFY(series1 != series2);
    }
}

void tst_QDefaultMaskGenerator::tst_randomnessWithSeed()
{
    //generate two series of data, and see if they differ
    //the generator is seeded
    {
        QDefaultMaskGenerator generator;
        generator.seed();

        QVector<quint32> series1, series2;
        for (int i = 0; i < 1000; ++i)
            series1 << generator.nextMask();
        for (int i = 0; i < 1000; ++i)
            series2 << generator.nextMask();

        QVERIFY(series1 != series2);
    }
    //generates two series of data with 2 distinct generators
    //both generators are seeded
    {
        QDefaultMaskGenerator generator1, generator2;
        generator1.seed();
        generator2.seed();

        QVector<quint32> series1, series2;
        for (int i = 0; i < 1000; ++i) {
            series1 << generator1.nextMask();
            series2 << generator2.nextMask();
        }

        QVERIFY(series1 != series2);
    }
    //generates two series of data with 2 distinct generators
    //only one of the two is seeded
    {
        QDefaultMaskGenerator generator1, generator2;
        generator1.seed();

        QVector<quint32> series1, series2;
        for (int i = 0; i < 1000; ++i) {
            series1 << generator1.nextMask();
            series2 << generator2.nextMask();
        }

        QVERIFY(series1 != series2);
    }
}

QTEST_MAIN(tst_QDefaultMaskGenerator)

#include "tst_defaultmaskgenerator.moc"
