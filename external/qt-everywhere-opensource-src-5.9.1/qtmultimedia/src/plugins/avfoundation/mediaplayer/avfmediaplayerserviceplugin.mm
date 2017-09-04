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

#include "avfmediaplayerserviceplugin.h"
#include <QtCore/QDebug>

#include "avfmediaplayerservice.h"

#import <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

AVFMediaPlayerServicePlugin::AVFMediaPlayerServicePlugin()
{
    buildSupportedTypes();
}

QMediaService *AVFMediaPlayerServicePlugin::create(const QString &key)
{
#ifdef QT_DEBUG_AVF
    qDebug() << "AVFMediaPlayerServicePlugin::create" << key;
#endif
    if (key == QLatin1String(Q_MEDIASERVICE_MEDIAPLAYER))
        return new AVFMediaPlayerService;

    qWarning() << "unsupported key: " << key;
    return 0;
}

void AVFMediaPlayerServicePlugin::release(QMediaService *service)
{
    delete service;
}

QMediaServiceProviderHint::Features AVFMediaPlayerServicePlugin::supportedFeatures(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_MEDIAPLAYER)
        return QMediaServiceProviderHint::VideoSurface;
    else
        return QMediaServiceProviderHint::Features();
}

QMultimedia::SupportEstimate AVFMediaPlayerServicePlugin::hasSupport(const QString &mimeType, const QStringList &codecs) const
{
    Q_UNUSED(codecs);

    if (m_supportedMimeTypes.contains(mimeType))
        return QMultimedia::ProbablySupported;

    return QMultimedia::MaybeSupported;
}

QStringList AVFMediaPlayerServicePlugin::supportedMimeTypes() const
{
    return m_supportedMimeTypes;
}

void AVFMediaPlayerServicePlugin::buildSupportedTypes()
{
    //Populate m_supportedMimeTypes with mimetypes AVAsset supports
    NSArray *mimeTypes = [AVURLAsset audiovisualMIMETypes];
    for (NSString *mimeType in mimeTypes)
    {
        m_supportedMimeTypes.append(QString::fromUtf8([mimeType UTF8String]));
    }
#ifdef QT_DEBUG_AVF
    qDebug() << "AVFMediaPlayerServicePlugin::buildSupportedTypes";
    qDebug() << "Supported Types: " << m_supportedMimeTypes;
#endif

}
