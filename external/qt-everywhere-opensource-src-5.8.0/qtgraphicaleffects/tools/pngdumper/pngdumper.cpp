/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

#include "pngdumper.h"

#include <QtQml/qqml.h>
#include <QtQuick/QQuickWindow>

ItemCapturer::ItemCapturer(QQuickItem *parent):
    QQuickItem(parent)
{
}

ItemCapturer::~ItemCapturer()
{
}

void ItemCapturer::grabItem(QQuickItem *item, QString filename)
{
    QQuickWindow *w = window();
    QImage img = w->grabWindow();
    while (img.width() * img.height() == 0)
        img = w->grabWindow();
    QQuickItem *rootItem = w->contentItem();
    QRectF rectf = rootItem->mapRectFromItem(item, QRectF(0, 0, item->width(), item->height()));
    QDir pwd = QDir().dirName();
    pwd.mkdir("output");
    img = img.copy(rectf.toRect());
    img.save("output/" + filename);
    emit imageSaved();
}

void ItemCapturer::document(QString s)
{
    printf(s.toLatin1().data());
}
