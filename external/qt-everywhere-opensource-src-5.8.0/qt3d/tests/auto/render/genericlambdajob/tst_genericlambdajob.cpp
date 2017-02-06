/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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
#include <Qt3DRender/private/genericlambdajob_p.h>
#include <functional>

namespace {

int randomTestValue = 0;

void freeStandingFunction()
{
    randomTestValue = 883;
}

} // anonymous

class tst_GenericLambdaJob : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void lambdaTest()
    {
        // GIVEN
        int a = 0;
        auto incremeneterLambda = [&] () { ++a; };
        Qt3DRender::Render::GenericLambdaJob<decltype(incremeneterLambda)> job(incremeneterLambda);

        // THEN
        QCOMPARE(a, 0);

        // WHEN
        job.run();

        // THEN
        QCOMPARE(a, 1);

        // WHEN
        job.run();
        QCOMPARE(a, 2);
    }

    void functionTest()
    {
        // GIVEN
        Qt3DRender::Render::GenericLambdaJob<decltype(&freeStandingFunction)> job(&freeStandingFunction);

        // THEN
        QCOMPARE(randomTestValue, 0);

        // WHEN
        job.run();

        // THEN
        QCOMPARE(randomTestValue, 883);
    }

    void stdFunctionTest()
    {
        // GIVEN
        int u = 1584;
        auto decrementerLambda = [&] () { u = 0; };
        std::function<void ()> func = decrementerLambda;
        Qt3DRender::Render::GenericLambdaJob<decltype(func)> job(func);

        // THEN
        QCOMPARE(u, 1584);

        // WHEN
        job.run();

        // THEN
        QCOMPARE(u, 0);
    }
};

QTEST_APPLESS_MAIN(tst_GenericLambdaJob)

#include "tst_genericlambdajob.moc"
