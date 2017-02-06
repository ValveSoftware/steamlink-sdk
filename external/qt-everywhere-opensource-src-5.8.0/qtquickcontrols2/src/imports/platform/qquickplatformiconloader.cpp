/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
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

#include "qquickplatformiconloader_p.h"

#include <QtCore/qobject.h>
#include <QtCore/qmetaobject.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

QQuickPlatformIconLoader::QQuickPlatformIconLoader(int slot, QObject *parent)
    : m_parent(parent),
      m_slot(slot),
      m_enabled(false)
{
    Q_ASSERT(slot != -1 && parent);
}

bool QQuickPlatformIconLoader::isEnabled() const
{
    return m_enabled;
}

void QQuickPlatformIconLoader::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (m_enabled)
        loadIcon();
}

QIcon QQuickPlatformIconLoader::icon() const
{
    QIcon fallback = QPixmap::fromImage(image());
    return QIcon::fromTheme(m_iconName, fallback);
}

QUrl QQuickPlatformIconLoader::iconSource() const
{
    return m_iconSource;
}

void QQuickPlatformIconLoader::setIconSource(const QUrl& source)
{
    m_iconSource = source;
    if (m_enabled)
        loadIcon();
}

QString QQuickPlatformIconLoader::iconName() const
{
    return m_iconName;
}

void QQuickPlatformIconLoader::setIconName(const QString& name)
{
    m_iconName = name;
    if (m_enabled)
        loadIcon();
}

void QQuickPlatformIconLoader::loadIcon()
{
    if (m_iconSource.isEmpty()) {
        clear(m_parent);
    } else {
        load(qmlEngine(m_parent), m_iconSource);
        if (m_slot != -1 && isLoading()) {
            connectFinished(m_parent, m_slot);
            m_slot = -1;
        }
    }

    if (!isLoading())
        m_parent->metaObject()->method(m_slot).invoke(m_parent);
}

QT_END_NAMESPACE
