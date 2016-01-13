/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
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

#ifndef FREQUENCYMONITOR_H
#define FREQUENCYMONITOR_H

#include <QObject>
#include <QTimer>

class FrequencyMonitorPrivate;

/**
 * Class for measuring frequency of events
 *
 * Occurrence of the event is notified by the client via the notify() slot.
 * On a regular interval, both an instantaneous and a rolling average
 * event frequency are calculated.
 */
class FrequencyMonitor : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(FrequencyMonitor)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int samplingInterval READ samplingInterval WRITE setSamplingInterval NOTIFY samplingIntervalChanged)
    Q_PROPERTY(int traceInterval READ traceInterval WRITE setTraceInterval NOTIFY traceIntervalChanged)
    Q_PROPERTY(qreal instantaneousFrequency READ instantaneousFrequency NOTIFY instantaneousFrequencyChanged)
    Q_PROPERTY(qreal averageFrequency READ averageFrequency NOTIFY averageFrequencyChanged)

public:
    FrequencyMonitor(QObject *parent = 0);
    ~FrequencyMonitor();

    static void qmlRegisterType();

    QString label() const;
    bool active() const;
    int samplingInterval() const;
    int traceInterval() const;
    qreal instantaneousFrequency() const;
    qreal averageFrequency() const;

signals:
    void labelChanged(const QString &value);
    void activeChanged(bool);
    void samplingIntervalChanged(int value);
    void traceIntervalChanged(int value);
    void frequencyChanged();
    void instantaneousFrequencyChanged(qreal value);
    void averageFrequencyChanged(qreal value);

public slots:
    Q_INVOKABLE void notify();
    Q_INVOKABLE void trace();
    void setActive(bool value);
    void setLabel(const QString &value);
    void setSamplingInterval(int value);
    void setTraceInterval(int value);

private:
    FrequencyMonitorPrivate *d_ptr;
};

#endif // FREQUENCYMONITOR_H
