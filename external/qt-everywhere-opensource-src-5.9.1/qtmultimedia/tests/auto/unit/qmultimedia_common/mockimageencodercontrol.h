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

#ifndef MOCKIMAGEENCODERCONTROL_H
#define MOCKIMAGEENCODERCONTROL_H

#include "qimageencodercontrol.h"

class MockImageEncoderControl : public QImageEncoderControl
{
public:
    MockImageEncoderControl(QObject *parent = 0)
        : QImageEncoderControl(parent)
    {
        m_settings = QImageEncoderSettings();
    }

    QList<QSize> supportedResolutions(const QImageEncoderSettings & settings = QImageEncoderSettings(),
                                      bool *continuous = 0) const
    {
        if (continuous)
            *continuous = true;

        QList<QSize> resolutions;
        if (settings.resolution().isValid()) {
            if (settings.resolution() == QSize(160,160) ||
                settings.resolution() == QSize(320,240))
                resolutions << settings.resolution();

            if (settings.quality() == QMultimedia::HighQuality && settings.resolution() == QSize(640,480))
                resolutions << settings.resolution();
        } else {
            resolutions << QSize(160, 120);
            resolutions << QSize(320, 240);
            if (settings.quality() == QMultimedia::HighQuality)
                resolutions << QSize(640, 480);
        }

        return resolutions;
    }

    QStringList supportedImageCodecs() const
    {
        QStringList codecs;
        codecs << "PNG" << "JPEG";
        return codecs;
    }

    QString imageCodecDescription(const QString &codecName) const {
        if (codecName == "PNG")
            return QString("Portable Network Graphic");
        if (codecName == "JPEG")
            return QString("Joint Photographic Expert Group");
        return QString();
    }

    QImageEncoderSettings imageSettings() const { return m_settings; }
    void setImageSettings(const QImageEncoderSettings &settings) { m_settings = settings; }

private:
    QImageEncoderSettings m_settings;
};


#endif // MOCKIMAGEENCODERCONTROL_H
