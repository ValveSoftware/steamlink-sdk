/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativecamerapreviewprovider_p.h"
#include <QtCore/qmutex.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

struct QDeclarativeCameraPreviewProviderPrivate
{
    QString id;
    QImage image;
    QMutex mutex;
};

Q_GLOBAL_STATIC(QDeclarativeCameraPreviewProviderPrivate, qDeclarativeCameraPreviewProviderPrivate)

QDeclarativeCameraPreviewProvider::QDeclarativeCameraPreviewProvider()
: QQuickImageProvider(QQuickImageProvider::Image)
{
}

QDeclarativeCameraPreviewProvider::~QDeclarativeCameraPreviewProvider()
{
    QDeclarativeCameraPreviewProviderPrivate *d = qDeclarativeCameraPreviewProviderPrivate();
    QMutexLocker lock(&d->mutex);
    d->id.clear();
    d->image = QImage();
}

QImage QDeclarativeCameraPreviewProvider::requestImage(const QString &id, QSize *size, const QSize& requestedSize)
{
    QDeclarativeCameraPreviewProviderPrivate *d = qDeclarativeCameraPreviewProviderPrivate();
    QMutexLocker lock(&d->mutex);

    if (d->id != id)
        return QImage();

    QImage res = d->image;
    if (!requestedSize.isEmpty())
        res = res.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (size)
        *size = res.size();

    return res;
}

void QDeclarativeCameraPreviewProvider::registerPreview(const QString &id, const QImage &preview)
{
    //only the last preview is kept
    QDeclarativeCameraPreviewProviderPrivate *d = qDeclarativeCameraPreviewProviderPrivate();
    QMutexLocker lock(&d->mutex);
    d->id = id;
    d->image = preview;
}

QT_END_NAMESPACE
