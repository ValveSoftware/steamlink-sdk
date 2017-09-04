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

#include "qeffect.h"
#include "qeffect_p.h"
#include "qtechnique.h"
#include "qparameter.h"

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

QEffectPrivate::QEffectPrivate()
    : QNodePrivate()
{
}

/*!
    \class Qt3DRender::QEffect
    \inmodule Qt3DRender
    \inherits Qt3DCore::QNode
    \since 5.7
    \brief The base class for effects in a Qt 3D scene.

    The QEffect class combines a set of techniques and parameters used by those techniques to
    produce a rendering effect for a material.

    An QEffect instance should be shared among several QMaterial instances when possible.

    \code
    QEffect *effect = new QEffect();

    // Create technique, render pass and shader
    QTechnique *gl3Technique = new QTechnique();
    QRenderPass *gl3Pass = new QRenderPass();
    QShaderProgram *glShader = new QShaderProgram();

    // Set the shader on the render pass
    gl3Pass->setShaderProgram(glShader);

    // Add the pass to the technique
    gl3Technique->addRenderPass(gl3Pass);

    // Set the targeted GL version for the technique
    gl3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    gl3Technique->graphicsApiFilter()->setMajorVersion(3);
    gl3Technique->graphicsApiFilter()->setMinorVersion(1);
    gl3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    // Add the technique to the effect
    effect->addTechnique(gl3Technique);
    \endcode

    A QParameter defined on an Effect is overridden by a QParameter (of the same
    name) defined in a QMaterial, QTechniqueFilter, QRenderPassFilter.

    \sa QMaterial, QTechnique, QParameter
*/

/*!
    \qmltype Effect
    \instantiates Qt3DRender::QEffect
    \inherits Node
    \inqmlmodule Qt3D.Render
    \since 5.7
    \brief The base class for effects in a Qt 3D scene.

    The Effect type combines a set of techniques and parameters used by those techniques to
    produce a rendering effect for a material.

    An Effect instance should be shared among several Material instances when possible.

    A Parameter defined on an Effect is overridden by a QParameter (of the same
    name) defined in a Material, TechniqueFilter, RenderPassFilter.

    \code
    Effect {
        id: effect

        technique: [
            Technique {
                id: gl3Technique
                graphicsApiFilter {
                    api: GraphicsApiFilter.OpenGL
                    profile: GraphicsApiFilter.CoreProfile
                    majorVersion: 3
                    minorVersion: 1
                }
                renderPasses: [
                    RenderPass {
                        id: gl3Pass
                        shaderProgram: ShaderProgram {
                            ...
                        }
                    }
                ]
            }
        ]
    }
    \endcode

    \sa Material, Technique, Parameter
*/

QEffect::QEffect(QNode *parent)
    : QNode(*new QEffectPrivate, parent)
{
}

QEffect::~QEffect()
{
}

/*! \internal */
QEffect::QEffect(QEffectPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

/*!
    \qmlproperty list<Technique> Effect::techniques

    Holds the list of techniques used by this effect.
*/
/*!
    \qmlproperty list<Parameter> Effect::parameters

    Holds the list of parameters used by this effect.
    A parameter is used to set a corresponding uniform value in the shader used by this effect.
*/

/*!
 * Adds \a parameter to the effect. It sends a QPropertyNodeAddedChange to the backend.
 * The \a parameter will be used to set a corresponding uniform value in the shader used
 * by this effect.
 */
void QEffect::addParameter(QParameter *parameter)
{
    Q_D(QEffect);
    if (parameter && !d->m_parameters.contains(parameter)) {
        d->m_parameters.append(parameter);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(parameter, &QEffect::removeParameter, d->m_parameters);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (!parameter->parent())
            parameter->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), parameter);
            change->setPropertyName("parameter");
            d->notifyObservers(change);
        }
    }
}

/*!
 * Removes a parameter \a parameter from the effect.
 */
void QEffect::removeParameter(QParameter *parameter)
{
    Q_D(QEffect);

    if (parameter && d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), parameter);
        change->setPropertyName("parameter");
        d->notifyObservers(change);
    }
    d->m_parameters.removeOne(parameter);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(parameter);
}

/*!
 * Returns the list of parameters used by the effect.
 */
QVector<QParameter *> QEffect::parameters() const
{
    Q_D(const QEffect);
    return d->m_parameters;
}

/*!
 * Adds a new technique \a t to the effect. It sends a QPropertyNodeAddedChange to the backend.
 */
void QEffect::addTechnique(QTechnique *t)
{
    Q_ASSERT(t);
    Q_D(QEffect);
    if (t && !d->m_techniques.contains(t)) {
        d->m_techniques.append(t);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(t, &QEffect::removeTechnique, d->m_techniques);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, tit gets destroyed as well
        if (!t->parent())
            t->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), t);
            change->setPropertyName("technique");
            d->notifyObservers(change);
        }
    }
}

/*!
 * Removes a technique \a t from the effect.
 */
void QEffect::removeTechnique(QTechnique *t)
{
    Q_D(QEffect);
    if (t && d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), t);
        change->setPropertyName("technique");
        d->notifyObservers(change);
    }
    d->m_techniques.removeOne(t);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(t);
}

/*!
 * Returns the list of techniques used by the effect.
 */
QVector<QTechnique *> QEffect::techniques() const
{
    Q_D(const QEffect);
    return d->m_techniques;
}

Qt3DCore::QNodeCreatedChangeBasePtr QEffect::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QEffectData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QEffect);
    data.parameterIds = qIdsForNodes(d->m_parameters);
    data.techniqueIds = qIdsForNodes(d->m_techniques);
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
