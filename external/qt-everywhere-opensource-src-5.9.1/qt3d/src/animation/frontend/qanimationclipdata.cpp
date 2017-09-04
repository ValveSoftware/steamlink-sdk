/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qanimationclipdata.h"

#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

class QAnimationClipDataPrivate
{
public:
    QVector<QChannel> m_channels;
    QString m_name;
};

/*!
    \class QAnimationClipData
    \inmodule Qt3DAnimation
    \brief Class containing the animation data
*/
QAnimationClipData::QAnimationClipData()
    : d(new QAnimationClipDataPrivate)
{
}

QAnimationClipData::QAnimationClipData(const QAnimationClipData &rhs)
    : d(new QAnimationClipDataPrivate)
{
    *d = *(rhs.d);
}

QAnimationClipData &QAnimationClipData::operator=(const QAnimationClipData &rhs)
{
    if (this != &rhs)
        *d = *(rhs.d);
    return *this;
}

QAnimationClipData::~QAnimationClipData()
{
}

void QAnimationClipData::setName(const QString &name)
{
    d->m_name = name;
}

QString QAnimationClipData::name() const
{
    return d->m_name;
}

int QAnimationClipData::channelCount() const
{
    return d->m_channels.size();
}

void QAnimationClipData::appendChannel(const QChannel &c)
{
    d->m_channels.append(c);
}

void QAnimationClipData::insertChannel(int index, const QChannel &c)
{
    d->m_channels.insert(index, c);
}

void QAnimationClipData::removeChannel(int index)
{
    d->m_channels.remove(index);
}

void QAnimationClipData::clearChannels()
{
    d->m_channels.clear();
}

bool QAnimationClipData::isValid() const Q_DECL_NOTHROW
{
    // TODO: Perform more thorough checks
    return !d->m_channels.isEmpty();
}

QAnimationClipData::const_iterator QAnimationClipData::begin() const Q_DECL_NOTHROW
{
    return d->m_channels.begin();
}

QAnimationClipData::const_iterator QAnimationClipData::end() const Q_DECL_NOTHROW
{
    return d->m_channels.end();
}


bool operator==(const QAnimationClipData &lhs, const QAnimationClipData &rhs) Q_DECL_NOTHROW
{
    return lhs.d->m_name == rhs.d->m_name &&
           lhs.d->m_channels == rhs.d->m_channels;
}

bool operator!=(const QAnimationClipData &lhs, const QAnimationClipData &rhs) Q_DECL_NOTHROW
{
    return lhs.d->m_name != rhs.d->m_name ||
           lhs.d->m_channels != rhs.d->m_channels;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
