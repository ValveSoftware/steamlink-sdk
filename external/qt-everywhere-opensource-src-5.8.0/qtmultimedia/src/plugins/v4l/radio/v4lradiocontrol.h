/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef V4LRADIOCONTROL_H
#define V4LRADIOCONTROL_H

#include <QtCore/qobject.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdatetime.h>

#include <qradiotunercontrol.h>

#if defined(Q_OS_FREEBSD)
#include <sys/types.h>
#else
#include <linux/types.h>
#endif
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

QT_USE_NAMESPACE

class V4LRadioService;

class V4LRadioControl : public QRadioTunerControl
{
    Q_OBJECT
public:
    V4LRadioControl(QObject *parent = 0);
    ~V4LRadioControl();

    bool isAvailable() const;
    QMultimedia::AvailabilityStatus availability() const;

    QRadioTuner::State state() const;

    QRadioTuner::Band band() const;
    void setBand(QRadioTuner::Band b);
    bool isBandSupported(QRadioTuner::Band b) const;

    int frequency() const;
    int frequencyStep(QRadioTuner::Band b) const;
    QPair<int,int> frequencyRange(QRadioTuner::Band b) const;
    void setFrequency(int frequency);

    bool isStereo() const;
    QRadioTuner::StereoMode stereoMode() const;
    void setStereoMode(QRadioTuner::StereoMode mode);

    int signalStrength() const;

    int volume() const;
    void setVolume(int volume);

    bool isMuted() const;
    void setMuted(bool muted);

    bool isSearching() const;
    void cancelSearch();

    void searchForward();
    void searchBackward();

    void start();
    void stop();

    QRadioTuner::Error error() const;
    QString errorString() const;

private slots:
    void search();

private:
    bool initRadio();
    void setVol(int v);
    int  getVol();

    int fd;

    bool m_error;
    bool muted;
    bool stereo;
    bool low;
    bool available;
    int  tuners;
    int  step;
    int  vol;
    int  sig;
    bool scanning;
    bool forward;
    QTimer* timer;
    QRadioTuner::Band   currentBand;
    qint64 freqMin;
    qint64 freqMax;
    qint64 currentFreq;
    QTime  playTime;
};

#endif
