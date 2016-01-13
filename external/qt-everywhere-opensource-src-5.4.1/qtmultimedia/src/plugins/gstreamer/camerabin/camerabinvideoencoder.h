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

#ifndef CAMERABINVIDEOENCODE_H
#define CAMERABINVIDEOENCODE_H

#include <qvideoencodersettingscontrol.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qset.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/encoding-profile.h>
#include <private/qgstcodecsinfo_p.h>

QT_BEGIN_NAMESPACE

class CameraBinSession;

class CameraBinVideoEncoder : public QVideoEncoderSettingsControl
{
    Q_OBJECT
public:
    CameraBinVideoEncoder(CameraBinSession *session);
    virtual ~CameraBinVideoEncoder();

    QList<QSize> supportedResolutions(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                      bool *continuous = 0) const;

    QList< qreal > supportedFrameRates(const QVideoEncoderSettings &settings = QVideoEncoderSettings(),
                                       bool *continuous = 0) const;

    QPair<int,int> rateAsRational(qreal) const;

    QStringList supportedVideoCodecs() const;
    QString videoCodecDescription(const QString &codecName) const;

    QVideoEncoderSettings videoSettings() const;
    void setVideoSettings(const QVideoEncoderSettings &settings);

    QVideoEncoderSettings actualVideoSettings() const;
    void setActualVideoSettings(const QVideoEncoderSettings&);
    void resetActualSettings();

    GstEncodingProfile *createProfile();

    void applySettings(GstElement *encoder);

Q_SIGNALS:
    void settingsChanged();

private:
    CameraBinSession *m_session;

    QGstCodecsInfo m_codecs;

    QVideoEncoderSettings m_actualVideoSettings;
    QVideoEncoderSettings m_videoSettings;
};

QT_END_NAMESPACE

#endif
