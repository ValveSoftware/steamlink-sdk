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

#include <Qt3DRender/qeffect.h>

#include <Qt3DQuickRender/private/quick3deffect_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

Quick3DEffect::Quick3DEffect(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QTechnique> Quick3DEffect::techniqueList()
{
    return QQmlListProperty<QTechnique>(this, 0,
                                       &Quick3DEffect::appendTechnique,
                                       &Quick3DEffect::techniqueCount,
                                       &Quick3DEffect::techniqueAt,
                                       &Quick3DEffect::clearTechniqueList);
}

QQmlListProperty<QParameter> Quick3DEffect::parameterList()
{
    return QQmlListProperty<QParameter>(this, 0,
                                        &Quick3DEffect::appendParameter,
                                        &Quick3DEffect::parametersCount,
                                        &Quick3DEffect::parameterAt,
                                        &Quick3DEffect::clearParameterList);
}

void Quick3DEffect::appendTechnique(QQmlListProperty<QTechnique> *list, QTechnique *bar)
{
    Quick3DEffect *eff = qobject_cast<Quick3DEffect*>(list->object);
    if (eff)
        eff->parentEffect()->addTechnique(bar);
}

// TO DO : Return a QAbstractTechnique once properly defined
QTechnique *Quick3DEffect::techniqueAt(QQmlListProperty<QTechnique> *list, int index)
{
    Quick3DEffect *eff = qobject_cast<Quick3DEffect*>(list->object);
    if (eff)
        return qobject_cast<QTechnique*>(eff->parentEffect()->techniques().at(index));
    return nullptr;
}

int Quick3DEffect::techniqueCount(QQmlListProperty<QTechnique> *list)
{
    Quick3DEffect *eff = qobject_cast<Quick3DEffect*>(list->object);
    if (eff)
        return eff->parentEffect()->techniques().count();
    return 0;
}

void Quick3DEffect::clearTechniqueList(QQmlListProperty<QTechnique> *list)
{
    Quick3DEffect *eff = qobject_cast<Quick3DEffect*>(list->object);
    if (eff) {
        // Ownership of techniques is handled by the QmlEngine so we shouldn't class clearTechniques
        // which deletes techniques
        const auto techniques = eff->parentEffect()->techniques();
        for (QTechnique *tech : techniques)
            eff->parentEffect()->removeTechnique(tech);
    }
}

void Quick3DEffect::appendParameter(QQmlListProperty<QParameter> *list, QParameter *param)
{
    Quick3DEffect *effect = qobject_cast<Quick3DEffect *>(list->object);
    qobject_cast<QEffect *>(effect->parentEffect())->addParameter(param);
}

QParameter *Quick3DEffect::parameterAt(QQmlListProperty<QParameter> *list, int index)
{
    Quick3DEffect *effect = qobject_cast<Quick3DEffect *>(list->object);
    return qobject_cast<QEffect *>(effect->parentEffect())->parameters().at(index);
}

int Quick3DEffect::parametersCount(QQmlListProperty<QParameter> *list)
{
    Quick3DEffect *effect = qobject_cast<Quick3DEffect *>(list->object);
    return qobject_cast<QEffect *>(effect->parentEffect())->parameters().count();
}

void Quick3DEffect::clearParameterList(QQmlListProperty<QParameter> *list)
{
    Quick3DEffect *effect = qobject_cast<Quick3DEffect *>(list->object);
    const auto parameters = qobject_cast<QEffect *>(effect->parentEffect())->parameters();
    for (QParameter *p : parameters)
        qobject_cast<QEffect *>(effect->parentEffect())->removeParameter(p);
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
