/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef QDECLARATIVERADIO_P_H
#define QDECLARATIVERADIO_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qradiotuner.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QDeclarativeRadioData;

class QDeclarativeRadio : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(Band band READ band WRITE setBand NOTIFY bandChanged)
    Q_PROPERTY(int frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(bool stereo READ stereo NOTIFY stereoStatusChanged)
    Q_PROPERTY(StereoMode stereoMode READ stereoMode WRITE setStereoMode)
    Q_PROPERTY(int signalStrength READ signalStrength NOTIFY signalStrengthChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(int frequencyStep READ frequencyStep NOTIFY bandChanged)
    Q_PROPERTY(int minimumFrequency READ minimumFrequency NOTIFY bandChanged)
    Q_PROPERTY(int maximumFrequency READ maximumFrequency NOTIFY bandChanged)
    Q_PROPERTY(bool antennaConnected READ isAntennaConnected NOTIFY antennaConnectedChanged)
    Q_PROPERTY(Availability availability READ availability NOTIFY availabilityChanged)
    Q_PROPERTY(QDeclarativeRadioData* radioData READ radioData CONSTANT)
    Q_ENUMS(State)
    Q_ENUMS(Band)
    Q_ENUMS(Error)
    Q_ENUMS(StereoMode)
    Q_ENUMS(SearchMode)
    Q_ENUMS(Availability)

public:
    enum State {
        ActiveState = QRadioTuner::ActiveState,
        StoppedState = QRadioTuner::StoppedState
    };

    enum Band {
        AM = QRadioTuner::AM,
        FM = QRadioTuner::FM,
        SW = QRadioTuner::SW,
        LW = QRadioTuner::LW,
        FM2 = QRadioTuner::FM2
    };

    enum Error {
        NoError = QRadioTuner::NoError,
        ResourceError = QRadioTuner::ResourceError,
        OpenError = QRadioTuner::OpenError,
        OutOfRangeError = QRadioTuner::OutOfRangeError
    };

    enum StereoMode {
        ForceStereo = QRadioTuner::ForceStereo,
        ForceMono = QRadioTuner::ForceMono,
        Auto = QRadioTuner::Auto
    };

    enum SearchMode {
        SearchFast = QRadioTuner::SearchFast,
        SearchGetStationId = QRadioTuner::SearchGetStationId
    };

    enum Availability {
        Available = QMultimedia::Available,
        Busy = QMultimedia::Busy,
        Unavailable = QMultimedia::ServiceMissing,
        ResourceMissing = QMultimedia::ResourceError
    };

    QDeclarativeRadio(QObject *parent = 0);
    ~QDeclarativeRadio();

    QDeclarativeRadio::State state() const;
    QDeclarativeRadio::Band band() const;
    int frequency() const;
    QDeclarativeRadio::StereoMode stereoMode() const;
    int volume() const;
    bool muted() const;

    bool stereo() const;
    int signalStrength() const;
    bool searching() const;

    int frequencyStep() const;
    int minimumFrequency() const;
    int maximumFrequency() const;

    bool isAntennaConnected() const;

    Q_INVOKABLE bool isAvailable() const {return availability() == Available;}
    Availability availability() const;

    QDeclarativeRadioData *radioData() { return m_radioData; }

public Q_SLOTS:
    void setBand(QDeclarativeRadio::Band band);
    void setFrequency(int frequency);
    void setStereoMode(QDeclarativeRadio::StereoMode stereoMode);
    void setVolume(int volume);
    void setMuted(bool muted);

    void cancelScan();
    void scanDown();
    void scanUp();
    void tuneUp();
    void tuneDown();
    void searchAllStations(QDeclarativeRadio::SearchMode searchMode = QDeclarativeRadio::SearchFast );

    void start();
    void stop();

Q_SIGNALS:
    void stateChanged(QDeclarativeRadio::State state);
    void bandChanged(QDeclarativeRadio::Band band);
    void frequencyChanged(int frequency);
    void stereoStatusChanged(bool stereo);
    void searchingChanged(bool searching);
    void signalStrengthChanged(int signalStrength);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void stationFound(int frequency, QString stationId);
    void antennaConnectedChanged(bool connectionStatus);

    void availabilityChanged(Availability availability);

    void errorChanged();
    void error(QDeclarativeRadio::Error errorCode);

private Q_SLOTS:
    void _q_stateChanged(QRadioTuner::State state);
    void _q_bandChanged(QRadioTuner::Band band);
    void _q_error(QRadioTuner::Error errorCode);
    void _q_availabilityChanged(QMultimedia::AvailabilityStatus);

private:
    Q_DISABLE_COPY(QDeclarativeRadio)

    QRadioTuner *m_radioTuner;
    QDeclarativeRadioData *m_radioData;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeRadio))

#endif // QDECLARATIVERADIO_P_H
