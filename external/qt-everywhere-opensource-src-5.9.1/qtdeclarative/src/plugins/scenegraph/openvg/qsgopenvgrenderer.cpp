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

#include "qsgopenvgrenderer_p.h"
#include "qsgopenvgcontext_p.h"
#include "qsgopenvgnodevisitor.h"
#include "qopenvgcontext_p.h"
#include "qsgopenvghelpers.h"

#include <QtGui/QWindow>

#include <VG/openvg.h>

QT_BEGIN_NAMESPACE

QSGOpenVGRenderer::QSGOpenVGRenderer(QSGRenderContext *context)
    : QSGRenderer(context)
{

}

QSGOpenVGRenderer::~QSGOpenVGRenderer()
{

}

void QSGOpenVGRenderer::renderScene(uint fboId)
{
    Q_UNUSED(fboId)
    class B : public QSGBindable
    {
    public:
        void bind() const { }
    } bindable;
    QSGRenderer::renderScene(bindable);
}

void QSGOpenVGRenderer::render()
{
    //Clear the window geometry with the clear color
    vgSetfv(VG_CLEAR_COLOR, 4, QSGOpenVGHelpers::qColorToVGColor(clearColor()).constData());
    vgClear(0, 0, VG_MAXINT, VG_MAXINT);

    // Visit each node to render scene
    QSGOpenVGNodeVisitor rendererVisitor;
    rendererVisitor.visitChildren(rootNode());
}

void QSGOpenVGRenderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    QSGRenderer::nodeChanged(node, state);
}

QT_END_NAMESPACE
