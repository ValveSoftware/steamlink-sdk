/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt SVG module of the Qt Toolkit.
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

#include "qsvgfont_p.h"

#include "qpainter.h"
#include "qpen.h"
#include "qdebug.h"
#include "qpicture.h"

QT_BEGIN_NAMESPACE

QSvgGlyph::QSvgGlyph(QChar unicode, const QPainterPath &path, qreal horizAdvX)
    : m_unicode(unicode), m_path(path), m_horizAdvX(horizAdvX)
{

}


QSvgFont::QSvgFont(qreal horizAdvX)
    : m_horizAdvX(horizAdvX)
{
}


QString QSvgFont::familyName() const
{
    return m_familyName;
}


void QSvgFont::addGlyph(QChar unicode, const QPainterPath &path, qreal horizAdvX )
{
    m_glyphs.insert(unicode, QSvgGlyph(unicode, path,
                                       (horizAdvX==-1)?m_horizAdvX:horizAdvX));
}


void QSvgFont::draw(QPainter *p, const QPointF &point, const QString &str, qreal pixelSize, Qt::Alignment alignment) const
{
    p->save();
    p->translate(point);
    p->scale(pixelSize / m_unitsPerEm, -pixelSize / m_unitsPerEm);

    // Calculate the text width to be used for alignment
    int textWidth = 0;
    QString::const_iterator itr = str.constBegin();
    for ( ; itr != str.constEnd(); ++itr) {
        QChar unicode = *itr;
        if (!m_glyphs.contains(*itr)) {
            unicode = 0;
            if (!m_glyphs.contains(unicode))
                continue;
        }
        textWidth += static_cast<int>(m_glyphs[unicode].m_horizAdvX);
    }

    QPoint alignmentOffset(0, 0);
    if (alignment == Qt::AlignHCenter) {
        alignmentOffset.setX(-textWidth / 2);
    } else if (alignment == Qt::AlignRight) {
        alignmentOffset.setX(-textWidth);
    }

    p->translate(alignmentOffset);

    // since in SVG the embedded font ain't really a path
    // the outline has got to stay untransformed...
    qreal penWidth = p->pen().widthF();
    penWidth /= (pixelSize/m_unitsPerEm);
    QPen pen = p->pen();
    pen.setWidthF(penWidth);
    p->setPen(pen);

    itr = str.constBegin();
    for ( ; itr != str.constEnd(); ++itr) {
        QChar unicode = *itr;
        if (!m_glyphs.contains(*itr)) {
            unicode = 0;
            if (!m_glyphs.contains(unicode))
                continue;
        }
        p->drawPath(m_glyphs[unicode].m_path);
        p->translate(m_glyphs[unicode].m_horizAdvX, 0);
    }

    p->restore();
}

void QSvgFont::setFamilyName(const QString &name)
{
    m_familyName = name;
}

void QSvgFont::setUnitsPerEm(qreal upem)
{
    m_unitsPerEm = upem;
}

QT_END_NAMESPACE
