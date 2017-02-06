/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickstyleattached_p.h"
#include "qquickstyle_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qsettings.h>
#include <QtCore/qfileselector.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpalette.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuickTemplates2/private/qquickpopup_p.h>

QT_BEGIN_NAMESPACE

static bool isDarkSystemTheme()
{
    if (const QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme()) {
        if (const QPalette *systemPalette = theme->palette(QPlatformTheme::SystemPalette)) {
            const QColor textColor = systemPalette->color(QPalette::WindowText);
            return textColor.red() > 128 && textColor.blue() > 128 && textColor.green() > 128;
        }
    }
    return false;
}

Q_QUICKCONTROLS2_PRIVATE_EXPORT bool qt_is_dark_system_theme()
{
    static bool dark = isDarkSystemTheme();
    return dark;
}

static QQuickStyleAttached *attachedStyle(const QMetaObject *type, QObject *object, bool create = false)
{
    if (!object)
        return nullptr;
    int idx = -1;
    return qobject_cast<QQuickStyleAttached *>(qmlAttachedPropertiesObject(&idx, object, type, create));
}

static QQuickStyleAttached *findParentStyle(const QMetaObject *type, QObject *object)
{
    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (item) {
        // lookup parent items and popups
        QQuickItem *parent = item->parentItem();
        while (parent) {
            QQuickStyleAttached *style = attachedStyle(type, parent);
            if (style)
                return style;

            QQuickPopup *popup = qobject_cast<QQuickPopup *>(parent->parent());
            if (popup)
                return attachedStyle(type, popup);

            parent = parent->parentItem();
        }

        // fallback to item's window
        QQuickStyleAttached *style = attachedStyle(type, item->window());
        if (style)
            return style;
    } else {
        // lookup popup's window
        QQuickPopup *popup = qobject_cast<QQuickPopup *>(object);
        if (popup)
            return attachedStyle(type, popup->popupItem()->window());
    }

    // lookup parent window
    QQuickWindow *window = qobject_cast<QQuickWindow *>(object);
    if (window) {
        QQuickWindow *parentWindow = qobject_cast<QQuickWindow *>(window->parent());
        if (parentWindow) {
            QQuickStyleAttached *style = attachedStyle(type, window);
            if (style)
                return style;
        }
    }

    // fallback to engine (global)
    if (object) {
        QQmlEngine *engine = qmlEngine(object);
        if (engine) {
            QByteArray name = QByteArray("_q_") + type->className();
            QQuickStyleAttached *style = engine->property(name).value<QQuickStyleAttached*>();
            if (!style) {
                style = attachedStyle(type, engine, true);
                engine->setProperty(name, QVariant::fromValue(style));
            }
            return style;
        }
    }

    return nullptr;
}

static QList<QQuickStyleAttached *> findChildStyles(const QMetaObject *type, QObject *object)
{
    QList<QQuickStyleAttached *> children;

    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        QQuickWindow *window = qobject_cast<QQuickWindow *>(object);
        if (window) {
            item = window->contentItem();

            const auto windowChildren = window->children();
            for (QObject *child : windowChildren) {
                QQuickWindow *childWindow = qobject_cast<QQuickWindow *>(child);
                if (childWindow) {
                    QQuickStyleAttached *style = attachedStyle(type, childWindow);
                    if (style)
                        children += style;
                }
            }
        }
    }

    if (item) {
        const auto childItems = item->childItems();
        for (QQuickItem *child : childItems) {
            QQuickStyleAttached *style = attachedStyle(type, child);
            if (style)
                children += style;
            else
                children += findChildStyles(type, child);
        }
    }

    return children;
}

QQuickStyleAttached::QQuickStyleAttached(QObject *parent) : QObject(parent)
{
    QQuickItem *item = qobject_cast<QQuickItem *>(parent);
    if (!item) {
        QQuickPopup *popup = qobject_cast<QQuickPopup *>(parent);
        if (popup)
            item = popup->popupItem();
    }

    if (item) {
        connect(item, &QQuickItem::windowChanged, this, &QQuickStyleAttached::itemWindowChanged);
        QQuickItemPrivate::get(item)->addItemChangeListener(this, QQuickItemPrivate::Parent);
    }
}

QQuickStyleAttached::~QQuickStyleAttached()
{
    QQuickItem *item = qobject_cast<QQuickItem *>(parent());
    if (item) {
        disconnect(item, &QQuickItem::windowChanged, this, &QQuickStyleAttached::itemWindowChanged);
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Parent);
    }

    setParentStyle(nullptr);
}

QSharedPointer<QSettings> QQuickStyleAttached::settings(const QString &group)
{
#ifndef QT_NO_SETTINGS
    const QString filePath = QQuickStylePrivate::configFilePath();
    if (QFile::exists(filePath)) {
        QFileSelector selector;
        QSettings *settings = new QSettings(selector.select(filePath), QSettings::IniFormat);
        if (!group.isEmpty())
            settings->beginGroup(group);
        return QSharedPointer<QSettings>(settings);
    }
#endif // QT_NO_SETTINGS
    return QSharedPointer<QSettings>();
}

QList<QQuickStyleAttached *> QQuickStyleAttached::childStyles() const
{
    return m_childStyles;
}

QQuickStyleAttached *QQuickStyleAttached::parentStyle() const
{
    return m_parentStyle;
}

void QQuickStyleAttached::setParentStyle(QQuickStyleAttached *style)
{
    if (m_parentStyle != style) {
        QQuickStyleAttached *oldParent = m_parentStyle;
        if (m_parentStyle)
            m_parentStyle->m_childStyles.removeOne(this);
        m_parentStyle = style;
        if (style)
            style->m_childStyles.append(this);
        parentStyleChange(style, oldParent);
    }
}

void QQuickStyleAttached::init()
{
    QQuickStyleAttached *parentStyle = findParentStyle(metaObject(), parent());
    if (parentStyle)
        setParentStyle(parentStyle);

    const QList<QQuickStyleAttached *> children = findChildStyles(metaObject(), parent());
    for (QQuickStyleAttached *child : children)
        child->setParentStyle(this);
}

void QQuickStyleAttached::parentStyleChange(QQuickStyleAttached *newParent, QQuickStyleAttached *oldParent)
{
    Q_UNUSED(newParent);
    Q_UNUSED(oldParent);
}

void QQuickStyleAttached::itemWindowChanged(QQuickWindow *window)
{
    QQuickStyleAttached *parentStyle = nullptr;
    QQuickItem *item = qobject_cast<QQuickItem *>(sender());
    if (item)
        parentStyle = findParentStyle(metaObject(), item);
    if (!parentStyle)
        parentStyle = attachedStyle(metaObject(), window);
    setParentStyle(parentStyle);
}

void QQuickStyleAttached::itemParentChanged(QQuickItem *item, QQuickItem *parent)
{
    Q_UNUSED(parent);
    setParentStyle(findParentStyle(metaObject(), item));
}

QT_END_NAMESPACE
