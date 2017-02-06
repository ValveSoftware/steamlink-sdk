/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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
#include "qwaylandshmbackingstore_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandsubsurface_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandabstractdecoration_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/QPainter>
#include <QMutexLocker>
#include <QLoggingCategory>

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

Q_DECLARE_LOGGING_CATEGORY(logCategory)

Q_LOGGING_CATEGORY(logCategory, "qt.qpa.wayland.backingstore")

QWaylandShmBuffer::QWaylandShmBuffer(QWaylandDisplay *display,
                     const QSize &size, QImage::Format format, int scale)
    : QWaylandBuffer()
    , mShmPool(0)
    , mMarginsImage(0)
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

    QWaylandShm* shm = display->shm();
    wl_shm_format wl_format = shm->formatFrom(format);
    mImage = QImage(data, size.width(), size.height(), stride, format);
    mImage.setDevicePixelRatio(qreal(scale));

    mShmPool = wl_shm_create_pool(shm->object(), fd, alloc);
    init(wl_shm_pool_create_buffer(mShmPool,0, size.width(), size.height(),
                                       stride, wl_format));
    close(fd);
}

QWaylandShmBuffer::~QWaylandShmBuffer(void)
{
    delete mMarginsImage;
    if (mImage.constBits())
        munmap((void *) mImage.constBits(), mImage.byteCount());
    if (mShmPool)
        wl_shm_pool_destroy(mShmPool);
}

QImage *QWaylandShmBuffer::imageInsideMargins(const QMargins &marginsIn)
{
    QMargins margins = marginsIn * int(mImage.devicePixelRatio());

    if (!margins.isNull() && margins != mMargins) {
        if (mMarginsImage) {
            delete mMarginsImage;
        }
        uchar *bits = const_cast<uchar *>(mImage.constBits());
        uchar *b_s_data = bits + margins.top() * mImage.bytesPerLine() + margins.left() * 4;
        int b_s_width = mImage.size().width() - margins.left() - margins.right();
        int b_s_height = mImage.size().height() - margins.top() - margins.bottom();
        mMarginsImage = new QImage(b_s_data, b_s_width,b_s_height,mImage.bytesPerLine(),mImage.format());
        mMarginsImage->setDevicePixelRatio(mImage.devicePixelRatio());
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
    , mPainting(false)
{

}

QWaylandShmBackingStore::~QWaylandShmBackingStore()
{
    if (QWaylandWindow *w = waylandWindow())
        w->setBackingStore(Q_NULLPTR);

//    if (mFrontBuffer == waylandWindow()->attached())
//        waylandWindow()->attach(0);

    qDeleteAll(mBuffers);
}

QPaintDevice *QWaylandShmBackingStore::paintDevice()
{
    return contentSurface();
}

void QWaylandShmBackingStore::beginPaint(const QRegion &)
{
    mPainting = true;
    ensureSize();

    waylandWindow()->setCanResize(false);
}

void QWaylandShmBackingStore::endPaint()
{
    mPainting = false;
    waylandWindow()->setCanResize(true);
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

    QMargins margins = windowDecorationMargins();

    waylandWindow()->commit(mFrontBuffer, region.translated(margins.left(), margins.top()));
}

void QWaylandShmBackingStore::resize(const QSize &size, const QRegion &)
{
    mRequestedSize = size;
}

QWaylandShmBuffer *QWaylandShmBackingStore::getBuffer(const QSize &size)
{
    foreach (QWaylandShmBuffer *b, mBuffers) {
        if (!b->busy()) {
            if (b->size() == size) {
                return b;
            } else {
                mBuffers.removeOne(b);
                if (mBackBuffer == b)
                    mBackBuffer = 0;
                delete b;
            }
        }
    }

    static const int MAX_BUFFERS = 5;
    if (mBuffers.count() < MAX_BUFFERS) {
        QImage::Format format = QPlatformScreen::platformScreenForWindow(window())->format();
        QWaylandShmBuffer *b = new QWaylandShmBuffer(mDisplay, size, format, waylandWindow()->scale());
        mBuffers.prepend(b);
        return b;
    }
    return 0;
}

void QWaylandShmBackingStore::resize(const QSize &size)
{
    QMargins margins = windowDecorationMargins();
    int scale = waylandWindow()->scale();
    QSize sizeWithMargins = (size + QSize(margins.left()+margins.right(),margins.top()+margins.bottom())) * scale;

    // We look for a free buffer to draw into. If the buffer is not the last buffer we used,
    // that is mBackBuffer, and the size is the same we memcpy the old content into the new
    // buffer so that QPainter is happy to find the stuff it had drawn before. If the new
    // buffer has a different size it needs to be redrawn completely anyway, and if the buffer
    // is the same the stuff is there already.
    // You can exercise the different codepaths with weston, switching between the gl and the
    // pixman renderer. With the gl renderer release events are sent early so we can effectively
    // run single buffered, while with the pixman renderer we have to use two.
    QWaylandShmBuffer *buffer = getBuffer(sizeWithMargins);
    while (!buffer) {
        qCDebug(logCategory, "QWaylandShmBackingStore: stalling waiting for a buffer to be released from the compositor...");

        mDisplay->blockingReadEvents();
        buffer = getBuffer(sizeWithMargins);
    }

    int oldSize = mBackBuffer ? mBackBuffer->image()->byteCount() : 0;
    // mBackBuffer may have been deleted here but if so it means its size was different so we wouldn't copy it anyway
    if (mBackBuffer != buffer && oldSize == buffer->image()->byteCount()) {
        memcpy(buffer->image()->bits(), mBackBuffer->image()->constBits(), buffer->image()->byteCount());
    }
    mBackBuffer = buffer;
    // ensure the new buffer is at the beginning of the list so next time getBuffer() will pick
    // it if possible
    if (mBuffers.first() != buffer) {
        mBuffers.removeOne(buffer);
        mBuffers.prepend(buffer);
    }

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

    qreal dp = sourceImage.devicePixelRatio();
    int dpWidth = int(sourceImage.width() / dp);
    int dpHeight = int(sourceImage.height() / dp);
    QMatrix sourceMatrix;
    sourceMatrix.scale(dp, dp);
    QRect target; // needs to be in device independent pixels

    //Top
    target.setX(0);
    target.setY(0);
    target.setWidth(dpWidth);
    target.setHeight(windowDecorationMargins().top());
    decorationPainter.drawImage(target, sourceImage, sourceMatrix.mapRect(target));

    //Left
    target.setWidth(windowDecorationMargins().left());
    target.setHeight(dpHeight);
    decorationPainter.drawImage(target, sourceImage, sourceMatrix.mapRect(target));

    //Right
    target.setX(dpWidth - windowDecorationMargins().right());
    target.setWidth(windowDecorationMargins().right());
    decorationPainter.drawImage(target, sourceImage, sourceMatrix.mapRect(target));

    //Bottom
    target.setX(0);
    target.setY(dpHeight - windowDecorationMargins().bottom());
    target.setWidth(dpWidth);
    target.setHeight(windowDecorationMargins().bottom());
    decorationPainter.drawImage(target, sourceImage, sourceMatrix.mapRect(target));
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

#if QT_CONFIG(opengl)
QImage QWaylandShmBackingStore::toImage() const
{
    // Invoked from QPlatformBackingStore::composeAndFlush() that is called
    // instead of flush() for widgets that have renderToTexture children
    // (QOpenGLWidget, QQuickWidget).

    return *contentSurface();
}
#endif  // opengl

}

QT_END_NAMESPACE
