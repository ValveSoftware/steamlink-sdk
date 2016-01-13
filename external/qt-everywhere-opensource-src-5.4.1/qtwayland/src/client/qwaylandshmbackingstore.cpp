/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qwaylandshmbackingstore_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandabstractdecoration_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/QPainter>
#include <QMutexLocker>

#include <wayland-client.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

QT_BEGIN_NAMESPACE

QWaylandShmBuffer::QWaylandShmBuffer(QWaylandDisplay *display,
                     const QSize &size, QImage::Format format)
    : mMarginsImage(0)
{
    int stride = size.width() * 4;
    int alloc = stride * size.height();
    char filename[] = "/tmp/wayland-shm-XXXXXX";
    int fd = mkstemp(filename);
    if (fd < 0) {
        qWarning("mkstemp %s failed: %s", filename, strerror(errno));
        return;
    }
    int flags = fcntl(fd, F_GETFD);
    if (flags != -1)
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

    if (ftruncate(fd, alloc) < 0) {
        qWarning("ftruncate failed: %s", strerror(errno));
        close(fd);
        return;
    }
    uchar *data = (uchar *)
            mmap(NULL, alloc, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unlink(filename);

    if (data == (uchar *) MAP_FAILED) {
        qWarning("mmap /dev/zero failed: %s", strerror(errno));
        close(fd);
        return;
    }

    mImage = QImage(data, size.width(), size.height(), stride, format);
    mShmPool = wl_shm_create_pool(display->shm(), fd, alloc);
    mBuffer = wl_shm_pool_create_buffer(mShmPool,0, size.width(), size.height(),
                                       stride, WL_SHM_FORMAT_ARGB8888);
    close(fd);
}

QWaylandShmBuffer::~QWaylandShmBuffer(void)
{
    delete mMarginsImage;
    munmap((void *) mImage.constBits(), mImage.byteCount());
    wl_buffer_destroy(mBuffer);
    wl_shm_pool_destroy(mShmPool);
}

QImage *QWaylandShmBuffer::imageInsideMargins(const QMargins &margins)
{
    if (!margins.isNull() && margins != mMargins) {
        if (mMarginsImage) {
            delete mMarginsImage;
        }
        uchar *bits = const_cast<uchar *>(mImage.constBits());
        uchar *b_s_data = bits + margins.top() * mImage.bytesPerLine() + margins.left() * 4;
        int b_s_width = mImage.size().width() - margins.left() - margins.right();
        int b_s_height = mImage.size().height() - margins.top() - margins.bottom();
        mMarginsImage = new QImage(b_s_data, b_s_width,b_s_height,mImage.bytesPerLine(),mImage.format());
    }
    if (margins.isNull()) {
        delete mMarginsImage;
        mMarginsImage = 0;
    }

    mMargins = margins;
    if (!mMarginsImage)
        return &mImage;

    return mMarginsImage;

}

QWaylandShmBackingStore::QWaylandShmBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , mDisplay(QWaylandScreen::waylandScreenFromWindow(window)->display())
    , mFrontBuffer(0)
    , mBackBuffer(0)
    , mFrontBufferIsDirty(false)
    , mPainting(false)
    , mFrameCallback(0)
{

}

QWaylandShmBackingStore::~QWaylandShmBackingStore()
{
    if (QWaylandWindow *w = waylandWindow())
        w->setBackingStore(Q_NULLPTR);

    if (mFrameCallback)
        wl_callback_destroy(mFrameCallback);

//    if (mFrontBuffer == waylandWindow()->attached())
//        waylandWindow()->attach(0);

    if (mFrontBuffer != mBackBuffer)
        delete mFrontBuffer;

    delete mBackBuffer;
}

QPaintDevice *QWaylandShmBackingStore::paintDevice()
{
    return contentSurface();
}

void QWaylandShmBackingStore::beginPaint(const QRegion &)
{
    mPainting = true;
    ensureSize();

    QWaylandWindow *window = waylandWindow();
    if (window->attached() && mBackBuffer == window->attached() && mFrameCallback)
        window->waitForFrameSync();

    window->setCanResize(false);
}

void QWaylandShmBackingStore::endPaint()
{
    mPainting = false;
    waylandWindow()->setCanResize(true);
}

void QWaylandShmBackingStore::hidden()
{
    QMutexLocker lock(&mMutex);
    if (mFrameCallback) {
        wl_callback_destroy(mFrameCallback);
        mFrameCallback = Q_NULLPTR;
    }
}

void QWaylandShmBackingStore::ensureSize()
{
    waylandWindow()->setBackingStore(this);
    waylandWindow()->createDecoration();
    resize(mRequestedSize);
}

void QWaylandShmBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    // Invoked when the window is of type RasterSurface or when the window is
    // RasterGLSurface and there are no child widgets requiring OpenGL composition.

    // For the case of RasterGLSurface + having to compose, the composeAndFlush() is
    // called instead. The default implementation from QPlatformBackingStore is sufficient
    // however so no need to reimplement that.

    Q_UNUSED(window);
    Q_UNUSED(offset);

    if (windowDecoration() && windowDecoration()->isDirty())
        updateDecorations();

    mFrontBuffer = mBackBuffer;

    if (mFrameCallback) {
        mFrontBufferIsDirty = true;
        return;
    }

    mFrameCallback = waylandWindow()->frame();
    wl_callback_add_listener(mFrameCallback,&frameCallbackListener,this);
    QMargins margins = windowDecorationMargins();

    bool damageAll = false;
    if (waylandWindow()->attached() != mFrontBuffer) {
        delete waylandWindow()->attached();
        damageAll = true;
    }
    waylandWindow()->attachOffset(mFrontBuffer);

    if (damageAll) {
        //need to damage it all, otherwise the attach offset may screw up
        waylandWindow()->damage(QRect(QPoint(0,0),mFrontBuffer->size()));
    } else {
        QVector<QRect> rects = region.rects();
        for (int i = 0; i < rects.size(); i++) {
            QRect rect = rects.at(i);
            rect.translate(margins.left(),margins.top());
            waylandWindow()->damage(rect);
        }
    }
    waylandWindow()->commit();
    mFrontBufferIsDirty = false;
}

void QWaylandShmBackingStore::resize(const QSize &size, const QRegion &)
{
    mRequestedSize = size;
}

void QWaylandShmBackingStore::resize(const QSize &size)
{
    QMargins margins = windowDecorationMargins();
    QSize sizeWithMargins = size + QSize(margins.left()+margins.right(),margins.top()+margins.bottom());

    QImage::Format format = QPlatformScreen::platformScreenForWindow(window())->format();

    if (mBackBuffer != NULL && mBackBuffer->size() == sizeWithMargins)
        return;

    if (mBackBuffer != mFrontBuffer) {
        delete mBackBuffer; //we delete the attached buffer when we flush
    }

    mBackBuffer = new QWaylandShmBuffer(mDisplay, sizeWithMargins, format);

    if (windowDecoration() && window()->isVisible())
        windowDecoration()->update();
}

QImage *QWaylandShmBackingStore::entireSurface() const
{
    return mBackBuffer->image();
}

QImage *QWaylandShmBackingStore::contentSurface() const
{
    return windowDecoration() ? mBackBuffer->imageInsideMargins(windowDecorationMargins()) : mBackBuffer->image();
}

void QWaylandShmBackingStore::updateDecorations()
{
    QPainter decorationPainter(entireSurface());
    decorationPainter.setCompositionMode(QPainter::CompositionMode_Source);
    QImage sourceImage = windowDecoration()->contentImage();
    QRect target;
    //Top
    target.setX(0);
    target.setY(0);
    target.setWidth(sourceImage.width());
    target.setHeight(windowDecorationMargins().top());
    decorationPainter.drawImage(target, sourceImage, target);

    //Left
    target.setWidth(windowDecorationMargins().left());
    target.setHeight(sourceImage.height());
    decorationPainter.drawImage(target, sourceImage, target);

    //Right
    target.setX(sourceImage.width() - windowDecorationMargins().right());
    decorationPainter.drawImage(target, sourceImage, target);

    //Bottom
    target.setX(0);
    target.setY(sourceImage.height() - windowDecorationMargins().bottom());
    target.setWidth(sourceImage.width());
    target.setHeight(windowDecorationMargins().bottom());
    decorationPainter.drawImage(target, sourceImage, target);
}

QWaylandAbstractDecoration *QWaylandShmBackingStore::windowDecoration() const
{
    return waylandWindow()->decoration();
}

QMargins QWaylandShmBackingStore::windowDecorationMargins() const
{
    if (windowDecoration())
        return windowDecoration()->margins();
    return QMargins();
}

QWaylandWindow *QWaylandShmBackingStore::waylandWindow() const
{
    return static_cast<QWaylandWindow *>(window()->handle());
}

#ifndef QT_NO_OPENGL
QImage QWaylandShmBackingStore::toImage() const
{
    // Invoked from QPlatformBackingStore::composeAndFlush() that is called
    // instead of flush() for widgets that have renderToTexture children
    // (QOpenGLWidget, QQuickWidget).

    return *contentSurface();
}
#endif // QT_NO_OPENGL

void QWaylandShmBackingStore::done(void *data, wl_callback *callback, uint32_t time)
{
    Q_UNUSED(time);
    QWaylandShmBackingStore *self =
            static_cast<QWaylandShmBackingStore *>(data);
    if (callback != self->mFrameCallback) // others, like QWaylandWindow, may trigger callbacks too
        return;
    QMutexLocker lock(&self->mMutex);
    QWaylandWindow *window = self->waylandWindow();
    wl_callback_destroy(self->mFrameCallback);
    self->mFrameCallback = 0;


    if (self->mFrontBufferIsDirty && !self->mPainting) {
        self->mFrontBufferIsDirty = false;
        self->mFrameCallback = wl_surface_frame(window->object());
        wl_callback_add_listener(self->mFrameCallback,&self->frameCallbackListener,self);
        if (self->mFrontBuffer != window->attached()) {
            delete window->attached();
        }
        window->attachOffset(self->mFrontBuffer);
        window->damage(QRect(QPoint(0,0),self->mFrontBuffer->size()));
        window->commit();
    }
}

const struct wl_callback_listener QWaylandShmBackingStore::frameCallbackListener = {
    QWaylandShmBackingStore::done
};

QT_END_NAMESPACE
