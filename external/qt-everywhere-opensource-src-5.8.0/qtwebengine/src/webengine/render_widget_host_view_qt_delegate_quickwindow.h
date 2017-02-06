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

#ifndef RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_QUICKWINDOW_H
#define RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_QUICKWINDOW_H

#include "render_widget_host_view_qt_delegate.h"

#include "render_widget_host_view_qt_delegate_quick.h"

#include <QQuickWindow>
#include <QScopedPointer>

namespace QtWebEngineCore {

class RenderWidgetHostViewQtDelegateQuickWindow : public QQuickWindow , public RenderWidgetHostViewQtDelegate {

public:
    RenderWidgetHostViewQtDelegateQuickWindow(RenderWidgetHostViewQtDelegate *realDelegate);
    ~RenderWidgetHostViewQtDelegateQuickWindow();

    virtual void initAsChild(WebContentsAdapterClient* container) Q_DECL_OVERRIDE;
    virtual void initAsPopup(const QRect&) Q_DECL_OVERRIDE;
    virtual QRectF screenRect() const Q_DECL_OVERRIDE;
    virtual QRectF contentsRect() const Q_DECL_OVERRIDE;
    virtual void setKeyboardFocus() Q_DECL_OVERRIDE {}
    virtual bool hasKeyboardFocus() Q_DECL_OVERRIDE { return false; }
    virtual void lockMouse() Q_DECL_OVERRIDE {}
    virtual void unlockMouse() Q_DECL_OVERRIDE {}
    virtual void show() Q_DECL_OVERRIDE;
    virtual void hide() Q_DECL_OVERRIDE;
    virtual bool isVisible() const Q_DECL_OVERRIDE;
    virtual QWindow* window() const Q_DECL_OVERRIDE;
    virtual QSGTexture *createTextureFromImage(const QImage &) Q_DECL_OVERRIDE;
    virtual QSGLayer *createLayer() Q_DECL_OVERRIDE;
    virtual QSGInternalImageNode *createImageNode() Q_DECL_OVERRIDE;
    virtual QSGTextureNode *createTextureNode() Q_DECL_OVERRIDE;
    virtual QSGRectangleNode *createRectangleNode() Q_DECL_OVERRIDE;
    virtual void update() Q_DECL_OVERRIDE;
    virtual void updateCursor(const QCursor &) Q_DECL_OVERRIDE;
    virtual void resize(int width, int height) Q_DECL_OVERRIDE;
    virtual void move(const QPoint &screenPos) Q_DECL_OVERRIDE;
    virtual void inputMethodStateChanged(bool) Q_DECL_OVERRIDE {}
    virtual void setClearColor(const QColor &) Q_DECL_OVERRIDE { }

private:
    QScopedPointer<RenderWidgetHostViewQtDelegate> m_realDelegate;
};

} // namespace QtWebEngineCore

#endif // RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_QUICKWINDOW_H
