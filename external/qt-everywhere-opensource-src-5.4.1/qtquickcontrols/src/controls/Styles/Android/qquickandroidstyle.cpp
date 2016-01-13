/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickandroidstyle_p.h"
#include <qfileinfo.h>
#include <qfile.h>

QT_BEGIN_NAMESPACE

static QByteArray readFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QFile::ReadOnly | QFile::Text))
        return file.readAll();
    qWarning("QQuickAndroidStyle: failed to read %s", qPrintable(filePath));
    return QByteArray();
}

static QString resolvePath()
{
    QString stylePath(QFile::decodeName(qgetenv("MINISTRO_ANDROID_STYLE_PATH")));
    const QLatin1Char slashChar('/');
    if (!stylePath.isEmpty() && !stylePath.endsWith(slashChar))
        stylePath += slashChar;

    QString androidTheme = QLatin1String(qgetenv("QT_ANDROID_THEME"));
    if (!androidTheme.isEmpty() && !androidTheme.endsWith(slashChar))
        androidTheme += slashChar;

    if (stylePath.isEmpty())
        stylePath = QLatin1String("/data/data/org.kde.necessitas.ministro/files/dl/style/")
                  + QLatin1String(qgetenv("QT_ANDROID_THEME_DISPLAY_DPI")) + slashChar;
    Q_ASSERT(!stylePath.isEmpty());

    if (!androidTheme.isEmpty() && QFileInfo(stylePath + androidTheme + QLatin1String("style.json")).exists())
        stylePath += androidTheme;
    return stylePath;
}

QQuickAndroidStyle::QQuickAndroidStyle(QObject *parent) : QObject(parent)
{
    m_path = resolvePath();
}

QByteArray QQuickAndroidStyle::data() const
{
    if (m_data.isNull() && !m_path.isNull())
        m_data = readFile(m_path + QLatin1String("style.json"));
    return m_data;
}

QString QQuickAndroidStyle::filePath(const QString &fileName) const
{
    if (!fileName.isEmpty())
        return m_path + QFileInfo(fileName).fileName();
    return QString();
}

QColor QQuickAndroidStyle::colorValue(uint value) const
{
    return QColor::fromRgba(value);
}

QT_END_NAMESPACE
