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

#ifndef MOCKRADIOTUNERCONTROL_H
#define MOCKRADIOTUNERCONTROL_H

#include "qradiotunercontrol.h"

class MockRadioTunerControl : public QRadioTunerControl
{
    Q_OBJECT

public:
    MockRadioTunerControl(QObject *parent):
        QRadioTunerControl(parent),
        m_active(false),
        m_searching(false),m_muted(false),m_stereo(true),
        m_volume(100),m_signal(0),m_frequency(0),
        m_band(QRadioTuner::FM),m_err(QRadioTuner::NoError),
        m_errstr("")
    {
    }

    using QRadioTunerControl::error;

    QRadioTuner::State state() const
    {
        return m_active ? QRadioTuner::ActiveState : QRadioTuner::StoppedState;
    }

    QRadioTuner::Band band() const
    {
        return m_band;
    }

    void setBand(QRadioTuner::Band b)
    {
        if (b == QRadioTuner::FM2 || b == QRadioTuner::LW) {
            m_err = QRadioTuner::OutOfRangeError;
            m_errstr.clear();
            m_errstr = QString("band and range not supported");
        } else {
            m_err = QRadioTuner::NoError;
            m_errstr.clear();
            m_band = b;
            emit bandChanged(m_band);
        }
        emit error(m_err);

    }

    bool isBandSupported(QRadioTuner::Band b) const
    {
        if (b == QRadioTuner::FM || b == QRadioTuner::AM)
            return true;

        return false;
    }

    int frequency() const
    {
        return m_frequency;
    }

    QPair<int,int> frequencyRange(QRadioTuner::Band) const
    {
        return qMakePair<int,int>(1,2);
    }

    int frequencyStep(QRadioTuner::Band) const
    {
        return 1;
    }

    void setFrequency(int frequency)
    {
        if (frequency >= 148500000) {
            m_err = QRadioTuner::OutOfRangeError;
            m_errstr.clear();
            m_errstr = QString("band and range not supported");
        } else {
            m_err = QRadioTuner::NoError;
            m_errstr.clear();
            m_frequency = frequency;
            emit frequencyChanged(m_frequency);
        }

        emit error(m_err);
    }

    bool isStereo() const
    {
        return m_stereo;
    }

    void setStereo(bool stereo)
    {
        emit stereoStatusChanged(m_stereo = stereo);
    }


    QRadioTuner::StereoMode stereoMode() const
    {
        return m_stereoMode;
    }

    void setStereoMode(QRadioTuner::StereoMode mode)
    {
        m_stereoMode = mode;
    }

    QRadioTuner::Error error() const
    {
        return m_err;
    }

    QString errorString() const
    {
        return m_errstr;
    }

    int signalStrength() const
    {
        return m_signal;
    }

    int volume() const
    {
        return m_volume;
    }

    void setVolume(int volume)
    {
        m_volume = volume;
        emit volumeChanged(m_volume);
    }

    bool isMuted() const
    {
        return m_muted;
    }

    void setMuted(bool muted)
    {
        m_muted = muted;
        emit mutedChanged(m_muted);
    }

    bool isSearching() const
    {
        return m_searching;
    }

    void searchForward()
    {
        m_searching = true;
        emit searchingChanged(m_searching);
    }

    void searchBackward()
    {
        m_searching = true;
        emit searchingChanged(m_searching);
    }

    void cancelSearch()
    {
        m_searching = false;
        emit searchingChanged(m_searching);
    }

    void findNewStation( int frequency, QString stationId )
    {
        setFrequency(frequency);
        emit stationFound( frequency, stationId );
    }

    void searchAllStations(QRadioTuner::SearchMode searchMode = QRadioTuner::SearchFast)
    {
        QString programmeIdentifiers[3] = { "", "", "" };

        if ( searchMode == QRadioTuner::SearchGetStationId ) {
            programmeIdentifiers[0] = QString("MockProgramPI1");
            programmeIdentifiers[1] = QString("MockProgramPI2");
            programmeIdentifiers[2] = QString("MockProgramPI3");
        }
        m_searching = true;
        emit searchingChanged(m_searching);

        findNewStation(88300000, programmeIdentifiers[0]);
        findNewStation(95100000, programmeIdentifiers[1]);
        findNewStation(103100000, programmeIdentifiers[2]);

        m_searching = false;
        emit searchingChanged(m_searching);
    }

    void start()
    {
        if (!m_active) {
            m_active = true;
            emit stateChanged(state());
        }
    }

    void stop()
    {
        if (m_active) {
            m_active = false;
            emit stateChanged(state());
        }
    }

public:
    bool m_active;
    bool m_searching;
    bool m_muted;
    bool m_stereo;
    int  m_volume;
    int  m_signal;
    int  m_frequency;
    QRadioTuner::StereoMode m_stereoMode;
    QRadioTuner::Band m_band;
    QRadioTuner::Error m_err;
    QString m_errstr;
};

#endif // MOCKRADIOTUNERCONTROL_H
