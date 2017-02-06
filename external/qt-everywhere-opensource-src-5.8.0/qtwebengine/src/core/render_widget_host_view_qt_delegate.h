/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H
#define RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H

#include "qtwebenginecoreglobal.h"

#include <QRect>
#include <QtGui/qwindowdefs.h>

QT_BEGIN_NAMESPACE
class QCursor;
class QEvent;
class QPainter;
class QSGLayer;
class QSGNode;
class QSGRectangleNode;
class QSGTexture;
class QVariant;
class QWindow;
class QInputMethodEvent;

#if (QT_VERSION < QT_VERSION_CHECK(5, 8, 0))
class QSGImageNode;
typedef QSGImageNode QSGInternalImageNode;
class QSGSimpleTextureNode;
typedef QSGSimpleTextureNode QSGTextureNode;
#else
class QSGInternalImageNode;
class QSGImageNode;
typedef QSGImageNode QSGTextureNode;
#endif

QT_END_NAMESPACE

namespace QtWebEngineCore {

class WebContentsAdapterClient;

class QWEBENGINE_EXPORT RenderWidgetHostViewQtDelegateClient {
public:
    virtual ~RenderWidgetHostViewQtDelegateClient() { }
    virtual QSGNode *updatePaintNode(QSGNode *) = 0;
    virtual void notifyResize() = 0;
    virtual void notifyShown() = 0;
    virtual void notifyHidden() = 0;
    virtual void windowBoundsChanged() = 0;
    virtual void windowChanged() = 0;
    virtual bool forwardEvent(QEvent *) = 0;
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const = 0;
};

class QWEBENGINE_EXPORT RenderWidgetHostViewQtDelegate {
public:
    virtual ~RenderWidgetHostViewQtDelegate() { }
    virtual void initAsChild(WebContentsAdapterClient*) = 0;
    virtual void initAsPopup(const QRect&) = 0;
    virtual QRectF screenRect() const = 0;
    virtual QRectF contentsRect() const = 0;
    virtual void setKeyboardFocus() = 0;
    virtual bool hasKeyboardFocus() = 0;
    virtual void lockMouse() = 0;
    virtual void unlockMouse() = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual bool isVisible() const = 0;
    virtual QWindow* window() const = 0;
    virtual QSGTexture *createTextureFromImage(const QImage &) = 0;
    virtual QSGLayer *createLayer() = 0;
    virtual QSGInternalImageNode *createImageNode() = 0;
    virtual QSGTextureNode *createTextureNode() = 0;
    virtual QSGRectangleNode *createRectangleNode() = 0;
    virtual void update() = 0;
    virtual void updateCursor(const QCursor &) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void move(const QPoint &) = 0;
    virtual void inputMethodStateChanged(bool editorVisible) = 0;
    virtual void setClearColor(const QColor &color) = 0;
};

} // namespace QtWebEngineCore

#endif // RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H
