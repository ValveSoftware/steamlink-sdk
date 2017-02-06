/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "quick3dshaderdataarray_p.h"
#include <private/qshaderdata_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

class Quick3DShaderDataArrayPrivate : public Qt3DCore::QNodePrivate
{
public:
    Quick3DShaderDataArrayPrivate()
        : QNodePrivate()
    {}

    QVector<QShaderData *> m_values;
};

Quick3DShaderDataArray::Quick3DShaderDataArray(QNode *parent)
    : QNode(*new Quick3DShaderDataArrayPrivate, parent)
{
}

QQmlListProperty<QShaderData> Quick3DShaderDataArray::valuesList()
{
    return QQmlListProperty<QShaderData>(this, 0,
                                         &Quick3DShaderDataArray::appendValue,
                                         &Quick3DShaderDataArray::valueCount,
                                         &Quick3DShaderDataArray::valueAt,
                                         &Quick3DShaderDataArray::clearValues);
}

QVector<QShaderData *> Quick3DShaderDataArray::values() const
{
    Q_D(const Quick3DShaderDataArray);
    return d->m_values;
}

// TODO: Avoid cloning here
//void Quick3DShaderDataArray::copy(const QNode *ref)
//{
//    QNode::copy(ref);
//    const Quick3DShaderDataArray *dataArray = static_cast<const Quick3DShaderDataArray *>(ref);
//    Q_FOREACH (QShaderData *v, dataArray->d_func()->m_values)
//        d_func()->m_values.append(static_cast<QShaderData *>(QNode::clone(v)));
//}

void Quick3DShaderDataArray::appendValue(QQmlListProperty<QShaderData> *list, QShaderData *bar)
{
    Quick3DShaderDataArray *self = static_cast<Quick3DShaderDataArray *>(list->object);
    static_cast<Quick3DShaderDataArrayPrivate *>(Quick3DShaderDataArrayPrivate::get(self))->m_values.append(bar);
}

QShaderData *Quick3DShaderDataArray::valueAt(QQmlListProperty<QShaderData> *list, int index)
{
    Quick3DShaderDataArray *self = static_cast<Quick3DShaderDataArray *>(list->object);
    return static_cast<Quick3DShaderDataArrayPrivate *>(Quick3DShaderDataArrayPrivate::get(self))->m_values.at(index);
}

int Quick3DShaderDataArray::valueCount(QQmlListProperty<QShaderData> *list)
{
    Quick3DShaderDataArray *self = static_cast<Quick3DShaderDataArray *>(list->object);
    return static_cast<Quick3DShaderDataArrayPrivate *>(Quick3DShaderDataArrayPrivate::get(self))->m_values.count();
}

void Quick3DShaderDataArray::clearValues(QQmlListProperty<QShaderData> *list)
{
    Quick3DShaderDataArray *self = static_cast<Quick3DShaderDataArray *>(list->object);
    static_cast<Quick3DShaderDataArrayPrivate *>(Quick3DShaderDataArrayPrivate::get(self))->m_values.clear();
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
