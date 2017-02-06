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

#include "qsgdistancefieldutil_p.h"

#include <private/qsgadaptationlayer_p.h>
#ifndef QT_NO_OPENGL
# include <QtGui/private/qopenglengineshadersource_p.h>
#endif
#include <QtQuick/private/qsgcontext_p.h>

QT_BEGIN_NAMESPACE

static float qt_sg_envFloat(const char *name, float defaultValue)
{
    if (Q_LIKELY(!qEnvironmentVariableIsSet(name)))
        return defaultValue;
    bool ok = false;
    const float value = qgetenv(name).toFloat(&ok);
    return ok ? value : defaultValue;
}

static float defaultThresholdFunc(float glyphScale)
{
    static const float base = qt_sg_envFloat("QT_DF_BASE", 0.5f);
    static const float baseDev = qt_sg_envFloat("QT_DF_BASEDEVIATION", 0.065f);
    static const float devScaleMin = qt_sg_envFloat("QT_DF_SCALEFORMAXDEV", 0.15f);
    static const float devScaleMax = qt_sg_envFloat("QT_DF_SCALEFORNODEV", 0.3f);
    return base - ((qBound(devScaleMin, glyphScale, devScaleMax) - devScaleMin) / (devScaleMax - devScaleMin) * -baseDev + baseDev);
}

static float defaultAntialiasingSpreadFunc(float glyphScale)
{
    static const float range = qt_sg_envFloat("QT_DF_RANGE", 0.06f);
    return range / glyphScale;
}

QSGDistanceFieldGlyphCacheManager::QSGDistanceFieldGlyphCacheManager()
    : m_threshold_func(defaultThresholdFunc)
    , m_antialiasingSpread_func(defaultAntialiasingSpreadFunc)
{
}

QSGDistanceFieldGlyphCacheManager::~QSGDistanceFieldGlyphCacheManager()
{
    qDeleteAll(m_caches);
}

QSGDistanceFieldGlyphCache *QSGDistanceFieldGlyphCacheManager::cache(const QRawFont &font)
{
    return m_caches.value(font, 0);
}

void QSGDistanceFieldGlyphCacheManager::insertCache(const QRawFont &font, QSGDistanceFieldGlyphCache *cache)
{
    m_caches.insert(font, cache);
}

QT_END_NAMESPACE
