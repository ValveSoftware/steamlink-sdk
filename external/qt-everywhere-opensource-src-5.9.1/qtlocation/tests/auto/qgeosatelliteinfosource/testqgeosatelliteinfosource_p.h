/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef TESTQGEOSATELLITEINFOSOURCE_H
#define TESTQGEOSATELLITEINFOSOURCE_H

#include <QTest>
#include <QObject>

QT_BEGIN_NAMESPACE
class QGeoSatelliteInfoSource;
QT_END_NAMESPACE


class TestQGeoSatelliteInfoSource : public QObject
{
    Q_OBJECT

public:
    TestQGeoSatelliteInfoSource(QObject *parent = 0);

    static TestQGeoSatelliteInfoSource *createDefaultSourceTest();

protected:
    virtual QGeoSatelliteInfoSource *createTestSource() = 0;

    // MUST be called by subclasses if they override respective test slots
    void base_initTestCase();
    void base_init();
    void base_cleanup();
    void base_cleanupTestCase();

public slots:
    void test_slot1();
    void test_slot2();

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void constructor_withParent();
    void constructor_noParent();

    void updateInterval();

    void setUpdateInterval();
    void setUpdateInterval_data();

    void minimumUpdateInterval();

    void createDefaultSource();
    void createDefaultSource_noParent();

    void startUpdates_testIntervals();
    void startUpdates_testIntervalChangesWhileRunning();
    void startUpdates_testDefaultInterval();
    void startUpdates_testZeroInterval();
    void startUpdates_moreThanOnce();
    void stopUpdates();
    void stopUpdates_withoutStart();

    void requestUpdate();
    void requestUpdate_data();

    void requestUpdate_validTimeout();
    void requestUpdate_defaultTimeout();
    void requestUpdate_timeoutLessThanMinimumInterval();
    void requestUpdate_repeatedCalls();
    void requestUpdate_overlappingCalls();
    void requestUpdate_overlappingCallsWithTimeout();

    void requestUpdateAfterStartUpdates_ZeroInterval();
    void requestUpdateAfterStartUpdates_SmallInterval();
    void requestUpdateBeforeStartUpdates_ZeroInterval();
    void requestUpdateBeforeStartUpdates_SmallInterval();

    void removeSlotForRequestTimeout();
    void removeSlotForSatellitesInUseUpdated();
    void removeSlotForSatellitesInViewUpdated();

private:
    QGeoSatelliteInfoSource *m_source;
    bool m_testingDefaultSource;
    bool m_testSlot2Called;
};

#endif // #ifndef TESTQGEOSATELLITEINFOSOURCE_H
