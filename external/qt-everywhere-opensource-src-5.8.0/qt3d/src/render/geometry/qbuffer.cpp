/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qbuffer.h"
#include "qbuffer_p.h"
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

QBufferPrivate::QBufferPrivate()
    : QNodePrivate()
    , m_usage(QBuffer::StaticDraw)
    , m_syncData(false)
{
}

/*!
 * \qmltype Buffer
 * \instantiates Qt3DRender::QBuffer
 * \inqmlmodule Qt3D.Render
 *
 * \brief Provides a data store for raw data to later be used as vertices or
 * uniforms.
 */

/*!
 * \qmlproperty QBuffer::UsageType Buffer::usage
 *
 * Holds the buffer usage.
 */

/*!
 * \qmlproperty QBuffer::BufferType Buffer::type
 *
 * Holds the buffer type.
 */

/*!
 * \qmlproperty bool Buffer::syncData
 *
 * Holds the syncData flag. When syncData is true, this will force data created
 * by a Qt3DRender::QBufferDataGenerator to also be updated on the frontend
 * Qt3DRender::QBuffer node. By default syncData is false.
 *
 * \note: This has no effect if the buffer's data was set directly using the data
 * property.
 */

/*!
 * \class Qt3DRender::QBuffer
 * \inmodule Qt3DRender
 *
 * \inherits Qt3DCore::QNode
 *
 * \brief Provides a data store for raw data to later be used as vertices or
 * uniforms.
 *
 * Data can either be provided directly using QBuffer::setData() or by
 * specifying a generator with QBuffer::setDataGenerator() and providing a
 * Qt3DRender::QBufferDataGeneratorPtr.
 *
 * When using a generator the data will be loaded asynchronously in a job. The
 * loaded data can be read back if the QBuffer::syncData flag is set to true.
 */

/*!
 * \fn void Qt3DRender::QBuffer::dataChanged(const QByteArray &bytes)
 *
 * This signal is emitted with \a bytes when data changes.
 */

/*!
    \class Qt3DRender::QBufferDataGenerator
    \inmodule Qt3DRender

    \inherits Qt3DRender::QAbstractFunctor

    \brief Provides a mechanism to generate buffer data from a job.

    The Qt3DRender::QBufferDataGenerator should be subclassed to provide a way
    to fill the data of a Qt3DRender::QBuffer. Such functors are executed at
    runtime in a Qt 3D job (likely in parallel with many other jobs). When
    providing a functor you must implement the operator() which will be called
    to generate the actual data. You must make sure that you have stored copies
    of anything you might need for it to execute properly. You should also
    implement the operator==. It will be used to compare with other functors
    and based on that allow the renderer to decide if a new functor should be
    executed or not.

    \note functors are useful when you can build data from a few set of
    attributes (e.g: building a sphere from a radius property). If you already
    have access to the buffer data, using Qt3DRender::QBuffer::setData() is
    likely more efficient.

    \code

    QByteArray createSphereMeshVertexData(float radius, int rings, int slices)
    {
        ...
    }

    class SphereVertexDataFunctor : public QBufferDataGenerator
    {
    public:
        SphereVertexDataFunctor(int rings, int slices, float radius)
            : m_rings(rings)
            , m_slices(slices)
            , m_radius(radius)
        {}

        QByteArray operator ()() Q_DECL_OVERRIDE
        {
            return createSphereMeshVertexData(m_radius, m_rings, m_slices);
        }

        bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
        {
            const SphereVertexDataFunctor *otherFunctor = functor_cast<SphereVertexDataFunctor>(&other);
            if (otherFunctor != nullptr)
                return (otherFunctor->m_rings == m_rings &&
                        otherFunctor->m_slices == m_slices &&
                        otherFunctor->m_radius == m_radius);
            return false;
        }

        QT3D_FUNCTOR(SphereVertexDataFunctor)

    private:
        int m_rings;
        int m_slices;
        float m_radius;
    };

    \endcode

    The QT3D_FUNCTOR macro should be added when subclassing. This allows you to
    use functor_cast in your comparison operator to make sure that the other
    functor is of the same type as the one your are trying to compare against.
*/

/*!
    \fn Qt3DRender::QBufferDataGenerator::operator()()

    Should be implemented to return the buffer data as a QByteArray when called.
  */

/*!
    \fn Qt3DRender::QBufferDataGenerator::operator ==(const QBufferDataGenerator &other) const

    Should be reimplemented to return true when two generators are identical,
    false otherwise.

    \note The renderer uses this comparison to decide whether data for a buffer
    needs to be reuploaded or not when the functor on a Qt3DRender::QBuffer
    changes.
 */

/*!
 * \enum QBuffer::BufferType
 *
 * The type of the buffer.
 *
 * \value VertexBuffer
 *        GL_ARRAY_BUFFER
 * \value IndexBuffer
 *        GL_ELEMENT_ARRAY_BUFFER
 * \value PixelPackBuffer
 *        GL_PIXEL_PACK_BUFFER
 * \value PixelUnpackBuffer
 *        GL_PIXEL_UNPACK_BUFFER
 * \value UniformBuffer
 *        GL_UNIFORM_BUFFER
 * \value ShaderStorageBuffer
 *        GL_SHADER_STORAGE_BUFFER
 */

/*!
 * \enum QBuffer::UsageType
 *
 * The type of the usage.
 *
 * \value StreamDraw
 *        GL_STREAM_DRAW
 * \value StreamRead
 *        GL_STREAM_READ
 * \value StreamCopy
 *        GL_STREAM_COPY
 * \value StaticDraw
 *        GL_STATIC_DRAW
 * \value StaticRead
 *        GL_STATIC_READ
 * \value StaticCopy
 *        GL_STATIC_COPY
 * \value DynamicDraw
 *        GL_DYNAMIC_DRAW
 * \value DynamicRead
 *        GL_DYNAMIC_READ
 * \value DynamicCopy
 *        GL_DYNAMIC_COPY
 */

/*!
 * \typedef Qt3DRender::QBufferDataGeneratorPtr
 * \relates QBuffer
 */

/*!
 * Constructs a new QBuffer of buffer type \a ty with \a parent.
 */
QBuffer::QBuffer(QBuffer::BufferType ty, QNode *parent)
    : QNode(*new QBufferPrivate(), parent)
{
    Q_D(QBuffer);
    d->m_type = ty;
}

/*!
 * \internal
 */
QBuffer::~QBuffer()
{
}

/*!
 * \internal
 */
void QBuffer::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    QPropertyUpdatedChangePtr e = qSharedPointerCast<QPropertyUpdatedChange>(change);
    if (e->type() == PropertyUpdated && e->propertyName() == QByteArrayLiteral("data")) {
        const bool blocked = blockNotifications(true);
        setData(e->value().toByteArray());
        blockNotifications(blocked);
    }
}

/*!
 * Sets \a bytes as data.
 */
void QBuffer::setData(const QByteArray &bytes)
{
    Q_D(QBuffer);
    if (bytes != d->m_data) {
        d->m_data = bytes;
        Qt3DCore::QNodePrivate::get(this)->notifyPropertyChange("data", QVariant::fromValue(d->m_data));
        emit dataChanged(bytes);
    }
}

/*!
 * \Update the data.
 */
void QBuffer::updateData(int offset, const QByteArray &bytes)
{
    Q_D(QBuffer);
    Q_ASSERT(offset >= 0 && (offset + bytes.size()) <= d->m_data.size());

    // Update data
    d->m_data.replace(offset, bytes.size(), bytes);
    const bool blocked = blockNotifications(true);
    emit dataChanged(d->m_data);
    blockNotifications(blocked);

    QBufferUpdate updateData;
    updateData.offset = offset;
    updateData.data = bytes;

    auto e = QPropertyUpdatedChangePtr::create(id());
    e->setPropertyName("updateData");
    e->setValue(QVariant::fromValue(updateData));
    notifyObservers(e);
}

/*!
 * \return the data.
 */
QByteArray QBuffer::data() const
{
    Q_D(const QBuffer);
    return d->m_data;
}

/*!
 * \property QBuffer::usage
 *
 * Holds the buffer usage.
 */
QBuffer::UsageType QBuffer::usage() const
{
    Q_D(const QBuffer);
    return d->m_usage;
}

void QBuffer::setUsage(QBuffer::UsageType usage)
{
    Q_D(QBuffer);
    if (usage != d->m_usage) {
        d->m_usage = usage;
        emit usageChanged(usage);
    }
}

/*!
 * \property QBuffer::type
 *
 * Holds the buffer type.
 */
QBuffer::BufferType QBuffer::type() const
{
    Q_D(const QBuffer);
    return d->m_type;
}

/*!
 * Sets the buffer \a functor.
 */
void QBuffer::setDataGenerator(const QBufferDataGeneratorPtr &functor)
{
    Q_D(QBuffer);
    if (functor && d->m_functor && *functor == *d->m_functor)
        return;
    d->m_functor = functor;
    if (d->m_changeArbiter != nullptr) {
        auto change = QPropertyUpdatedChangePtr::create(d->m_id);
        change->setPropertyName("dataGenerator");
        change->setValue(QVariant::fromValue(d->m_functor));
        d->notifyObservers(change);
    }
}

/*!
 * \return the buffer functor.
 */
QBufferDataGeneratorPtr QBuffer::dataGenerator() const
{
    Q_D(const QBuffer);
    return d->m_functor;
}

/*!
 * \property QBuffer::syncData
 *
 * Holds the syncData flag. When syncData is true, this will force data created
 * by a Qt3DRender::QBufferDataGenerator to also be updated on the frontend
 * Qt3DRender::QBuffer node. By default syncData is false.
 *
 * \note: This has no effect if the buffer's data was set directly using the data
 * property.
 */
void QBuffer::setSyncData(bool syncData)
{
    Q_D(QBuffer);
    if (d->m_syncData != syncData) {
        d->m_syncData = syncData;
        emit syncDataChanged(syncData);
    }
}

bool QBuffer::isSyncData() const
{
    Q_D(const QBuffer);
    return d->m_syncData;
}

void QBuffer::setType(QBuffer::BufferType type)
{
    Q_D(QBuffer);
    if (type != d->m_type) {
        d->m_type = type;
        emit typeChanged(type);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QBuffer::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QBufferData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QBuffer);
    data.data = d->m_data;
    data.type = d->m_type;
    data.usage = d->m_usage;
    data.functor = d->m_functor;
    data.syncData = d->m_syncData;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
