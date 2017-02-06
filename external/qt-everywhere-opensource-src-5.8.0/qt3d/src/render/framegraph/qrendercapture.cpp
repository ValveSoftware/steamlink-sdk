/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <Qt3DRender/qrendercapture.h>
#include <Qt3DRender/private/qrendercapture_p.h>
#include <Qt3DCore/QSceneChange>
#include <Qt3DCore/QPropertyUpdatedChange>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
 * \class Qt3DRender::QRenderCapture
 * \inmodule Qt3DRender
 *
 * \brief Frame graph node for render capture
 *
 * The QRenderCapture is used to capture rendering into an image at any render stage.
 * Capturing must be initiated by the user and one image is returned per capture request.
 * User can issue multiple render capture requests simultaniously, but only one request
 * is served per QRenderCapture instance per frame.
 *
 * \since 5.8
 */

/*!
 * \qmltype RenderCapture
 * \instantiates Qt3DRender::QRenderCapture
 * \inherits FrameGraphNode
 * \inqmlmodule Qt3D.Render
 * \since 5.8
 * \brief Capture rendering
 */

/*!
 * \class Qt3DRender::QRenderCaptureReply
 * \inmodule Qt3DRender
 *
 * \brief Receives the result of render capture request.
 *
 * An object, which receives the image from QRenderCapture::requestCapture.
 *
 * \since 5.8
 */

/*!
 * \qmltype RenderCaptureReply
 * \instantiates Qt3DRender::QRenderCaptureReply
 * \inherits QObject
 * \inqmlmodule Qt3D.Render
 * \since 5.8
 * \brief Receives render capture result.
 */

/*!
 * \qmlproperty variant Qt3D.Render::RenderCaptureReply::image
 *
 * Holds the image, which was produced as a result of render capture.
 */

/*!
 * \qmlproperty int Qt3D.Render::RenderCaptureReply::captureId
 *
 * Holds the captureId, which was passed to the renderCapture.
 */

/*!
 * \qmlproperty bool Qt3D.Render::RenderCaptureReply::complete
 *
 * Holds the complete state of the render capture.
 */

/*!
 * \qmlmethod void Qt3D.Render::RenderCaptureReply::saveToFile(fileName)
 *
 * Saves the render capture result as an image to \a fileName.
 */

/*!
 * \qmlmethod RenderCaptureReply Qt3D.Render::RenderCapture::requestCapture(int captureId)
 *
 * Used to request render capture. User can specify a \a captureId to identify
 * the request. The requestId does not have to be unique. Only one render capture result
 * is produced per requestCapture call even if the frame graph has multiple leaf nodes.
 * The function returns a QRenderCaptureReply object, which receives the captured image
 * when it is done. The user is reponsible for deallocating the returned object.
 */

/*!
 * \internal
 */
QRenderCaptureReplyPrivate::QRenderCaptureReplyPrivate()
    : QObjectPrivate()
    , m_captureId(0)
    , m_complete(false)
{

}

/*!
 * The constructor creates an instance with the specified \a parent.
 */
QRenderCaptureReply::QRenderCaptureReply(QObject *parent)
    : QObject(* new QRenderCaptureReplyPrivate, parent)
{

}

/*!
 * \property QRenderCaptureReply::image
 *
 * Holds the image, which was produced as a result of render capture.
 */
QImage QRenderCaptureReply::image() const
{
    Q_D(const QRenderCaptureReply);
    return d->m_image;
}

/*!
 * \property QRenderCaptureReply::captureId
 *
 * Holds the captureId, which was passed to the renderCapture.
 */
int QRenderCaptureReply::captureId() const
{
    Q_D(const QRenderCaptureReply);
    return d->m_captureId;
}

/*!
 * \property QRenderCaptureReply::complete
 *
 * Holds the complete state of the render capture.
 */
bool QRenderCaptureReply::isComplete() const
{
    Q_D(const QRenderCaptureReply);
    return d->m_complete;
}

/*!
 * Saves the render capture result as an image to \a fileName.
 */
void QRenderCaptureReply::saveToFile(const QString &fileName) const
{
    Q_D(const QRenderCaptureReply);
    if (d->m_complete)
        d->m_image.save(fileName);
}

/*!
 * \internal
 */
QRenderCapturePrivate::QRenderCapturePrivate()
    : QFrameGraphNodePrivate()
{
}

/*!
 * \internal
 */
QRenderCaptureReply *QRenderCapturePrivate::createReply(int captureId)
{
    QRenderCaptureReply *reply = new QRenderCaptureReply();
    reply->d_func()->m_captureId = captureId;
    m_waitingReplies.push_back(reply);
    return reply;
}

/*!
 * \internal
 */
QRenderCaptureReply *QRenderCapturePrivate::takeReply(int captureId)
{
    QRenderCaptureReply *reply = nullptr;
    for (int i = 0; i < m_waitingReplies.size(); ++i) {
        if (m_waitingReplies[i]->captureId() == captureId) {
            reply = m_waitingReplies[i];
            m_waitingReplies.remove(i);
            break;
        }
    }
    return reply;
}

/*!
 * \internal
 */
void QRenderCapturePrivate::setImage(QRenderCaptureReply *reply, const QImage &image)
{
    reply->d_func()->m_complete = true;
    reply->d_func()->m_image = image;
}

/*!
 * The constructor creates an instance with the specified \a parent.
 */
QRenderCapture::QRenderCapture(Qt3DCore::QNode *parent)
    : QFrameGraphNode(*new QRenderCapturePrivate, parent)
{
}

/*!
 * Used to request render capture. User can specify a \a captureId to identify
 * the request. The requestId does not have to be unique. Only one render capture result
 * is produced per requestCapture call even if the frame graph has multiple leaf nodes.
 * The function returns a QRenderCaptureReply object, which receives the captured image
 * when it is done. The user is reponsible for deallocating the returned object.
 */
QRenderCaptureReply *QRenderCapture::requestCapture(int captureId)
{
    Q_D(QRenderCapture);
    QRenderCaptureReply *reply = d->createReply(captureId);

    Qt3DCore::QPropertyUpdatedChangePtr change(new Qt3DCore::QPropertyUpdatedChange(id()));
    change->setPropertyName(QByteArrayLiteral("renderCaptureRequest"));
    change->setValue(QVariant::fromValue(captureId));
    d->notifyObservers(change);

    return reply;
}

/*!
 * \internal
 */
void QRenderCapture::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QRenderCapture);
    Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
    if (propertyChange->type() == Qt3DCore::PropertyUpdated) {
        if (propertyChange->propertyName() == QByteArrayLiteral("renderCaptureData")) {
            RenderCaptureDataPtr data = propertyChange->value().value<RenderCaptureDataPtr>();
            QRenderCaptureReply *reply = d->takeReply(data.data()->captureId);
            if (reply) {
                d->setImage(reply, data.data()->image);
                emit reply->completeChanged(true);
            }
        }
    }
}

/*!
 * \internal
 */
Qt3DCore::QNodeCreatedChangeBasePtr QRenderCapture::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QRenderCaptureInitData>::create(this);
    QRenderCaptureInitData &data = creationChange->data;
    data.captureId = 0;
    return creationChange;
}

} // Qt3DRender

QT_END_NAMESPACE
