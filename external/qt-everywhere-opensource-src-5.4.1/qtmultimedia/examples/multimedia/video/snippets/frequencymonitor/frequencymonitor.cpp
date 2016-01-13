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

#include "frequencymonitor.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QTime>
#include <QTimer>

//#define VERBOSE_TRACE

inline QDebug qtTrace() { return qDebug() << "[frequencymonitor]"; }
#ifdef VERBOSE_TRACE
inline QDebug qtVerboseTrace() { return qtTrace(); }
#else
inline QNoDebug qtVerboseTrace() { return QNoDebug(); }
#endif

static const int DefaultSamplingInterval = 100;
static const int DefaultTraceInterval = 0;

class FrequencyMonitorPrivate : public QObject
{
    Q_OBJECT

public:
    FrequencyMonitorPrivate(FrequencyMonitor *parent);
    void calculateInstantaneousFrequency();

private slots:
    void calculateAverageFrequency();
    void stalled();

public:
    FrequencyMonitor *const q_ptr;
    QString m_label;
    bool m_active;
    qreal m_instantaneousFrequency;
    QElapsedTimer m_instantaneousElapsed;
    QTimer *m_averageTimer;
    QElapsedTimer m_averageElapsed;
    int m_count;
    qreal m_averageFrequency;
    QTimer *m_traceTimer;
    QTimer *m_stalledTimer;
};

FrequencyMonitorPrivate::FrequencyMonitorPrivate(FrequencyMonitor *parent)
:   QObject(parent)
,   q_ptr(parent)
,   m_active(false)
,   m_instantaneousFrequency(0)
,   m_averageTimer(new QTimer(this))
,   m_count(0)
,   m_averageFrequency(0)
,   m_traceTimer(new QTimer(this))
,   m_stalledTimer(new QTimer(this))
{
    m_instantaneousElapsed.start();
    connect(m_averageTimer, SIGNAL(timeout()),
            this, SLOT(calculateAverageFrequency()));
    if (DefaultSamplingInterval)
        m_averageTimer->start(DefaultSamplingInterval);
    m_averageElapsed.start();
    connect(m_traceTimer, SIGNAL(timeout()),
            q_ptr, SLOT(trace()));
    if (DefaultTraceInterval)
        m_traceTimer->start(DefaultTraceInterval);
    m_stalledTimer->setSingleShot(true);
    connect(m_stalledTimer, SIGNAL(timeout()),
            this, SLOT(stalled()));
}

void FrequencyMonitorPrivate::calculateInstantaneousFrequency()
{
    const qint64 ms = m_instantaneousElapsed.restart();
    m_instantaneousFrequency = ms ? qreal(1000) / ms : 0;
    m_stalledTimer->start(3 * ms);
    if (m_instantaneousFrequency)
        q_ptr->setActive(true);
    emit q_ptr->instantaneousFrequencyChanged(m_instantaneousFrequency);
    emit q_ptr->frequencyChanged();
}

void FrequencyMonitorPrivate::calculateAverageFrequency()
{
    const qint64 ms = m_averageElapsed.restart();
    m_averageFrequency = qreal(m_count * 1000) / ms;
    emit q_ptr->averageFrequencyChanged(m_averageFrequency);
    emit q_ptr->frequencyChanged();
    m_count = 0;
}

void FrequencyMonitorPrivate::stalled()
{
    if (m_instantaneousFrequency) {
        m_instantaneousFrequency = 0;
        emit q_ptr->instantaneousFrequencyChanged(m_instantaneousFrequency);
        emit q_ptr->frequencyChanged();
    }
}

FrequencyMonitor::FrequencyMonitor(QObject *parent)
:   QObject(parent)
{
    d_ptr = new FrequencyMonitorPrivate(this);
}

FrequencyMonitor::~FrequencyMonitor()
{

}

QString FrequencyMonitor::label() const
{
    return d_func()->m_label;
}

bool FrequencyMonitor::active() const
{
    return d_func()->m_active;
}

int FrequencyMonitor::samplingInterval() const
{
    return d_ptr->m_averageTimer->isActive() ? d_ptr->m_averageTimer->interval() : 0;
}

int FrequencyMonitor::traceInterval() const
{
    return d_ptr->m_traceTimer->isActive() ? d_ptr->m_traceTimer->interval() : 0;
}

qreal FrequencyMonitor::instantaneousFrequency() const
{
    return d_func()->m_instantaneousFrequency;
}

qreal FrequencyMonitor::averageFrequency() const
{
    return d_func()->m_averageFrequency;
}

void FrequencyMonitor::notify()
{
    Q_D(FrequencyMonitor);
    ++(d->m_count);
    d->calculateInstantaneousFrequency();
}

void FrequencyMonitor::trace()
{
    Q_D(FrequencyMonitor);
    const QString value = QString::fromLatin1("instant %1 average %2")
                            .arg(d->m_instantaneousFrequency, 0, 'f', 2)
                            .arg(d->m_averageFrequency, 0, 'f', 2);
    if (d->m_label.isEmpty())
        qtTrace() << "FrequencyMonitor::trace" << value;
    else
        qtTrace() << "FrequencyMonitor::trace" << "label" << d->m_label << value;
}

void FrequencyMonitor::setLabel(const QString &value)
{
    Q_D(FrequencyMonitor);
    if (d->m_label != value) {
        d->m_label = value;
        emit labelChanged(d->m_label);
    }
}

void FrequencyMonitor::setActive(bool value)
{
    Q_D(FrequencyMonitor);
    if (d->m_active != value) {
        d->m_active = value;
        emit activeChanged(d->m_active);
    }
}

void FrequencyMonitor::setSamplingInterval(int value)
{
    Q_D(FrequencyMonitor);
    if (samplingInterval() != value) {
        if (value) {
            d->m_averageTimer->setInterval(value);
            d->m_averageTimer->start();
        } else {
            d->m_averageTimer->stop();
        }
        emit samplingIntervalChanged(value);
    }
}

void FrequencyMonitor::setTraceInterval(int value)
{
    Q_D(FrequencyMonitor);
    if (traceInterval() != value) {
        if (value) {
            d->m_traceTimer->setInterval(value);
            d->m_traceTimer->start();
        } else {
            d->m_traceTimer->stop();
        }
        emit traceIntervalChanged(value);
    }
}

#include "frequencymonitor.moc"
