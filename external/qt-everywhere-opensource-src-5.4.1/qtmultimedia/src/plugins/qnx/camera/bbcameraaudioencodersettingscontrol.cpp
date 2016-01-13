/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#include "bbcameraaudioencodersettingscontrol.h"

#include "bbcamerasession.h"

QT_BEGIN_NAMESPACE

BbCameraAudioEncoderSettingsControl::BbCameraAudioEncoderSettingsControl(BbCameraSession *session, QObject *parent)
    : QAudioEncoderSettingsControl(parent)
    , m_session(session)
{
}

QStringList BbCameraAudioEncoderSettingsControl::supportedAudioCodecs() const
{
    return QStringList() << QLatin1String("none") << QLatin1String("aac") << QLatin1String("raw");
}

QString BbCameraAudioEncoderSettingsControl::codecDescription(const QString &codecName) const
{
    if (codecName == QLatin1String("none"))
        return tr("No compression");
    else if (codecName == QLatin1String("aac"))
        return tr("AAC compression");
    else if (codecName == QLatin1String("raw"))
        return tr("PCM uncompressed");

    return QString();
}

QList<int> BbCameraAudioEncoderSettingsControl::supportedSampleRates(const QAudioEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(settings);
    Q_UNUSED(continuous);

    // no API provided by BB10 yet
    return QList<int>();
}

QAudioEncoderSettings BbCameraAudioEncoderSettingsControl::audioSettings() const
{
    return m_session->audioSettings();
}

void BbCameraAudioEncoderSettingsControl::setAudioSettings(const QAudioEncoderSettings &settings)
{
    m_session->setAudioSettings(settings);
}

QT_END_NAMESPACE
