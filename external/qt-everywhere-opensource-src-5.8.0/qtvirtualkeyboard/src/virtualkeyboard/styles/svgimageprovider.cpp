/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "svgimageprovider.h"
#include <QImage>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>

SvgImageProvider::SvgImageProvider() :
    QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

SvgImageProvider::~SvgImageProvider()
{
}

QPixmap SvgImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    QSize imageSize(-1, -1);
    QUrl request(id);
    QString imagePath = ":/" + request.path();
    if (request.hasQuery()) {
        const QString query = request.query();
        const QStringList paramList = query.split(QChar('&'), QString::SkipEmptyParts);
        QVariantMap params;
        for (const QString &param : paramList) {
            QStringList keyValue = param.split(QChar('='), QString::SkipEmptyParts);
            if (keyValue.length() == 2)
                params[keyValue[0]] = keyValue[1];
        }
        const auto widthIt = params.constFind("width");
        if (widthIt != params.cend()) {
            bool ok = false;
            int value = widthIt.value().toInt(&ok);
            if (ok)
                imageSize.setWidth(value);
        }
        const auto heightIt = params.constFind("height");
        if (heightIt != params.cend()) {
            bool ok = false;
            int value = heightIt.value().toInt(&ok);
            if (ok)
                imageSize.setHeight(value);
        }
    } else {
        imageSize = requestedSize;
    }

    QPixmap image;
    if ((imageSize.width() > 0 || imageSize.height() > 0) && imagePath.endsWith(".svg")) {
        QSvgRenderer renderer(imagePath);
        QSize defaultSize(renderer.defaultSize());
        if (defaultSize.isEmpty())
            return image;
        if (imageSize.width() <= 0 && imageSize.height() > 0) {
            double aspectRatio = (double)defaultSize.width() / (double)defaultSize.height();
            imageSize.setWidth(qRound(imageSize.height() * aspectRatio));
        } else if (imageSize.width() > 0 && imageSize.height() <= 0) {
            double aspectRatio = (double)defaultSize.width() / (double)defaultSize.height();
            imageSize.setHeight(qRound(imageSize.width() / aspectRatio));
        }
        image = QPixmap(imageSize);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter, image.rect());
    } else {
        image = QPixmap(imagePath);
        imageSize = image.size();
    }

    QPixmap result;
    if (requestedSize.isValid() && requestedSize != imageSize)
        result = image.scaled(requestedSize, Qt::KeepAspectRatio);
    else
        result = image;

    *size = result.size();

    return result;
}
