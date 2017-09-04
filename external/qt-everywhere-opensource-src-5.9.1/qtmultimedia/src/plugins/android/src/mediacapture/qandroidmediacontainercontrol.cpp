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

#include "qandroidmediacontainercontrol.h"

#include "qandroidcapturesession.h"

QT_BEGIN_NAMESPACE

QAndroidMediaContainerControl::QAndroidMediaContainerControl(QAndroidCaptureSession *session)
    : QMediaContainerControl()
    , m_session(session)
{
}

QStringList QAndroidMediaContainerControl::supportedContainers() const
{
    return QStringList() << QLatin1String("mp4")
                         << QLatin1String("3gp")
                         << QLatin1String("amr")
                         << QLatin1String("awb");
}

QString QAndroidMediaContainerControl::containerFormat() const
{
    return m_session->containerFormat();
}

void QAndroidMediaContainerControl::setContainerFormat(const QString &format)
{
    m_session->setContainerFormat(format);
}

QString QAndroidMediaContainerControl::containerDescription(const QString &formatMimeType) const
{
    if (formatMimeType == QLatin1String("mp4"))
        return tr("MPEG4 media file format");
    else if (formatMimeType == QLatin1String("3gp"))
        return tr("3GPP media file format");
    else if (formatMimeType == QLatin1String("amr"))
        return tr("AMR NB file format");
    else if (formatMimeType == QLatin1String("awb"))
        return tr("AMR WB file format");

    return QString();
}

QT_END_NAMESPACE
