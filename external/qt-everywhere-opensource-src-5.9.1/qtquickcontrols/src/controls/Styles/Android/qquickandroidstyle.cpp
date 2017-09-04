/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

QQuickAndroidStyle1::QQuickAndroidStyle1(QObject *parent) : QObject(parent)
{
    m_path = resolvePath();
}

QByteArray QQuickAndroidStyle1::data() const
{
    if (m_data.isNull() && !m_path.isNull())
        m_data = readFile(m_path + QLatin1String("style.json"));
    return m_data;
}

QString QQuickAndroidStyle1::filePath(const QString &fileName) const
{
    if (!fileName.isEmpty())
        return m_path + QFileInfo(fileName).fileName();
    return QString();
}

QColor QQuickAndroidStyle1::colorValue(uint value) const
{
    return QColor::fromRgba(value);
}

QT_END_NAMESPACE
