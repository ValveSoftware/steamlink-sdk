/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickangledirection_p.h"
#include "qquickcustomparticle_p.h"
#include "qquickellipseextruder_p.h"
#include "qquicktrailemitter_p.h"
#include "qquickfriction_p.h"
#include "qquickgravity_p.h"
#include "qquickimageparticle_p.h"
#include "qquickitemparticle_p.h"
#include "qquickage_p.h"
#include "qquicklineextruder_p.h"
#include "qquickmaskextruder_p.h"
#include "qquickparticleaffector_p.h"
#include "qquickparticleemitter_p.h"
#include "qquickparticleextruder_p.h"
#include "qquickparticlepainter_p.h"
#include "qquickparticlesmodule_p.h"
#include "qquickparticlesystem_p.h"
#include "qquickpointattractor_p.h"
#include "qquickpointdirection_p.h"
#include "qquickspritegoal_p.h"
#include "qquickdirection_p.h"
#include "qquicktargetdirection_p.h"
#include "qquickturbulence_p.h"
#include "qquickwander_p.h"
#include "qquickcumulativedirection_p.h"
#include "qquickcustomaffector_p.h"
#include "qquickrectangleextruder_p.h"
#include "qquickparticlegroup_p.h"
#include "qquickgroupgoal_p.h"

static void initResources()
{
    Q_INIT_RESOURCE(particles);
}

QT_BEGIN_NAMESPACE

void QQuickParticlesModule::defineModule()
{
    initResources();

    const char* uri = "QtQuick.Particles";

    qmlRegisterType<QQuickParticleSystem>(uri, 2, 0, "ParticleSystem");
    qmlRegisterType<QQuickParticleGroup>(uri, 2, 0, "ParticleGroup");

    qmlRegisterType<QQuickImageParticle>(uri, 2, 0, "ImageParticle");
    qmlRegisterType<QQuickCustomParticle>(uri, 2, 0, "CustomParticle");
    qmlRegisterType<QQuickItemParticle>(uri, 2, 0, "ItemParticle");

    qmlRegisterType<QQuickParticleEmitter>(uri, 2, 0, "Emitter");
    qmlRegisterType<QQuickTrailEmitter>(uri, 2, 0, "TrailEmitter");

    qmlRegisterType<QQuickEllipseExtruder>(uri, 2, 0, "EllipseShape");
    qmlRegisterType<QQuickRectangleExtruder>(uri, 2, 0, "RectangleShape");
    qmlRegisterType<QQuickLineExtruder>(uri, 2, 0, "LineShape");
    qmlRegisterType<QQuickMaskExtruder>(uri, 2, 0, "MaskShape");

    qmlRegisterType<QQuickPointDirection>(uri, 2, 0, "PointDirection");
    qmlRegisterType<QQuickAngleDirection>(uri, 2, 0, "AngleDirection");
    qmlRegisterType<QQuickTargetDirection>(uri, 2, 0, "TargetDirection");
    qmlRegisterType<QQuickCumulativeDirection>(uri, 2, 0, "CumulativeDirection");

    qmlRegisterType<QQuickCustomAffector>(uri, 2, 0, "Affector");
    qmlRegisterType<QQuickWanderAffector>(uri, 2, 0, "Wander");
    qmlRegisterType<QQuickFrictionAffector>(uri, 2, 0, "Friction");
    qmlRegisterType<QQuickAttractorAffector>(uri, 2, 0, "Attractor");
    qmlRegisterType<QQuickGravityAffector>(uri, 2, 0, "Gravity");
    qmlRegisterType<QQuickAgeAffector>(uri, 2, 0, "Age");
    qmlRegisterType<QQuickSpriteGoalAffector>(uri, 2, 0, "SpriteGoal");
    qmlRegisterType<QQuickGroupGoalAffector>(uri, 2, 0, "GroupGoal");
    qmlRegisterType<QQuickTurbulenceAffector>(uri, 2, 0 , "Turbulence");

    //Exposed just for completeness
    qmlRegisterUncreatableType<QQuickParticleAffector>(uri, 2, 0, "ParticleAffector",
                                                    QStringLiteral("Abstract type. Use one of the inheriting types instead."));
    qmlRegisterUncreatableType<QQuickParticlePainter>(uri, 2, 0, "ParticlePainter",
                                                   QStringLiteral("Abstract type. Use one of the inheriting types instead."));
    qmlRegisterUncreatableType<QQuickParticleExtruder>(uri, 2, 0, "ParticleExtruder",
                                                    QStringLiteral("Abstract type. Use one of the inheriting types instead."));
    qmlRegisterUncreatableType<QQuickDirection>(uri, 2, 0, "NullVector",
                                             QStringLiteral("Abstract type. Use one of the inheriting types instead."));
}

QT_END_NAMESPACE

