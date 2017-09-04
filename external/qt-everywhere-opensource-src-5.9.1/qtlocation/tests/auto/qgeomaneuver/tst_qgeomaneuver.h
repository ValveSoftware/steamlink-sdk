/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef TST_QGEOMANEUVER_H
#define TST_QGEOMANEUVER_H


#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QMetaType>
#include <QSignalSpy>

#include "../utils/qlocationtestutils_p.h"
#include <qgeomaneuver.h>
#include <qgeocoordinate.h>


class tst_QGeoManeuver : public QObject
{
    Q_OBJECT

public:
    tst_QGeoManeuver();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    //Start unit test for QGeoRouteManeuver
    void constructor();
    void copy_constructor();
    void destructor();
    void direction();
    void direction_data();
    void distanceToNextInstruction();
    void instructionText();
    void position();
    void position_data();
    void timeToNextInstruction();
    void waypoint();
    void waypoint_data();
    void isValid();
    void operators();
    //End Unit Test for QGeoRouteManeuver

private:
    QGeoManeuver *qgeomaneuver;
    QGeoCoordinate *qgeocoordinate;

};

Q_DECLARE_METATYPE( QList<double>);
Q_DECLARE_METATYPE (QGeoManeuver::InstructionDirection);

#endif // TST_QGEOMANEUVER_H

