/***************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickstyleselector_p.h"
#include "qquickstyleselector_p_p.h"
#include "qquickstyle.h"
#include "qquickstyle_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qsysinfo.h>
#include <QtCore/qlocale.h>
#include <QtQml/qqmlfile.h>

#include <QtCore/private/qfileselector_p.h>
#include <QtGui/private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

static bool isLocalScheme(const QString &scheme)
{
    bool local = scheme == QLatin1String("qrc");
#ifdef Q_OS_ANDROID
    local |= scheme == QLatin1String("assets");
#endif
    return local;
}

static QString ensureSlash(const QString &path)
{
    if (path.endsWith(QLatin1Char('/')))
        return path;
    return path + QLatin1Char('/');
}

static QStringList allSelectors(const QString &style = QString())
{
    static const QStringList platformSelectors = QFileSelectorPrivate::platformSelectors();
    QStringList selectors = platformSelectors;
    selectors += QLocale().name();
    if (!style.isEmpty())
        selectors.prepend(style);
    return selectors;
}

QString QQuickStyleSelectorPrivate::select(const QString &filePath) const
{
    QFileInfo fi(filePath);
    // If file doesn't exist, don't select
    if (!fi.exists())
        return filePath;

    const QString path = fi.path();
    const QString ret = QFileSelectorPrivate::selectionHelper(path.isEmpty() ? QString() : path + QLatin1Char('/'),
                                                              fi.fileName(), allSelectors(styleName), QChar());

    if (!ret.isEmpty())
        return ret;
    return filePath;
}

QString QQuickStyleSelectorPrivate::trySelect(const QString &filePath, const QString &fallback) const
{
    QFileInfo fi(filePath);
    if (!fi.exists())
        return fallback;

    // the path contains the name of the custom/fallback style, so exclude it from
    // the selectors. the rest of the selectors (os, locale) are still valid, though.
    const QString path = fi.path();
    const QString selectedPath = QFileSelectorPrivate::selectionHelper(path.isEmpty() ? QString() : path + QLatin1Char('/'),
                                                                       fi.fileName(), allSelectors(), QChar());
    if (selectedPath.startsWith(QLatin1Char(':')))
        return QLatin1String("qrc") + selectedPath;
    return QUrl::fromLocalFile(QFileInfo(selectedPath).absoluteFilePath()).toString();
}

QQuickStyleSelector::QQuickStyleSelector() : d_ptr(new QQuickStyleSelectorPrivate)
{
    Q_D(QQuickStyleSelector);
    d->styleName = QQuickStyle::name();
    d->stylePath = QQuickStyle::path();
}

QQuickStyleSelector::~QQuickStyleSelector()
{
}

QUrl QQuickStyleSelector::baseUrl() const
{
    Q_D(const QQuickStyleSelector);
    return d->baseUrl;
}

void QQuickStyleSelector::setBaseUrl(const QUrl &url)
{
    Q_D(QQuickStyleSelector);
    d->baseUrl = url;
    d->basePath = QQmlFile::urlToLocalFileOrQrc(url.toString(QUrl::StripTrailingSlash) + QLatin1Char('/'));
}

QString QQuickStyleSelector::select(const QString &fileName) const
{
    Q_D(const QQuickStyleSelector);

    // 1) try selecting from a custom style path, for example ":/mystyle"
    if (QQuickStylePrivate::isCustomStyle()) {
        // NOTE: this path may contain a subset of controls
        const QString selectedPath = d->trySelect(ensureSlash(d->stylePath) + d->styleName + QLatin1Char('/') + fileName);
        if (!selectedPath.isEmpty())
            return selectedPath;
    }

    // 2) try selecting from the fallback style path, for example QT_INSTALL_QML/QtQuick/Controls.2/Material
    const QString fallbackStyle = QQuickStylePrivate::fallbackStyle();
    if (!fallbackStyle.isEmpty()) {
        // NOTE: this path may also contain a subset of controls
        const QString selectedPath = d->trySelect(ensureSlash(d->basePath) + fallbackStyle + QLatin1Char('/') + fileName);
        if (!selectedPath.isEmpty())
            return selectedPath;
    }

    // 3) fallback to the default style that is guaranteed to contain all controls
    QUrl url(ensureSlash(d->baseUrl.toString()) + fileName);
    if (isLocalScheme(url.scheme())) {
        QString equivalentPath = QLatin1Char(':') + url.path();
        QString selectedPath = d->select(equivalentPath);
        url.setPath(selectedPath.remove(0, 1));
    } else if (url.isLocalFile()) {
        url = QUrl::fromLocalFile(d->select(url.toLocalFile()));
    }
    return url.toString();
}

QT_END_NAMESPACE
