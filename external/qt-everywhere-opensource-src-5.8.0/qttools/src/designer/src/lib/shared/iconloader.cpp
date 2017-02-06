/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "iconloader_p.h"

#include <QtCore/QFile>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

QDESIGNER_SHARED_EXPORT QIcon createIconSet(const QString &name)
{
    QStringList candidates = QStringList()
        << (QString::fromUtf8(":/qt-project.org/formeditor/images/") + name)
#ifdef Q_OS_MACOS
        << (QString::fromUtf8(":/qt-project.org/formeditor/images/mac/") + name)
#else
        << (QString::fromUtf8(":/qt-project.org/formeditor/images/win/") + name)
#endif
        << (QString::fromUtf8(":/qt-project.org/formeditor/images/designer_") + name);

    foreach (const QString &f, candidates) {
        if (QFile::exists(f))
            return QIcon(f);
    }

    return QIcon();
}

QDESIGNER_SHARED_EXPORT QIcon emptyIcon()
{
    return QIcon(QStringLiteral(":/qt-project.org/formeditor/images/emptyicon.png"));
}

static QIcon buildIcon(const QString &prefix, const int *sizes, size_t sizeCount)
{
    QIcon result;
    for (size_t i = 0; i < sizeCount; ++i) {
        const QString size = QString::number(sizes[i]);
        const QPixmap pixmap(prefix + size + QLatin1Char('x') + size + QStringLiteral(".png"));
        Q_ASSERT(!pixmap.size().isEmpty());
        result.addPixmap(pixmap);
    }
    return result;
}

QDESIGNER_SHARED_EXPORT QIcon qtLogoIcon()
{
    static const int sizes[] = {16, 24, 32, 64};
    static const QIcon result =
        buildIcon(QStringLiteral(":/qt-project.org/formeditor/images/qtlogo"),
                  sizes, sizeof(sizes) / sizeof(sizes[0]));
    return result;
}

} // namespace qdesigner_internal

QT_END_NAMESPACE

