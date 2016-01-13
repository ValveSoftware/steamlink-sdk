/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgdistancefieldutil_p.h"

#include <private/qsgadaptationlayer_p.h>
#include <QtGui/private/qopenglengineshadersource_p.h>
#include <QtQuick/private/qsgcontext_p.h>

QT_BEGIN_NAMESPACE

static float defaultThresholdFunc(float glyphScale)
{
    static float base = qgetenv("QT_DF_BASE").isEmpty() ? 0.5f : qgetenv("QT_DF_BASE").toFloat();
    static float baseDev = qgetenv("QT_DF_BASEDEVIATION").isEmpty() ? 0.065f : qgetenv("QT_DF_BASEDEVIATION").toFloat();
    static float devScaleMin = qgetenv("QT_DF_SCALEFORMAXDEV").isEmpty() ? 0.15f : qgetenv("QT_DF_SCALEFORMAXDEV").toFloat();
    static float devScaleMax = qgetenv("QT_DF_SCALEFORNODEV").isEmpty() ? 0.3f : qgetenv("QT_DF_SCALEFORNODEV").toFloat();
    return base - ((qBound(devScaleMin, glyphScale, devScaleMax) - devScaleMin) / (devScaleMax - devScaleMin) * -baseDev + baseDev);
}

static float defaultAntialiasingSpreadFunc(float glyphScale)
{
    static float range = qgetenv("QT_DF_RANGE").isEmpty() ? 0.06f : qgetenv("QT_DF_RANGE").toFloat();
    return range / glyphScale;
}

QSGDistanceFieldGlyphCacheManager::QSGDistanceFieldGlyphCacheManager()
    : m_threshold_func(defaultThresholdFunc)
    , m_antialiasingSpread_func(defaultAntialiasingSpreadFunc)
{
}

QSGDistanceFieldGlyphCacheManager::~QSGDistanceFieldGlyphCacheManager()
{
    qDeleteAll(m_caches.values());
}

QSGDistanceFieldGlyphCache *QSGDistanceFieldGlyphCacheManager::cache(const QRawFont &font)
{
    return m_caches.value(fontKey(font), 0);
}

void QSGDistanceFieldGlyphCacheManager::insertCache(const QRawFont &font, QSGDistanceFieldGlyphCache *cache)
{
    m_caches.insert(fontKey(font), cache);
}

QString QSGDistanceFieldGlyphCacheManager::fontKey(const QRawFont &font)
{
    QFontEngine *fe = QRawFontPrivate::get(font)->fontEngine;
    if (!fe->faceId().filename.isEmpty()) {
        QByteArray keyName = fe->faceId().filename;
        if (font.style() != QFont::StyleNormal)
            keyName += QByteArray(" I");
        if (font.weight() != QFont::Normal)
            keyName += ' ' + QByteArray::number(font.weight());
        keyName += QByteArray(" DF");
        return QString::fromUtf8(keyName);
    } else {
        return QString::fromLatin1("%1_%2_%3_%4")
                  .arg(font.familyName())
                  .arg(font.styleName())
                  .arg(font.weight())
                  .arg(font.style());
    }
}

QT_END_NAMESPACE
