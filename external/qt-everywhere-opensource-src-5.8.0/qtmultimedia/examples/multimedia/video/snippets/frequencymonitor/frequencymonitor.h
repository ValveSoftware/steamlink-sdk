/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
