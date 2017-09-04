/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <qbackendnodetester.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DInput/private/axis_p.h>
#include <Qt3DInput/private/axisaccumulator_p.h>
#include <Qt3DInput/private/axisaccumulatorjob_p.h>
#include <Qt3DInput/private/qabstractaxisinput_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/QAnalogAxisInput>
#include <Qt3DInput/QAxis>
#include <Qt3DInput/QAxisAccumulator>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"

class tst_AxisAccumulatorJob : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:
    void checkIntegration()
    {
        // GIVEN
        const auto sourceAxisType = Qt3DInput::QAxisAccumulator::Velocity;
        const float axisValue = 1.0f;
        const float scale = 10.0f;
        const float dt = 0.1f;
        const float valueResultEnabled = 1.0f;
        const float valueResultDisabled = 0.0f;

        // Set up an axis
        Qt3DInput::QAxis *axis = new Qt3DInput::QAxis;
        Qt3DInput::Input::AxisManager axisManager;
        Qt3DInput::Input::Axis *backendAxis = axisManager.getOrCreateResource(axis->id());
        backendAxis->setEnabled(true);
        backendAxis->setAxisValue(axisValue);

        // Hook up a bunch of accumulators to this axis
        Qt3DInput::Input::AxisAccumulatorManager axisAccumulatorManager;
        QVector<Qt3DInput::Input::AxisAccumulator *> accumulators;
        for (int i = 0; i < 10; ++i) {
            auto axisAccumulator = new Qt3DInput::QAxisAccumulator;
            Qt3DInput::Input::AxisAccumulator *backendAxisAccumulator
                    = axisAccumulatorManager.getOrCreateResource(axisAccumulator->id());
            accumulators.push_back(backendAxisAccumulator);

            axisAccumulator->setEnabled(i % 2 == 0); // Enable only even accumulators
            axisAccumulator->setSourceAxis(axis);
            axisAccumulator->setScale(scale);
            axisAccumulator->setSourceAxisType(sourceAxisType);
            simulateInitialization(axisAccumulator, backendAxisAccumulator);
        }

        // WHEN
        Qt3DInput::Input::AxisAccumulatorJob job(&axisAccumulatorManager, &axisManager);
        job.setDeltaTime(dt);
        job.run();


        // THEN
        for (const auto accumulator : accumulators) {
            QCOMPARE(accumulator->value(), accumulator->isEnabled() ? valueResultEnabled
                                                                    : valueResultDisabled);
        }
    }
};

QTEST_APPLESS_MAIN(tst_AxisAccumulatorJob)

#include "tst_axisaccumulatorjob.moc"
