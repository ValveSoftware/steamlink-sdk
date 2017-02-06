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

#include "quick3dtechniquefilter_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DTechniqueFilter::Quick3DTechniqueFilter(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QFilterKey> Quick3DTechniqueFilter::matchList()
{
    return QQmlListProperty<QFilterKey>(this, 0,
                                         &Quick3DTechniqueFilter::appendRequire,
                                         &Quick3DTechniqueFilter::requiresCount,
                                         &Quick3DTechniqueFilter::requireAt,
                                         &Quick3DTechniqueFilter::clearRequires);
}

QQmlListProperty<QParameter> Quick3DTechniqueFilter::parameterList()
{
    return QQmlListProperty<QParameter>(this, 0,
                                        &Quick3DTechniqueFilter::appendParameter,
                                        &Quick3DTechniqueFilter::parametersCount,
                                        &Quick3DTechniqueFilter::parameterAt,
                                        &Quick3DTechniqueFilter::clearParameterList);
}

void Quick3DTechniqueFilter::appendRequire(QQmlListProperty<QFilterKey> *list, QFilterKey *criterion)
{
    Quick3DTechniqueFilter *filter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    if (filter) {
        criterion->setParent(filter->parentTechniqueFilter());
        filter->parentTechniqueFilter()->addMatch(criterion);
    }
}

QFilterKey *Quick3DTechniqueFilter::requireAt(QQmlListProperty<QFilterKey> *list, int index)
{
    Quick3DTechniqueFilter *filter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    if (filter)
        return filter->parentTechniqueFilter()->matchAll().at(index);
    return 0;
}

int Quick3DTechniqueFilter::requiresCount(QQmlListProperty<QFilterKey> *list)
{
    Quick3DTechniqueFilter *filter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    if (filter)
        return filter->parentTechniqueFilter()->matchAll().size();
    return 0;
}

void Quick3DTechniqueFilter::clearRequires(QQmlListProperty<QFilterKey> *list)
{
    Quick3DTechniqueFilter *filter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    if (filter) {
        const auto criteria = filter->parentTechniqueFilter()->matchAll();
        for (QFilterKey *criterion : criteria)
            filter->parentTechniqueFilter()->removeMatch(criterion);
    }
}

void Quick3DTechniqueFilter::appendParameter(QQmlListProperty<QParameter> *list, QParameter *param)
{
    Quick3DTechniqueFilter *techniqueFilter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    techniqueFilter->parentTechniqueFilter()->addParameter(param);
}

QParameter *Quick3DTechniqueFilter::parameterAt(QQmlListProperty<QParameter> *list, int index)
{
    Quick3DTechniqueFilter *techniqueFilter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    return techniqueFilter->parentTechniqueFilter()->parameters().at(index);
}

int Quick3DTechniqueFilter::parametersCount(QQmlListProperty<QParameter> *list)
{
    Quick3DTechniqueFilter *techniqueFilter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    return techniqueFilter->parentTechniqueFilter()->parameters().count();
}

void Quick3DTechniqueFilter::clearParameterList(QQmlListProperty<QParameter> *list)
{
    Quick3DTechniqueFilter *techniqueFilter = qobject_cast<Quick3DTechniqueFilter *>(list->object);
    const auto parameters = techniqueFilter->parentTechniqueFilter()->parameters();
    for (QParameter *p : parameters)
        techniqueFilter->parentTechniqueFilter()->removeParameter(p);
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
