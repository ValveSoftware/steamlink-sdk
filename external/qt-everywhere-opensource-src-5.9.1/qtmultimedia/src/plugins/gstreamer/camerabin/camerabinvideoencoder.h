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

#ifndef CAMERABINVIDEOENCODE_H
#define CAMERABINVIDEOENCODE_H

#include <QtMultimedia/private/qtmultimediaglobal_p.h>
#include <qvideoencodersettingscontrol.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qset.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#if QT_CONFIG(gstreamer_encodingprofiles)
#include <gst/pbutils/encoding-profile.h>
#include <private/qgstcodecsinfo_p.h>
#endif

QT_BEGIN_NAMESPACE

class CameraBinSession;

class CameraBinVideoEncoder : public QVideoEncoderSettingsControl
{
    Q_OBJECT
public:
    CameraBinVideoEncoder(CameraBinSession *session);
    virtual ~CameraBinVideoEncoder();

    QList<QSize> supportedResolutions(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                      bool *continuous = 0) const override;

    QList< qreal > supportedFrameRates(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                       bool *continuous = 0) const override;

    QPair<int,int> rateAsRational(qreal) const;

    QStringList supportedVideoCodecs() const override;
    QString videoCodecDescription(const QString &codecName) const override;

    QVideoEncoderSettings videoSettings() const override;
    void setVideoSettings(const QVideoEncoderSettings &settings) override;

    QVideoEncoderSettings actualVideoSettings() const;
    void setActualVideoSettings(const QVideoEncoderSettings&);
    void resetActualSettings();

#if QT_CONFIG(gstreamer_encodingprofiles)
    GstEncodingProfile *createProfile();
#endif

    void applySettings(GstElement *encoder);

Q_SIGNALS:
    void settingsChanged();

private:
    CameraBinSession *m_session;

#if QT_CONFIG(gstreamer_encodingprofiles)
    QGstCodecsInfo m_codecs;
#endif

    QVideoEncoderSettings m_actualVideoSettings;
    QVideoEncoderSettings m_videoSettings;
};

QT_END_NAMESPACE

#endif
