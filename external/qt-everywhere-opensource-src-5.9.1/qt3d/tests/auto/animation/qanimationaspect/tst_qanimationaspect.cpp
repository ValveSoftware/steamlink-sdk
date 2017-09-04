/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include <Qt3DAnimation/qanimationaspect.h>
#include <Qt3DCore/qaspectengine.h>

class tst_QAnimationAspect: public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void checkConstruction()
    {
        // WHEN
        Qt3DAnimation::QAnimationAspect animationAspect;

        // THEN
        QVERIFY(animationAspect.parent() == nullptr);
        QCOMPARE(animationAspect.objectName(), QLatin1String("Animation Aspect"));
    }

    void checkRegistration()
    {
        {
            // GIVEN
            Qt3DCore::QAspectEngine engine;

            // WHEN
            auto animationAspect = new Qt3DAnimation::QAnimationAspect;
            engine.registerAspect(animationAspect);

            // THEN
            QCOMPARE(engine.aspects().size(), 1);
            QCOMPARE(engine.aspects().first(), animationAspect);

            // WHEN
            engine.unregisterAspect(animationAspect);

            // THEN
            QCOMPARE(engine.aspects().size(), 0);
        }

        {
            // GIVEN
            Qt3DCore::QAspectEngine engine;

            // WHEN
            engine.registerAspect(QLatin1String("animation"));

            // THEN
            QCOMPARE(engine.aspects().size(), 1);
            QCOMPARE(engine.aspects().first()->objectName(), QLatin1String("Animation Aspect"));

            // WHEN
            engine.unregisterAspect(QLatin1String("animation"));

            // THEN
            QCOMPARE(engine.aspects().size(), 0);
        }
    }
};

QTEST_MAIN(tst_QAnimationAspect)

#include "tst_qanimationaspect.moc"
