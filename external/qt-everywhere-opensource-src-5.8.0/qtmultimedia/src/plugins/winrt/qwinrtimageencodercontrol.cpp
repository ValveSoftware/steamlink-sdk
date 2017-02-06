/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwinrtimageencodercontrol.h"
QT_BEGIN_NAMESPACE

class QWinRTImageEncoderControlPrivate
{
public:
    QList<QSize> supportedResolutions;
    QImageEncoderSettings imageEncoderSetting;
};

QWinRTImageEncoderControl::QWinRTImageEncoderControl(QObject *parent)
    : QImageEncoderControl(parent), d_ptr(new QWinRTImageEncoderControlPrivate)
{
}

QStringList QWinRTImageEncoderControl::supportedImageCodecs() const
{
    return QStringList() << QStringLiteral("jpeg");
}

QString QWinRTImageEncoderControl::imageCodecDescription(const QString &codecName) const
{
    if (codecName == QStringLiteral("jpeg"))
        return tr("JPEG image");

    return QString();
}

QList<QSize> QWinRTImageEncoderControl::supportedResolutions(const QImageEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(settings);
    Q_D(const QWinRTImageEncoderControl);

    if (continuous)
        *continuous = false;

    return d->supportedResolutions;
}

QImageEncoderSettings QWinRTImageEncoderControl::imageSettings() const
{
    Q_D(const QWinRTImageEncoderControl);
    return d->imageEncoderSetting;
}

void QWinRTImageEncoderControl::setImageSettings(const QImageEncoderSettings &settings)
{
    Q_D(QWinRTImageEncoderControl);
    if (d->imageEncoderSetting == settings)
        return;

    d->imageEncoderSetting = settings;
    applySettings();
}

void QWinRTImageEncoderControl::setSupportedResolutionsList(const QList<QSize> resolution)
{
    Q_D(QWinRTImageEncoderControl);
    d->supportedResolutions = resolution;
    applySettings();
}

void QWinRTImageEncoderControl::applySettings()
{
    Q_D(QWinRTImageEncoderControl);
    if (d->imageEncoderSetting.codec().isEmpty())
        d->imageEncoderSetting.setCodec(QStringLiteral("jpeg"));

    QSize requestResolution = d->imageEncoderSetting.resolution();
    if (d->supportedResolutions.isEmpty() || d->supportedResolutions.contains(requestResolution))
        return;

    // Find closest resolution from the list
    const int pixelCount = requestResolution.width() * requestResolution.height();
    int minimumGap = std::numeric_limits<int>::max();
    for (const QSize &size : qAsConst(d->supportedResolutions)) {
        int gap = qAbs(pixelCount - size.width() * size.height());
        if (gap < minimumGap) {
            minimumGap = gap;
            requestResolution = size;
            if (gap == 0)
                break;
        }
    }
    d->imageEncoderSetting.setResolution(requestResolution);
}

QT_END_NAMESPACE
