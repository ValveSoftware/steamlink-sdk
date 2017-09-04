/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <QtTest/qtest.h>
#include <Qt3DAnimation/qmorphtarget.h>
#include <qobject.h>
#include <qsignalspy.h>

class tst_QMorphTarget : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QMorphTarget morphTarget;

        // THEN
        QCOMPARE(morphTarget.attributeNames(), QStringList());
    }

    void testSetAttributes()
    {
        // GIVEN
        Qt3DAnimation::QMorphTarget morphTarget;
        Qt3DRender::QAttribute attribute1;
        Qt3DRender::QAttribute attribute2;

        attribute1.setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        attribute2.setName(Qt3DRender::QAttribute::defaultNormalAttributeName());

        {
            // WHEN
            morphTarget.addAttribute(&attribute1);

            // THEN
            QStringList names = morphTarget.attributeNames();
            QCOMPARE(names.size(), 1);
            QCOMPARE(names.at(0), Qt3DRender::QAttribute::defaultPositionAttributeName());
        }

        {
            // WHEN
            morphTarget.addAttribute(&attribute2);

            // THEN
            QStringList names = morphTarget.attributeNames();
            QCOMPARE(names.size(), 2);
            QCOMPARE(names.at(1), Qt3DRender::QAttribute::defaultNormalAttributeName());
        }
    }

    void testFromGeometry()
    {
        // GIVEN
        Qt3DRender::QGeometry geometry;
        Qt3DRender::QAttribute *attribute1 = new Qt3DRender::QAttribute;
        Qt3DRender::QAttribute *attribute2 = new Qt3DRender::QAttribute;
        attribute1->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        attribute2->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        geometry.addAttribute(attribute1);
        geometry.addAttribute(attribute2);

        QStringList attributes;
        attributes.push_back(Qt3DRender::QAttribute::defaultPositionAttributeName());

        {
            // WHEN
            QScopedPointer<Qt3DAnimation::QMorphTarget> morphTarget(
                        Qt3DAnimation::QMorphTarget::fromGeometry(&geometry, attributes));

            // THEN
            QStringList names = morphTarget->attributeNames();
            QCOMPARE(names.size(), 1);
            QCOMPARE(names.at(0), Qt3DRender::QAttribute::defaultPositionAttributeName());
        }

        {
            // WHEN
            attributes.push_back(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
            QScopedPointer<Qt3DAnimation::QMorphTarget> morphTarget(
                        Qt3DAnimation::QMorphTarget::fromGeometry(&geometry, attributes));

            // THEN
            QStringList names = morphTarget->attributeNames();
            QCOMPARE(names.size(), 2);
            QCOMPARE(names.at(0), Qt3DRender::QAttribute::defaultPositionAttributeName());
            QCOMPARE(names.at(1), Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        }
    }

};

QTEST_APPLESS_MAIN(tst_QMorphTarget)

#include "tst_qmorphtarget.moc"
