/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qchannelcomponent.h"

#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

class QChannelComponentPrivate
{
public:
    QVector<QKeyFrame> m_keyFrames;
    QString m_name;
};

QChannelComponent::QChannelComponent()
    : d(new QChannelComponentPrivate)
{
}

QChannelComponent::QChannelComponent(const QString &name)
    : d(new QChannelComponentPrivate)
{
    d->m_name = name;
}

QChannelComponent::QChannelComponent(const QChannelComponent &rhs)
    : d(new QChannelComponentPrivate)
{
    *d = *(rhs.d);
}

QChannelComponent &QChannelComponent::operator=(const QChannelComponent &rhs)
{
    if (this != &rhs)
        *d = *(rhs.d);
    return *this;
}

QChannelComponent::~QChannelComponent()
{
}

void QChannelComponent::setName(const QString &name)
{
    d->m_name = name;
}

QString QChannelComponent::name() const
{
    return d->m_name;
}

int QChannelComponent::keyFrameCount() const
{
    return d->m_keyFrames.size();
}

void QChannelComponent::appendKeyFrame(const QKeyFrame &kf)
{
    d->m_keyFrames.append(kf);
}

void QChannelComponent::insertKeyFrame(int index, const QKeyFrame &kf)
{
    d->m_keyFrames.insert(index, kf);
}

void QChannelComponent::removeKeyFrame(int index)
{
    d->m_keyFrames.remove(index);
}

void QChannelComponent::clearKeyFrames()
{
    d->m_keyFrames.clear();
}

QChannelComponent::const_iterator QChannelComponent::begin() const Q_DECL_NOTHROW
{
    return d->m_keyFrames.begin();
}

QChannelComponent::const_iterator QChannelComponent::end() const Q_DECL_NOTHROW
{
    return d->m_keyFrames.end();
}

bool operator==(const QChannelComponent &lhs, const QChannelComponent &rhs) Q_DECL_NOTHROW
{
    return lhs.d->m_name == rhs.d->m_name &&
           lhs.d->m_keyFrames == rhs.d->m_keyFrames;
}

bool operator!=(const QChannelComponent &lhs, const QChannelComponent &rhs) Q_DECL_NOTHROW
{
    return lhs.d->m_name != rhs.d->m_name ||
           lhs.d->m_keyFrames != rhs.d->m_keyFrames;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
