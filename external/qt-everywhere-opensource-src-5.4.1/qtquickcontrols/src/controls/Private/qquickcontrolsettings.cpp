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

#include "qquickcontrolsettings_p.h"
#include <qquickitem.h>
#include <qcoreapplication.h>
#include <qqmlengine.h>
#include <qlibrary.h>
#include <qdir.h>
#include <QTouchDevice>
#include <QGuiApplication>
#include <QStyleHints>
#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_NO_SDK)
#include <private/qjnihelpers_p.h>
#endif

QT_BEGIN_NAMESPACE

static QString defaultStyleName()
{
    //Only enable QStyle support when we are using QApplication
#if defined(QT_WIDGETS_LIB) && !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID) && !defined(Q_OS_BLACKBERRY) && !defined(Q_OS_QNX) && !defined(Q_OS_WINRT)
    if (QCoreApplication::instance()->inherits("QApplication"))
        return QLatin1String("Desktop");
#elif defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_NO_SDK)
    if (QtAndroidPrivate::androidSdkVersion() >= 11)
        return QLatin1String("Android");
#elif defined(Q_OS_IOS)
    return QLatin1String("iOS");
#endif
    return QLatin1String("Base");
}

static QString styleImportName()
{
    QString name = qgetenv("QT_QUICK_CONTROLS_STYLE");
    if (name.isEmpty())
        name = defaultStyleName();
    return QFileInfo(name).fileName();
}

static bool fromResource(const QString &path)
{
    return path.startsWith("qrc:");
}

bool QQuickControlSettings::hasTouchScreen() const
{
// QTBUG-36007
#if defined(Q_OS_ANDROID)
    return true;
#else
    foreach (const QTouchDevice *dev, QTouchDevice::devices())
        if (dev->type() == QTouchDevice::TouchScreen)
            return true;
    return false;
#endif
}

bool QQuickControlSettings::isMobile() const
{
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_QNX) || defined(Q_OS_WINRT)
    return true;
#else
    return false;
#endif
}

static QString styleImportPath(QQmlEngine *engine, const QString &styleName)
{
    QString path = qgetenv("QT_QUICK_CONTROLS_STYLE");
    QFileInfo info(path);
    if (fromResource(path)) {
        path = info.path();
    } else if (info.isRelative()) {
        bool found = false;
        foreach (const QString &import, engine->importPathList()) {
            QDir dir(import + QLatin1String("/QtQuick/Controls/Styles"));
            if (dir.exists(styleName)) {
                found = true;
                path = dir.absolutePath();
                break;
            }
        }
        if (!found)
            path = "qrc:/QtQuick/Controls/Styles";
    } else {
        path = info.absolutePath();
    }
    return path;
}

QQuickControlSettings::QQuickControlSettings(QQmlEngine *engine)
{
    m_name = styleImportName();
    m_path = styleImportPath(engine, m_name);

    QString path = styleFilePath();
    if (fromResource(path))
        path = path.remove(0, 3); // remove qrc from the path

    QDir dir(path);
    if (!dir.exists()) {
        QString unknownStyle = m_name;
        m_name = defaultStyleName();
        m_path = styleImportPath(engine, m_name);
        qWarning() << "WARNING: Cannot find style" << unknownStyle << "- fallback:" << styleFilePath();
    } else {
        typedef bool (*StyleInitFunc)();
        typedef const char *(*StylePathFunc)();

        foreach (const QString &fileName, dir.entryList()) {
            if (QLibrary::isLibrary(fileName)) {
                QLibrary lib(dir.absoluteFilePath(fileName));
                StyleInitFunc initFunc = (StyleInitFunc) lib.resolve("qt_quick_controls_style_init");
                if (initFunc)
                    initFunc();
                StylePathFunc pathFunc = (StylePathFunc) lib.resolve("qt_quick_controls_style_path");
                if (pathFunc)
                    m_path = QString::fromLocal8Bit(pathFunc());
                if (initFunc || pathFunc)
                    break;
            }
        }
    }

    connect(this, SIGNAL(styleNameChanged()), SIGNAL(styleChanged()));
    connect(this, SIGNAL(stylePathChanged()), SIGNAL(styleChanged()));
}

QString QQuickControlSettings::style() const
{
    if (fromResource(styleFilePath()))
        return styleFilePath();
    else
        return QUrl::fromLocalFile(styleFilePath()).toString();
}

QString QQuickControlSettings::styleName() const
{
    return m_name;
}

void QQuickControlSettings::setStyleName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit styleNameChanged();
    }
}

QString QQuickControlSettings::stylePath() const
{
    return m_path;
}

void QQuickControlSettings::setStylePath(const QString &path)
{
    if (m_path != path) {
        m_path = path;
        emit stylePathChanged();
    }
}

QString QQuickControlSettings::styleFilePath() const
{
    return m_path + QLatin1Char('/') + m_name;
}

extern Q_GUI_EXPORT int qt_defaultDpiX();

qreal QQuickControlSettings::dpiScaleFactor() const
{
#ifndef Q_OS_MAC
    return (qreal(qt_defaultDpiX()) / 96.0);
#endif
    return 1.0;
}

qreal QQuickControlSettings::dragThreshold() const
{
    return qApp->styleHints()->startDragDistance();
}


QT_END_NAMESPACE
