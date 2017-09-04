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
#include <Qt3DRender/private/uniform_p.h>

using namespace Qt3DRender;
using namespace Qt3DRender::Render;

class tst_Uniform : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        UniformValue v;
        // THEN
        QVERIFY(v.constData<float>()[0] == 0.0f);
        QVERIFY(v.constData<float>()[1] == 0.0f);
        QVERIFY(v.constData<float>()[2] == 0.0f);
        QVERIFY(v.constData<float>()[3] == 0.0f);
    }

    void checkDefaultCTors()
    {
        {
            // GIVEN
            UniformValue v(883);
            // THEN
            QCOMPARE(v.constData<int>()[0], 883);
            QCOMPARE(v.constData<int>()[1], 0);
            QCOMPARE(v.constData<int>()[2], 0);
            QCOMPARE(v.constData<int>()[3], 0);
        }
        {
            // GIVEN
            UniformValue v(1584U);
            // THEN
            QCOMPARE(v.constData<uint>()[0], 1584U);
            QCOMPARE(v.constData<uint>()[1], 0U);
            QCOMPARE(v.constData<uint>()[2], 0U);
            QCOMPARE(v.constData<uint>()[3], 0U);
        }
        {
            // GIVEN
            UniformValue v(454.0f);
            // THEN
            QCOMPARE(v.constData<float>()[0], 454.0f);
            QCOMPARE(v.constData<float>()[1], 0.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v(350.0);
            // THEN
            // Note: Uniform value does a double -> float conversion
            QCOMPARE(v.constData<float>()[0], 350.0f);
            QCOMPARE(v.constData<float>()[1], 0.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v(true);
            // THEN
            QCOMPARE(v.constData<bool>()[0], true);
            QCOMPARE(v.constData<bool>()[1], false);
            QCOMPARE(v.constData<bool>()[2], false);
            QCOMPARE(v.constData<bool>()[3], false);
        }
        {
            // GIVEN
            UniformValue v(QVector2D(355.0f, 383.0f));
            // THEN
            QCOMPARE(v.constData<float>()[0], 355.0f);
            QCOMPARE(v.constData<float>()[1], 383.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v(QVector3D(572.0f, 355.0f, 383.0f));
            // THEN
            QCOMPARE(v.constData<float>()[0], 572.0f);
            QCOMPARE(v.constData<float>()[1], 355.0f);
            QCOMPARE(v.constData<float>()[2], 383.0f);
            QCOMPARE(v.constData<float>()[4], 0.0f);
        }
        {
            // GIVEN
            UniformValue v(QVector4D(355.0f, 383.0f, 1340.0f, 1603.0f));
            // THEN
            QCOMPARE(v.constData<float>()[0], 355.0f);
            QCOMPARE(v.constData<float>()[1], 383.0f);
            QCOMPARE(v.constData<float>()[2], 1340.0f);
            QCOMPARE(v.constData<float>()[3], 1603.0f);
        }
    }

    void checkFromVariant()
    {
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant(883));
            // THEN
            QCOMPARE(v.constData<int>()[0], 883);
            QCOMPARE(v.constData<int>()[1], 0);
            QCOMPARE(v.constData<int>()[2], 0);
            QCOMPARE(v.constData<int>()[3], 0);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant(1584U));
            // THEN
            QCOMPARE(v.constData<uint>()[0], 1584U);
            QCOMPARE(v.constData<uint>()[1], 0U);
            QCOMPARE(v.constData<uint>()[2], 0U);
            QCOMPARE(v.constData<uint>()[3], 0U);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant(454.0f));
            // THEN
            QCOMPARE(v.constData<float>()[0], 454.0f);
            QCOMPARE(v.constData<float>()[1], 0.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant(350.0));
            // THEN
            // Note: Uniform value does a double -> float conversion
            QCOMPARE(v.constData<float>()[0], 350.0f);
            QCOMPARE(v.constData<float>()[1], 0.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant(true));
            // THEN
            QCOMPARE(v.constData<bool>()[0], true);
            QCOMPARE(v.constData<bool>()[1], false);
            QCOMPARE(v.constData<bool>()[2], false);
            QCOMPARE(v.constData<bool>()[3], false);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QVector2D(355.0f, 383.0f)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 355.0f);
            QCOMPARE(v.constData<float>()[1], 383.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QVector3D(572.0f, 355.0f, 383.0f)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 572.0f);
            QCOMPARE(v.constData<float>()[1], 355.0f);
            QCOMPARE(v.constData<float>()[2], 383.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QVector4D(355.0f, 383.0f, 1340.0f, 1603.0f)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 355.0f);
            QCOMPARE(v.constData<float>()[1], 383.0f);
            QCOMPARE(v.constData<float>()[2], 1340.0f);
            QCOMPARE(v.constData<float>()[3], 1603.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QPoint(427, 396)));
            // THEN
            QCOMPARE(v.constData<int>()[0], 427);
            QCOMPARE(v.constData<int>()[1], 396);
            QCOMPARE(v.constData<int>()[2], 0);
            QCOMPARE(v.constData<int>()[3], 0);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QSize(427, 396)));
            // THEN
            QCOMPARE(v.constData<int>()[0], 427);
            QCOMPARE(v.constData<int>()[1], 396);
            QCOMPARE(v.constData<int>()[2], 0);
            QCOMPARE(v.constData<int>()[3], 0);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QRect(427, 396, 454, 1584)));
            // THEN
            QCOMPARE(v.constData<int>()[0], 427);
            QCOMPARE(v.constData<int>()[1], 396);
            QCOMPARE(v.constData<int>()[2], 454);
            QCOMPARE(v.constData<int>()[3], 1584);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QPointF(427, 396)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 427.0f);
            QCOMPARE(v.constData<float>()[1], 396.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QSizeF(427, 396)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 427.0f);
            QCOMPARE(v.constData<float>()[1], 396.0f);
            QCOMPARE(v.constData<float>()[2], 0.0f);
            QCOMPARE(v.constData<float>()[3], 0.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QRectF(427, 396, 454, 1584)));
            // THEN
            QCOMPARE(v.constData<float>()[0], 427.0f);
            QCOMPARE(v.constData<float>()[1], 396.0f);
            QCOMPARE(v.constData<float>()[2], 454.0f);
            QCOMPARE(v.constData<float>()[3], 1584.0f);
        }
        {
            // GIVEN
            UniformValue v = UniformValue::fromVariant(QVariant::fromValue(QMatrix4x4()));
            // THEN
            QCOMPARE(v.constData<float>()[ 0], 1.0f);
            QCOMPARE(v.constData<float>()[ 1], 0.0f);
            QCOMPARE(v.constData<float>()[ 2], 0.0f);
            QCOMPARE(v.constData<float>()[ 3], 0.0f);

            QCOMPARE(v.constData<float>()[ 4], 0.0f);
            QCOMPARE(v.constData<float>()[ 5], 1.0f);
            QCOMPARE(v.constData<float>()[ 6], 0.0f);
            QCOMPARE(v.constData<float>()[ 7], 0.0f);

            QCOMPARE(v.constData<float>()[ 8], 0.0f);
            QCOMPARE(v.constData<float>()[ 9], 0.0f);
            QCOMPARE(v.constData<float>()[10], 1.0f);
            QCOMPARE(v.constData<float>()[11], 0.0f);

            QCOMPARE(v.constData<float>()[12], 0.0f);
            QCOMPARE(v.constData<float>()[13], 0.0f);
            QCOMPARE(v.constData<float>()[14], 0.0f);
            QCOMPARE(v.constData<float>()[15], 1.0f);
        }
        {
            // GIVEN
            QVariant variants = QVariantList() << QVariant(427.0f) << QVariant(454.0f) << QVariant(883.0f) << QVariant(1340.0f);
            UniformValue v = UniformValue::fromVariant(variants);

            // THEN
            QCOMPARE(v.constData<float>()[0], 427.0f);
            QCOMPARE(v.constData<float>()[1], 454.0f);
            QCOMPARE(v.constData<float>()[2], 883.0f);
            QCOMPARE(v.constData<float>()[3], 1340.0f);
        }
        {
            // GIVEN
            QVariant variants = QVariantList() << QVariant::fromValue(QVector4D(2.0f, 16.0f, 8.0f, 4.0f)) << QVariant(QVector4D(3.0f, 24.0f, 12.0f, 6.0f));
            UniformValue v = UniformValue::fromVariant(variants);

            // THEN
            QCOMPARE(v.constData<float>()[0], 2.0f);
            QCOMPARE(v.constData<float>()[1], 16.0f);
            QCOMPARE(v.constData<float>()[2], 8.0f);
            QCOMPARE(v.constData<float>()[3], 4.0f);
            QCOMPARE(v.constData<float>()[4], 3.0f);
            QCOMPARE(v.constData<float>()[5], 24.0f);
            QCOMPARE(v.constData<float>()[6], 12.0f);
            QCOMPARE(v.constData<float>()[7], 6.0f);
        }
        {
            // GIVEN
            QVariant variants = QVariantList() << QVariant(427) << QVariant(454) << QVariant(883) << QVariant(1340);
            UniformValue v = UniformValue::fromVariant(variants);

            // THEN
            QCOMPARE(v.constData<int>()[0], 427);
            QCOMPARE(v.constData<int>()[1], 454);
            QCOMPARE(v.constData<int>()[2], 883);
            QCOMPARE(v.constData<int>()[3], 1340);
        }
    }

    void checkComparison()
    {
        // GIVEN
        const UniformValue v1(QVector3D(454.0f, 883.0f, 572.0f));
        UniformValue v2(454.0f);

        // THEN
        QVERIFY(!(v1 == v2));
        QVERIFY(v1 != v2);

        // WHEN
        v2 = UniformValue::fromVariant(QVector3D(454.0f, 883.0f, 572.0f));
        // THEN
        QVERIFY(v1 == v2);
        QVERIFY(!(v1 != v2));

        // WHEN
        v2 = UniformValue::fromVariant(QVector3D(454.0f, 883.0f, 572.0f));
        // THEN
        QVERIFY(v1 == v2);
        QVERIFY(!(v1 != v2));

        // WHEN
        v2 = UniformValue::fromVariant(454.0f);
        // THEN
        QVERIFY(!(v1 == v2));
        QVERIFY(v1 != v2);
    }
};


QTEST_APPLESS_MAIN(tst_Uniform)

#include "tst_uniform.moc"
