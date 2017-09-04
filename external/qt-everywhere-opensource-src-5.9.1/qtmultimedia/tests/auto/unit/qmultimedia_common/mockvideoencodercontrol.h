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

#ifndef MOCKVIDEOENCODERCONTROL_H
#define MOCKVIDEOENCODERCONTROL_H

#include "qvideoencodersettingscontrol.h"

class MockVideoEncoderControl : public QVideoEncoderSettingsControl
{
    Q_OBJECT
public:
    MockVideoEncoderControl(QObject *parent):
        QVideoEncoderSettingsControl(parent)
    {
        m_videoCodecs << "video/3gpp" << "video/H264";
        m_sizes << QSize(320,240) << QSize(640,480);
        m_framerates << 30 << 15 << 1;
    }
    ~MockVideoEncoderControl() {}

    QVideoEncoderSettings videoSettings() const { return m_videoSettings; }
    void setVideoSettings(const QVideoEncoderSettings &settings) { m_videoSettings = settings; };

    QList<QSize> supportedResolutions(const QVideoEncoderSettings & = QVideoEncoderSettings(),
                                      bool *continuous = 0) const
    {
        if (continuous)
            *continuous = true;

        return m_sizes;
    }

    QList<qreal> supportedFrameRates(const QVideoEncoderSettings & = QVideoEncoderSettings(),
                                     bool *continuous = 0) const
    {
        if (continuous)
            *continuous = false;

        return m_framerates;
    }

    QStringList supportedVideoCodecs() const { return m_videoCodecs; }
    QString videoCodecDescription(const QString &codecName) const { return codecName; }

private:
    QVideoEncoderSettings m_videoSettings;

    QStringList m_videoCodecs;
    QList<QSize> m_sizes;
    QList<qreal> m_framerates;
};

#endif // MOCKVIDEOENCODERCONTROL_H
