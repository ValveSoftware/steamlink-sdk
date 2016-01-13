/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquicktaskbarbutton_p.h"
#include "qquickiconloader_p.h"

#include <QQuickWindow>
#include <QVariant>

QT_BEGIN_NAMESPACE

/*!
    \qmltype TaskbarButton
    \instantiates QQuickTaskbarButton
    \inqmlmodule QtWinExtras

    \brief Manages Windows taskbar buttons.

    \since QtWinExtras 1.0

    The TaskbarButton type enables you to set an overlay icon and to display
    a progress indicator on a taskbar button. An overlay icon indicates change
    in the state of the application. A progress indicator shows how time-consuming
    tasks are progressing.

    \section3 Example

    The following example illustrates how to use TaskbarButton in QML:

    \snippet code/taskbar.qml taskbar_qml

    \image taskbar-button.png Taskbar Button

    \sa QWinTaskbarButton, QWinTaskbarProgress
 */

QQuickTaskbarOverlay::QQuickTaskbarOverlay(QWinTaskbarButton *button, QObject *parent) :
    QObject(parent), m_button(button)
{
}

QUrl QQuickTaskbarOverlay::iconSource() const
{
    return m_iconSource;
}

void QQuickTaskbarOverlay::setIconSource(const QUrl &iconSource)
{
    if (m_iconSource == iconSource)
        return;

    if (iconSource.isEmpty()) {
        m_button->clearOverlayIcon();
        m_iconSource = iconSource;
        emit iconSourceChanged();
        return;
    }

    if (QQuickIconLoader::load(iconSource, qmlEngine(parent()), QVariant::Icon, QSize(),
                               this, &QQuickTaskbarOverlay::iconLoaded) != QQuickIconLoader::LoadError) {
        m_iconSource = iconSource;
        emit iconSourceChanged();
    }
}

QString QQuickTaskbarOverlay::accessibleDescription() const
{
    return m_button->overlayAccessibleDescription();
}

void QQuickTaskbarOverlay::setAccessibleDescription(const QString &description)
{
    if (accessibleDescription() != description) {
        m_button->setOverlayAccessibleDescription(description);
        emit accessibleDescriptionChanged();
    }
}

void QQuickTaskbarOverlay::iconLoaded(const QVariant &value)
{
    m_button->setOverlayIcon(value.value<QIcon>());
}

QQuickTaskbarButton::QQuickTaskbarButton(QQuickItem *parent) : QQuickItem(parent),
    m_button(new QWinTaskbarButton(this)), m_overlay(new QQuickTaskbarOverlay(m_button, this))
{
}

QQuickTaskbarButton::~QQuickTaskbarButton()
{
}

/*!
    \qmlpropertygroup ::TaskbarButton::progress
    \qmlproperty int TaskbarButton::progress.value
    \qmlproperty int TaskbarButton::progress.minimum
    \qmlproperty int TaskbarButton::progress.maximum
    \qmlproperty bool TaskbarButton::progress.visible
    \qmlproperty bool TaskbarButton::progress.paused
    \qmlproperty bool TaskbarButton::progress.stopped

    The taskbar progress indicator.

    \sa QWinTaskbarProgress
 */
QWinTaskbarProgress *QQuickTaskbarButton::progress() const
{
    return m_button->progress();
}

/*!
    \qmlpropertygroup ::TaskbarButton::overlay
    \qmlproperty url TaskbarButton::overlay.iconSource
    \qmlproperty string TaskbarButton::overlay.accessibleDescription

    The overlay icon and a description of the overlay for accessibility purposes.
 */
QQuickTaskbarOverlay *QQuickTaskbarButton::overlay() const
{
    return m_overlay;
}

void QQuickTaskbarButton::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    if (change == ItemSceneChange) {
        m_button->setWindow(data.window);
    }
    QQuickItem::itemChange(change, data);
}

QT_END_NAMESPACE
