/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDQUICKITEM_P_H
#define QWAYLANDQUICKITEM_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/QSGMaterialShader>
#include <QtQuick/QSGMaterial>

#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandOutput>

QT_BEGIN_NAMESPACE

class QWaylandSurfaceTextureProvider;
class QMutex;
class QOpenGLTexture;

class QWaylandBufferMaterialShader : public QSGMaterialShader
{
public:
    QWaylandBufferMaterialShader(QWaylandBufferRef::BufferFormatEgl format);

    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect) override;
    char const *const *attributeNames() const override;

protected:
    void initialize() override;

private:
    const QWaylandBufferRef::BufferFormatEgl m_format;
    int m_id_matrix;
    int m_id_opacity;
    QVarLengthArray<int, 3> m_id_tex;
};

class QWaylandBufferMaterial : public QSGMaterial
{
public:
    QWaylandBufferMaterial(QWaylandBufferRef::BufferFormatEgl format);
    ~QWaylandBufferMaterial();

    void setTextureForPlane(int plane, QOpenGLTexture *texture);

    void bind();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader() const override;

private:
    void setTextureParameters(GLenum target);
    void ensureTextures(int count);

    const QWaylandBufferRef::BufferFormatEgl m_format;
    QVarLengthArray<QOpenGLTexture*, 3> m_textures;
};

class QWaylandQuickItemPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QWaylandQuickItem)
public:
    QWaylandQuickItemPrivate()
        : QQuickItemPrivate()
        , view(Q_NULLPTR)
        , oldSurface(Q_NULLPTR)
        , provider(Q_NULLPTR)
        , paintEnabled(true)
        , touchEventsEnabled(true)
        , inputEventsEnabled(true)
        , isDragging(false)
        , newTexture(false)
        , focusOnClick(true)
        , sizeFollowsSurface(true)
        , connectedWindow(Q_NULLPTR)
        , origin(QWaylandSurface::OriginTopLeft)
    {
    }

    void init()
    {
        Q_Q(QWaylandQuickItem);
        if (!mutex)
            mutex = new QMutex;

        view.reset(new QWaylandView(q));
        q->setFlag(QQuickItem::ItemHasContents);

        q->update();

        q->setSmooth(true);

        setInputEventsEnabled(true);
        QObject::connect(q, &QQuickItem::windowChanged, q, &QWaylandQuickItem::updateWindow);
        QObject::connect(view.data(), &QWaylandView::surfaceChanged, q, &QWaylandQuickItem::surfaceChanged);
        QObject::connect(view.data(), &QWaylandView::surfaceChanged, q, &QWaylandQuickItem::handleSurfaceChanged);
        QObject::connect(view.data(), &QWaylandView::surfaceDestroyed, q, &QWaylandQuickItem::surfaceDestroyed);
        QObject::connect(view.data(), &QWaylandView::outputChanged, q, &QWaylandQuickItem::outputChanged);
        QObject::connect(view.data(), &QWaylandView::bufferLockedChanged, q, &QWaylandQuickItem::bufferLockedChanged);
        QObject::connect(view.data(), &QWaylandView::allowDiscardFrontBufferChanged, q, &QWaylandQuickItem::allowDiscardFrontBuffer);

        q->updateWindow();
    }

    void setInputEventsEnabled(bool enable)
    {
        Q_Q(QWaylandQuickItem);
        q->setAcceptedMouseButtons(enable ? (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton |
                                   Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::ExtraButton3 | Qt::ExtraButton4 |
                                   Qt::ExtraButton5 | Qt::ExtraButton6 | Qt::ExtraButton7 | Qt::ExtraButton8 |
                                   Qt::ExtraButton9 | Qt::ExtraButton10 | Qt::ExtraButton11 |
                                   Qt::ExtraButton12 | Qt::ExtraButton13) : Qt::NoButton);
        q->setAcceptHoverEvents(enable);
        inputEventsEnabled = enable;
    }

    bool shouldSendInputEvents() const { return view->surface() && inputEventsEnabled; }
    qreal scaleFactor() const;

    static QMutex *mutex;

    QScopedPointer<QWaylandView> view;
    QWaylandSurface *oldSurface;
    mutable QWaylandSurfaceTextureProvider *provider;
    bool paintEnabled;
    bool touchEventsEnabled;
    bool inputEventsEnabled;
    bool isDragging;
    bool newTexture;
    bool focusOnClick;
    bool sizeFollowsSurface;
    QPoint hoverPos;

    QQuickWindow *connectedWindow;
    QWaylandSurface::Origin origin;
    QPointer<QObject> subsurfaceHandler;
    QVector<QWaylandSeat *> touchingSeats;
};

QT_END_NAMESPACE

#endif  /*QWAYLANDQUICKITEM_P_H*/
