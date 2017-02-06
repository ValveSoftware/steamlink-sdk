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

#include "quick3drenderpassfilter_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DRenderPassFilter::Quick3DRenderPassFilter(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QFilterKey> Quick3DRenderPassFilter::includeList()
{
    return QQmlListProperty<QFilterKey>(this, 0,
                                         &Quick3DRenderPassFilter::appendInclude,
                                         &Quick3DRenderPassFilter::includesCount,
                                         &Quick3DRenderPassFilter::includeAt,
                                         &Quick3DRenderPassFilter::clearIncludes);
}

QQmlListProperty<QParameter> Quick3DRenderPassFilter::parameterList()
{
    return QQmlListProperty<QParameter>(this, 0,
                                        &Quick3DRenderPassFilter::appendParameter,
                                        &Quick3DRenderPassFilter::parametersCount,
                                        &Quick3DRenderPassFilter::parameterAt,
                                        &Quick3DRenderPassFilter::clearParameterList);

}

void Quick3DRenderPassFilter::appendInclude(QQmlListProperty<QFilterKey> *list, QFilterKey *annotation)
{
    Quick3DRenderPassFilter *filter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    if (filter) {
        annotation->setParent(filter->parentRenderPassFilter());
        filter->parentRenderPassFilter()->addMatch(annotation);
    }
}

QFilterKey *Quick3DRenderPassFilter::includeAt(QQmlListProperty<QFilterKey> *list, int index)
{
    Quick3DRenderPassFilter *filter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    if (filter)
        return filter->parentRenderPassFilter()->matchAny().at(index);
    return 0;
}

int Quick3DRenderPassFilter::includesCount(QQmlListProperty<QFilterKey> *list)
{
    Quick3DRenderPassFilter *filter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    if (filter)
        return filter->parentRenderPassFilter()->matchAny().count();
    return 0;
}

void Quick3DRenderPassFilter::clearIncludes(QQmlListProperty<QFilterKey> *list)
{
    Quick3DRenderPassFilter *filter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    if (filter) {
        const auto criteria = filter->parentRenderPassFilter()->matchAny();
        for (QFilterKey *criterion : criteria)
            filter->parentRenderPassFilter()->removeMatch(criterion);
    }
}

void Quick3DRenderPassFilter::appendParameter(QQmlListProperty<QParameter> *list, QParameter *param)
{
    Quick3DRenderPassFilter *rPassFilter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    rPassFilter->parentRenderPassFilter()->addParameter(param);
}

QParameter *Quick3DRenderPassFilter::parameterAt(QQmlListProperty<QParameter> *list, int index)
{
    Quick3DRenderPassFilter *rPassFilter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    return rPassFilter->parentRenderPassFilter()->parameters().at(index);
}

int Quick3DRenderPassFilter::parametersCount(QQmlListProperty<QParameter> *list)
{
    Quick3DRenderPassFilter *rPassFilter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    return rPassFilter->parentRenderPassFilter()->parameters().count();
}

void Quick3DRenderPassFilter::clearParameterList(QQmlListProperty<QParameter> *list)
{
    Quick3DRenderPassFilter *rPassFilter = qobject_cast<Quick3DRenderPassFilter *>(list->object);
    const auto parameters = rPassFilter->parentRenderPassFilter()->parameters();
    for (QParameter *p : parameters)
        rPassFilter->parentRenderPassFilter()->removeParameter(p);
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
