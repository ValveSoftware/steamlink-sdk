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

#include "quick3dtechnique_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DTechnique::Quick3DTechnique(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QRenderPass> Quick3DTechnique::renderPassList()
{
    return QQmlListProperty<QRenderPass>(this, 0,
                                         &Quick3DTechnique::appendRenderPass,
                                         &Quick3DTechnique::renderPassCount,
                                         &Quick3DTechnique::renderPassAt,
                                         &Quick3DTechnique::clearRenderPasses);
}

QQmlListProperty<QParameter> Quick3DTechnique::parameterList()
{
    return QQmlListProperty<QParameter>(this, 0,
                                        &Quick3DTechnique::appendParameter,
                                        &Quick3DTechnique::parametersCount,
                                        &Quick3DTechnique::parameterAt,
                                        &Quick3DTechnique::clearParameterList);
}

void Quick3DTechnique::appendParameter(QQmlListProperty<QParameter> *list, QParameter *param)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    technique->parentTechnique()->addParameter(param);
}

QParameter *Quick3DTechnique::parameterAt(QQmlListProperty<QParameter> *list, int index)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    return technique->parentTechnique()->parameters().at(index);
}

int Quick3DTechnique::parametersCount(QQmlListProperty<QParameter> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    return technique->parentTechnique()->parameters().count();
}

void Quick3DTechnique::clearParameterList(QQmlListProperty<QParameter> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    const auto parameters = technique->parentTechnique()->parameters();
    for (QParameter *p : parameters)
        technique->parentTechnique()->removeParameter(p);
}

void Quick3DTechnique::appendRenderPass(QQmlListProperty<QRenderPass> *list, QRenderPass *renderPass)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique) {
        technique->parentTechnique()->addRenderPass(renderPass);
    }
}

QRenderPass *Quick3DTechnique::renderPassAt(QQmlListProperty<QRenderPass> *list, int index)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique)
        return qobject_cast<QRenderPass *>(technique->parentTechnique()->renderPasses().at(index));
    return 0;
}

int Quick3DTechnique::renderPassCount(QQmlListProperty<QRenderPass> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique)
        return technique->parentTechnique()->renderPasses().size();
    return 0;
}

void Quick3DTechnique::clearRenderPasses(QQmlListProperty<QRenderPass> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique) {
        const auto passes = technique->parentTechnique()->renderPasses();
        for (QRenderPass *pass : passes)
            technique->parentTechnique()->removeRenderPass(pass);
    }
}

QQmlListProperty<QFilterKey> Quick3DTechnique::filterKeyList()
{
    return QQmlListProperty<QFilterKey>(this, 0,
                                         &Quick3DTechnique::appendFilterKey,
                                         &Quick3DTechnique::filterKeyCount,
                                         &Quick3DTechnique::filterKeyAt,
                                         &Quick3DTechnique::clearFilterKeyList);
}

void Quick3DTechnique::appendFilterKey(QQmlListProperty<QFilterKey> *list, QFilterKey *filterKey)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique) {
        if (!filterKey->parent())
            filterKey->setParent(technique->parentTechnique());
        technique->parentTechnique()->addFilterKey(filterKey);
    }
}

QFilterKey *Quick3DTechnique::filterKeyAt(QQmlListProperty<QFilterKey> *list, int index)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique)
        return technique->parentTechnique()->filterKeys().at(index);
    return 0;
}

int Quick3DTechnique::filterKeyCount(QQmlListProperty<QFilterKey> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique)
        return technique->parentTechnique()->filterKeys().size();
    return 0;
}

void Quick3DTechnique::clearFilterKeyList(QQmlListProperty<QFilterKey> *list)
{
    Quick3DTechnique *technique = qobject_cast<Quick3DTechnique *>(list->object);
    if (technique) {
        const auto keys = technique->parentTechnique()->filterKeys();
        for (QFilterKey *a : keys)
            technique->parentTechnique()->removeFilterKey(a);
    }
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
