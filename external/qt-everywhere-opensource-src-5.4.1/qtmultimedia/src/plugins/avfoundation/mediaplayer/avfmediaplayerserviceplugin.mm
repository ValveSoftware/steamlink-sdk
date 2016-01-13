/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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
