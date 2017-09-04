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

#ifndef QSGOPENVGINTERNALIMAGENODE_H
#define QSGOPENVGINTERNALIMAGENODE_H

#include <private/qsgadaptationlayer_p.h>
#include "qsgopenvgrenderable.h"

#include <VG/openvg.h>

QT_BEGIN_NAMESPACE

class QSGOpenVGInternalImageNode : public QSGInternalImageNode, public QSGOpenVGRenderable
{
public:
    QSGOpenVGInternalImageNode();
    ~QSGOpenVGInternalImageNode();

    void render() override;

    void setTargetRect(const QRectF &rect) override;
    void setInnerTargetRect(const QRectF &rect) override;
    void setInnerSourceRect(const QRectF &rect) override;
    void setSubSourceRect(const QRectF &rect) override;
    void setTexture(QSGTexture *texture) override;
    void setMirror(bool mirror) override;
    void setMipmapFiltering(QSGTexture::Filtering filtering) override;
    void setFiltering(QSGTexture::Filtering filtering) override;
    void setHorizontalWrapMode(QSGTexture::WrapMode wrapMode) override;
    void setVerticalWrapMode(QSGTexture::WrapMode wrapMode) override;
    void update() override;

    void preprocess() override;

private:

    QRectF m_targetRect;
    QRectF m_innerTargetRect;
    QRectF m_innerSourceRect = QRectF(0, 0, 1, 1);
    QRectF m_subSourceRect = QRectF(0, 0, 1, 1);

    bool m_mirror = false;
    bool m_smooth = true;
    bool m_tileHorizontal = false;
    bool m_tileVertical = false;

    QSGTexture *m_texture = nullptr;

    VGImage m_subSourceRectImage = 0;
    bool m_subSourceRectImageDirty = true;
};

QT_END_NAMESPACE

#endif // QSGOPENVGINTERNALIMAGENODE_H
