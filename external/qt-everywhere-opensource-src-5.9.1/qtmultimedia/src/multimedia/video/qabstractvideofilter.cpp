/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qabstractvideofilter.h"

QT_BEGIN_NAMESPACE

/*!
  \class QAbstractVideoFilter
  \since 5.5
  \brief The QAbstractVideoFilter class represents a filter that is applied to the video frames
  received by a VideoOutput type.
  \inmodule QtMultimedia

  \ingroup multimedia
  \ingroup multimedia_video

  QAbstractVideoFilter provides a convenient way for applications to run image
  processing, computer vision algorithms or any generic transformation or
  calculation on the output of a VideoOutput type, regardless of the source
  (video or camera). By providing a simple interface it allows applications and
  third parties to easily develop QML types that provide image processing
  algorithms using popular frameworks like \l{http://opencv.org}{OpenCV}. Due to
  the close integration with the final stages of the Qt Multimedia video
  pipeline, accelerated and possibly zero-copy solutions are feasible too: for
  instance, a plugin providing OpenCL-based algorithms can use OpenCL's OpenGL
  interop to use the OpenGL textures created by a hardware accelerated video
  decoder, without additional readbacks and copies.

  \note QAbstractVideoFilter is not always the best choice. To apply effects or
  transformations using OpenGL shaders to the image shown on screen, the
  standard Qt Quick approach of using ShaderEffect items in combination with
  VideoOutput should be used. VideoFilter is not a replacement for this. It is
  rather targeted for performing computations (that do not necessarily change
  the image shown on screen) and computer vision algorithms provided by
  external frameworks.

  QAbstractVideoFilter is meant to be subclassed. The subclasses are then registered to
  the QML engine, so they can be used as a QML type. The list of filters are
  assigned to a VideoOutput type via its \l{QtMultimedia::VideoOutput::filters}{filters}
  property.

  A single filter represents one transformation or processing step on
  a video frame. The output is a modified video frame, some arbitrary data or
  both. For example, image transformations will result in a different image,
  whereas an algorithm for detecting objects on an image will likely provide
  a list of rectangles.

  Arbitrary data can be represented as properties on the QAbstractVideoFilter subclass
  and on the QObject or QJSValue instances passed to its signals. What exactly
  these properties and signals are, is up to the individual video
  filters. Completion of the operations can be indicated by
  signals. Computations that do not result in a modified image will pass the
  input image through so that subsequent filters can be placed after them.

  Properties set on QAbstractVideoFilter serve as input to the computation, similarly
  to how uniform values are specified in ShaderEffect types. The changed
  property values are taken into use when the next video frame is processed.

  The typical usage is to subclass QAbstractVideoFilter and QVideoFilterRunnable:

  \badcode
    class MyFilterRunnable : public QVideoFilterRunnable {
    public:
        QVideoFrame run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags) { ... }
    };

    class MyFilter : public QAbstractVideoFilter {
    public:
        QVideoFilterRunnable *createFilterRunnable() { return new MyFilterRunnable; }
    signals:
        void finished(QObject *result);
    };

    int main(int argc, char **argv) {
        ...
        qmlRegisterType<MyFilter>("my.uri", 1, 0, "MyFilter");
        ...
    }
  \endcode

  MyFilter is thus accessible from QML:

  \badcode
    import my.uri 1.0

    Camera {
        id: camera
    }
    MyFilter {
        id: filter
        // set properties, they can also be animated
        onFinished: console.log("results of the computation: " + result)
    }
    VideoOutput {
        source: camera
        filters: [ filter ]
        anchors.fill: parent
    }
  \endcode

  This also allows providing filters in QML plugins, separately from the application.

  \sa VideoOutput, Camera, MediaPlayer, QVideoFilterRunnable
*/

/*!
  \class QVideoFilterRunnable
  \since 5.5
  \brief The QVideoFilterRunnable class represents the implementation of a filter
  that owns all graphics and computational resources, and performs the actual filtering
  or calculations.
  \inmodule QtMultimedia

  \ingroup multimedia
  \ingroup multimedia_video

  Video filters are split into QAbstractVideoFilter and corresponding QVideoFilterRunnable
  instances, similar to QQuickItem and QSGNode. This is necessary to support
  threaded rendering scenarios. When using the threaded render loop of the Qt
  Quick scene graph, all rendering happens on a dedicated thread.
  QVideoFilterRunnable instances always live on this thread and all its functions,
  run(), the constructor, and the destructor, are guaranteed to be invoked on
  that thread with the OpenGL context bound. QAbstractVideoFilter instances live on
  the main (GUI) thread, like any other QObject and QQuickItem instances
  created from QML.

  Once created, QVideoFilterRunnable instances are managed by Qt Multimedia and
  will be automatically destroyed and recreated when necessary, for example
  when the scene graph is invalidated or the QQuickWindow changes or is closed.
  Creation happens via the QAbstractVideoFilter::createFilterRunnable() factory function.

  \sa QAbstractVideoFilter
 */

/*!
  \fn QVideoFrame QVideoFilterRunnable::run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags)

  Reimplement this function to perform filtering or computation on the \a
  input video frame. Like the constructor and destructor, this function is
  always called on the render thread with the OpenGL context bound.

  Implementations that do not modify the video frame can simply return \a input.

  It is safe to access properties of the associated QAbstractVideoFilter instance from
  this function.

  \a input will not be mapped, it is up to this function to call QVideoFrame::map()
  and QVideoFrame::unmap() as necessary.

  \a surfaceFormat provides additional information, for example it can be used
  to determine which way is up in the input image as that is important for
  filters to operate on multiple platforms with multiple cameras.

  \a flags contains additional information about the filter's invocation. For
  example the LastInChain flag indicates that the filter is the last in a
  VideoOutput's associated filter list. This can be very useful in cases where
  multiple filters are chained together and the work is performed on image data
  in some custom format (for example a format specific to some computer vision
  framework). To avoid conversion on every filter in the chain, all
  intermediate filters can return a QVideoFrame hosting data in the custom
  format. Only the last, where the flag is set, returns a QVideoFrame in a
  format compatible with Qt.

  Filters that want to expose the results of their computation to Javascript
  code in QML can declare their own custom signals in the QAbstractVideoFilter
  subclass to indicate the completion of the operation. For filters that only
  calculate some results and do not modify the video frame, it is also possible
  to operate asynchronously. They can queue the necessary operations using the
  compute API and return from this function without emitting any signals. The
  signal indicating the completion is then emitted only when the compute API
  indicates that the operations were done and the results are available. Note
  that it is strongly recommended to represent the filter's output data as a
  separate instance of QJSValue or a QObject-derived class which is passed as a
  parameter to the signal and becomes exposed to the Javascript engine. In case
  of QObject the ownership of this object is controlled by the standard QML
  rules: if it has no parent, ownership is transferred to the Javascript engine,
  otherwise it stays with the emitter. Note that the signal connection may be
  queued,for example when using the threaded render loop of Qt Quick, and so the
  object must stay valid for a longer time, destroying it right after calling
  this function is not safe. Using a dedicated results object is guaranteed to
  be safe even when using threaded rendering. The same is not necessarily true
  for properties on the QAbstractVideoFilter instance itself: properties can
  safely be read in run() since the gui thread is blocked during that time but
  writing may become problematic.

  \note Avoid time consuming operations in this function as they block the
  entire rendering of the application.

  \note The handleType() and pixelFormat() of \a input is completely up to the
  video decoding backend on the platform in use. On some platforms different
  forms of input are used depending on the graphics stack. For example, when
  playing back videos on Windows with the WMF backend, QVideoFrame contains
  OpenGL-wrapped Direct3D textures in case of using ANGLE, but regular pixel
  data when using desktop OpenGL (opengl32.dll). Similarly, the video file
  format will often decide if the data is RGB or YUV, but this may also depend
  on the decoder and the configuration in use. The returned video frame does
  not have to be in the same format as the input, for example a filter with an
  input of a QVideoFrame backed by system memory can output a QVideoFrame with
  an OpenGL texture handle.

  \sa QVideoFrame, QVideoSurfaceFormat
 */

/*!
  \enum QVideoFilterRunnable::RunFlag

  \value LastInChain Indicates that the filter runnable's associated QAbstractVideoFilter
  is the last in the corresponding VideoOutput type's filters list, meaning
  that the returned frame is the one that is going to be presented to the scene
  graph without invoking any further filters.
 */

class QAbstractVideoFilterPrivate
{
public:
    QAbstractVideoFilterPrivate() :
        active(true)
    { }

    bool active;
};

/*!
  \internal
 */
QVideoFilterRunnable::~QVideoFilterRunnable()
{
}

/*!
  Constructs a new QAbstractVideoFilter instance with parent object \a parent.
 */
QAbstractVideoFilter::QAbstractVideoFilter(QObject *parent) :
    QObject(parent),
    d_ptr(new QAbstractVideoFilterPrivate)
{
}

/*!
  \internal
 */
QAbstractVideoFilter::~QAbstractVideoFilter()
{
    delete d_ptr;
}

/*!
    \property QAbstractVideoFilter::active
    \brief the active status of the filter.

    This is true if the filter is active, false otherwise.

    By default filters are active. When set to \c false, the filter will be
    ignored by the VideoOutput type.
 */
bool QAbstractVideoFilter::isActive() const
{
    Q_D(const QAbstractVideoFilter);
    return d->active;
}

void QAbstractVideoFilter::setActive(bool v)
{
    Q_D(QAbstractVideoFilter);
    if (d->active != v) {
        d->active = v;
        emit activeChanged();
    }
}

/*!
  \fn QVideoFilterRunnable *QAbstractVideoFilter::createFilterRunnable()

  Factory function to create a new instance of a QVideoFilterRunnable subclass
  corresponding to this filter.

  This function is called on the thread on which the Qt Quick scene graph
  performs rendering, with the OpenGL context bound. Ownership of the returned
  instance is transferred: the returned instance will live on the render thread
  and will be destroyed automatically when necessary.

  Typically, implementations of the function will simply construct a new
  QVideoFilterRunnable instance, passing \c this to the constructor as the
  filter runnables must know their associated QAbstractVideoFilter instance to
  access dynamic properties and optionally emit signals.
 */

QT_END_NAMESPACE
