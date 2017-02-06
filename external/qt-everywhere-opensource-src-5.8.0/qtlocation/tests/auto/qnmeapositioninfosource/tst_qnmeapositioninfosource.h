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

#include "qnmeapositioninfosourceproxyfactory.h"
#include "../qgeopositioninfosource/testqgeopositioninfosource_p.h"
#include "../utils/qlocationtestutils_p.h"

#include <QtPositioning/qnmeapositioninfosource.h>

#include <QTest>
#include <QDir>
#include <QDebug>
#include <QBuffer>
#include <QSignalSpy>
#include <QMetaType>
#include <QFile>
#include <QTemporaryFile>
#include <QHash>
#include <QTimer>

QT_USE_NAMESPACE
Q_DECLARE_METATYPE(QNmeaPositionInfoSource::UpdateMode)
Q_DECLARE_METATYPE(QGeoPositionInfo)
Q_DECLARE_METATYPE(QList<QDateTime>)

class tst_QNmeaPositionInfoSource : public QObject
{
    Q_OBJECT

public:
    enum UpdateTriggerMethod
    {
        StartUpdatesMethod,
        RequestUpdatesMethod
    };

    tst_QNmeaPositionInfoSource(QNmeaPositionInfoSource::UpdateMode mode, QObject *parent = 0);

private:
    QList<QDateTime> createDateTimes(int count) const
    {
        QList<QDateTime> dateTimes;
        QDateTime dt = QDateTime::currentDateTime().toUTC();
        int interval = 100;
        for (int i=0; i<count; i++) {
            dateTimes << dt.addMSecs(interval);
            interval += 100;
        }
        return dateTimes;
    }

private slots:
    void initTestCase();

    void constructor();

    void supportedPositioningMethods();

    void minimumUpdateInterval();

    void userEquivalentRangeError();

    void setUpdateInterval_delayedUpdate();

    void lastKnownPosition();

    void beginWithBufferedData();
    void beginWithBufferedData_data();

    void startUpdates();
    void startUpdates_data();

    void startUpdates_withTimeout();

    void startUpdates_expectLatestUpdateOnly();

    void startUpdates_waitForValidDateTime();
    void startUpdates_waitForValidDateTime_data();

    void requestUpdate_waitForValidDateTime();
    void requestUpdate_waitForValidDateTime_data();

    void requestUpdate();
    void requestUpdate_after_start();

    void testWithBadNmea();
    void testWithBadNmea_data();

private:
    QNmeaPositionInfoSource::UpdateMode m_mode;
};

Q_DECLARE_METATYPE(tst_QNmeaPositionInfoSource::UpdateTriggerMethod)

//---------------------------------------------------

class Feeder : public QObject
{
    Q_OBJECT

public:
    Feeder(QObject *parent)
        : QObject(parent)
    {
    }

    void start(QNmeaPositionInfoSourceProxy *proxy)
    {
        m_proxy = proxy;
        QTimer *timer = new QTimer;
        QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
        timer->setInterval(proxy->source()->minimumUpdateInterval()*2);
        timer->start();
    }

public slots:
    void timeout()
    {
        m_proxy->feedBytes(QLocationTestUtils::createRmcSentence(QDateTime::currentDateTime()).toLatin1());
    }

private:
    QNmeaPositionInfoSourceProxy *m_proxy;
};

//---------------------------------------------------


class UnlimitedNmeaStream : public QIODevice
{
    Q_OBJECT

public:
    UnlimitedNmeaStream(QObject *parent) : QIODevice(parent) {}

protected:
    qint64 readData(char *data, qint64 maxSize)
    {
        QByteArray bytes = QLocationTestUtils::createRmcSentence(QDateTime::currentDateTime()).toLatin1();
        qint64 sz = qMin(qint64(bytes.size()), maxSize);
        memcpy(data, bytes.constData(), sz);
        return sz;
    }

    qint64 writeData(const char *, qint64)
    {
        return -1;
    }

    qint64 bytesAvailable() const
    {
        return 1024 + QIODevice::bytesAvailable();
    }
};
