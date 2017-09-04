/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qanimationcliploader.h"
#include "qanimationcliploader_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

QAnimationClipLoaderPrivate::QAnimationClipLoaderPrivate()
    : QAbstractAnimationClipPrivate()
    , m_source()
    , m_status(QAnimationClipLoader::NotReady)
{
}

void QAnimationClipLoaderPrivate::setStatus(QAnimationClipLoader::Status status)
{
    Q_Q(QAnimationClipLoader);
    if (status != m_status) {
        m_status = status;
        const bool blocked = q->blockNotifications(true);
        emit q->statusChanged(m_status);
        q->blockNotifications(blocked);
    }
}

/*!
    \enum QAnimationClipLoader::Status

    This enum identifies the status of animation clip.

    \value NotReady              The clip has not been loaded yet
    \value Ready                 The clip was successfully loaded
    \value Error                 An error occurred while loading the clip
*/
/*!
    \class QAnimationClipLoader
    \inherits QAbstractAnimationClip
    \inmodule Qt3DAnimation
    \brief Enables loading key frame animation data from a file
*/

QAnimationClipLoader::QAnimationClipLoader(Qt3DCore::QNode *parent)
    : QAbstractAnimationClip(*new QAnimationClipLoaderPrivate, parent)
{
}

QAnimationClipLoader::QAnimationClipLoader(const QUrl &source,
                                           Qt3DCore::QNode *parent)
    : QAbstractAnimationClip(*new QAnimationClipLoaderPrivate, parent)
{
    setSource(source);
}

QAnimationClipLoader::QAnimationClipLoader(QAnimationClipLoaderPrivate &dd, Qt3DCore::QNode *parent)
    : QAbstractAnimationClip(dd, parent)
{
}

QAnimationClipLoader::~QAnimationClipLoader()
{
}

QUrl QAnimationClipLoader::source() const
{
    Q_D(const QAnimationClipLoader);
    return d->m_source;
}

QAnimationClipLoader::Status QAnimationClipLoader::status() const
{
    Q_D(const QAnimationClipLoader);
    return d->m_status;
}

void QAnimationClipLoader::setSource(const QUrl &source)
{
    Q_D(QAnimationClipLoader);
    if (d->m_source == source)
        return;

    d->m_source = source;
    emit sourceChanged(source);
}

void QAnimationClipLoader::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QAnimationClipLoader);
    if (change->type() == Qt3DCore::PropertyUpdated) {
        const Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
        if (e->propertyName() == QByteArrayLiteral("status"))
            d->setStatus(static_cast<QAnimationClipLoader::Status>(e->value().toInt()));
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QAnimationClipLoader::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QAnimationClipLoaderData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QAnimationClipLoader);
    data.source = d->m_source;
    return creationChange;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
