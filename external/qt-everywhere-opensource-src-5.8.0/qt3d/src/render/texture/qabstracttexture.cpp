/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qabstracttexture.h"
#include "qabstracttexture_p.h"
#include <Qt3DRender/qabstracttextureimage.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

QAbstractTexturePrivate::QAbstractTexturePrivate()
    : QNodePrivate()
    , m_target(QAbstractTexture::Target2D)
    , m_format(QAbstractTexture::Automatic)
    , m_width(1)
    , m_height(1)
    , m_depth(1)
    , m_autoMipMap(false)
    , m_minFilter(QAbstractTexture::Nearest)
    , m_magFilter(QAbstractTexture::Nearest)
    , m_status(QAbstractTexture::None)
    , m_maximumAnisotropy(1.0f)
    , m_comparisonFunction(QAbstractTexture::CompareLessEqual)
    , m_comparisonMode(QAbstractTexture::CompareNone)
    , m_layers(1)
    , m_samples(1)
{
}

void QAbstractTexturePrivate::setDataFunctor(const QTextureGeneratorPtr &generator)
{
    if (generator != m_dataFunctor) {
        m_dataFunctor = generator;
        auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(m_id);
        change->setPropertyName("generator");
        change->setValue(QVariant::fromValue(generator));
        notifyObservers(change);
    }
}

/*!
    \class Qt3DRender::QAbstractTexture
    \inmodule Qt3DRender
    \since 5.5
    \brief A base class to be used to provide textures.

    The QAbstractTexture class shouldn't be used directly but rather
    through one of its subclasses. Each subclass implements a given texture
    target (2D, 2DArray, 3D, CubeMap ...) Each subclass provides a set of
    functors for each layer, cube map face and mipmap level. In turn the
    backend uses those functor to properly fill a corresponding OpenGL texture
    with data.
 */

/*!
    \enum Qt3DRender::QAbstractTexture::CubeMapFace

    This enum identifies the faces of a cube map texture
    \value CubeMapPositiveX     Specify the positive X face of a cube map
    \value CubeMapNegativeX     Specify the negative X face of a cube map
    \value CubeMapPositiveY     Specify the positive Y face of a cube map
    \value CubeMapNegativeY     Specify the negative Y face of a cube map
    \value CubeMapPositiveZ     Specify the positive Z face of a cube map
    \value CubeMapNegativeZ     Specify the negative Z face of a cube map
    \value AllFaces             Specify all the faces of a cube map

    \note AllFaces should only be used when a behavior needs to be applied to
    all the faces of a cubemap. This is the case for example when using a cube
    map as a texture attachment. Using AllFaces in the attachment specfication
    would result in all faces being bound to the attachment point. On the other
    hand, if a specific face is specified, the attachment would only be using
    the specified face.
*/

/*!
 * The constructor creates a new QAbstractTexture::QAbstractTexture
 * instance with the specified \a parent.
 */
QAbstractTexture::QAbstractTexture(QNode *parent)
    : QNode(*new QAbstractTexturePrivate, parent)
{
}

/*!
 * The constructor creates a new QAbstractTexture::QAbstractTexture
 * instance with the specified \a target and \a parent.
 */
QAbstractTexture::QAbstractTexture(Target target, QNode *parent)
    : QNode(*new QAbstractTexturePrivate, parent)
{
    d_func()->m_target = target;
}

/*! \internal */
QAbstractTexture::~QAbstractTexture()
{
}

/*! \internal */
QAbstractTexture::QAbstractTexture(QAbstractTexturePrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

/*!
    Sets the size of the texture provider to width \a w, height \a h and depth \a d.
 */
void QAbstractTexture::setSize(int w, int h, int d)
{
    setWidth(w);
    setHeight(h);
    setDepth(d);
}

/*!
    \property Qt3DRender::QAbstractTexture::width

    Holds the width of the texture provider.
 */
void QAbstractTexture::setWidth(int width)
{
    Q_D(QAbstractTexture);
    if (d->m_width != width) {
        d->m_width = width;
        emit widthChanged(width);
    }
}

/*!
    \property Qt3DRender::QAbstractTexture::height

    Holds the height of the texture provider.
 */
void QAbstractTexture::setHeight(int height)
{
    Q_D(QAbstractTexture);
    if (d->m_height != height) {
        d->m_height = height;
        emit heightChanged(height);
    }
}

/*!
    \property Qt3DRender::QAbstractTexture::depth

    Holds the depth of the texture provider.
 */
void QAbstractTexture::setDepth(int depth)
{
    Q_D(QAbstractTexture);
    if (d->m_depth != depth) {
        d->m_depth = depth;
        emit depthChanged(depth);
    }
}

/*!
 * \return the width of the texture
 */
int QAbstractTexture::width() const
{
    Q_D(const QAbstractTexture);
    return d->m_width;
}

/*!
 * \return the height of the texture
 */
int QAbstractTexture::height() const
{
    Q_D(const QAbstractTexture);
    return d->m_height;
}

/*!
 * \return the depth of the texture
 */
int QAbstractTexture::depth() const
{
    Q_D(const QAbstractTexture);
    return d->m_depth;
}

/*!
    \property Qt3DRender::QAbstractTexture::layers

    Holds the maximum layer count of the texture provider. By default, the
    maximum layer count is 1.

    \note this has a meaning only for texture providers that have 3D or
    array target formats.
 */
void QAbstractTexture::setLayers(int layers)
{
    Q_D(QAbstractTexture);
    if (d->m_layers != layers) {
        d->m_layers = layers;
        emit layersChanged(layers);
    }
}

/*!
    \return the maximum number of layers for the texture provider.

    \note this has a meaning only for texture providers that have 3D or
     array target formats.
 */
int QAbstractTexture::layers() const
{
    Q_D(const QAbstractTexture);
    return d->m_layers;
}

/*!
    \property Qt3DRender::QAbstractTexture::samples

    Holds the number of samples per texel for the texture provider.
    By default, the number of samples is 1.

    \note this has a meaning only for texture providers that have multisample
    formats.
 */
void QAbstractTexture::setSamples(int samples)
{
    Q_D(QAbstractTexture);
    if (d->m_samples != samples) {
        d->m_samples = samples;
        emit samplesChanged(samples);
    }
}

/*!
    \return the number of samples per texel for the texture provider.

    \note this has a meaning only for texture providers that have multisample
    formats.
 */
int QAbstractTexture::samples() const
{
    Q_D(const QAbstractTexture);
    return d->m_samples;
}

/*!
    \property Qt3DRender::QAbstractTexture::format

    Holds the format of the texture provider.
 */
void QAbstractTexture::setFormat(TextureFormat format)
{
    Q_D(QAbstractTexture);
    if (d->m_format != format) {
        d->m_format = format;
        emit formatChanged(format);
    }
}

/*!
    \return the texture provider's format.
 */
QAbstractTexture::TextureFormat QAbstractTexture::format() const
{
    Q_D(const QAbstractTexture);
    return d->m_format;
}

/*!
    \property Qt3DRender::QAbstractTexture::status readonly

    Holds the current status of the texture provider.
 */
void QAbstractTexture::setStatus(Status status)
{
    Q_D(QAbstractTexture);
    if (status != d->m_status) {
        d->m_status = status;
        emit statusChanged(status);
    }
}

/*!
 * \return the current status
 */
QAbstractTexture::Status QAbstractTexture::status() const
{
    Q_D(const QAbstractTexture);
    return d->m_status;
}

/*!
    \property Qt3DRender::QAbstractTexture::target readonly

    Holds the target format of the texture provider.

    \note The target format can only be set once.
 */
QAbstractTexture::Target QAbstractTexture::target() const
{
    Q_D(const QAbstractTexture);
    return d->m_target;
}

/*!
    Adds a new Qt3DCore::QAbstractTextureImage \a textureImage to the texture provider.

    \note Qt3DRender::QAbstractTextureImage should never be shared between multiple
    Qt3DRender::QAbstractTexture instances.
 */
void QAbstractTexture::addTextureImage(QAbstractTextureImage *textureImage)
{
    Q_ASSERT(textureImage);
    Q_D(QAbstractTexture);
    if (!d->m_textureImages.contains(textureImage)) {
        d->m_textureImages.append(textureImage);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(textureImage, &QAbstractTexture::removeTextureImage, d->m_textureImages);

        if (textureImage->parent() && textureImage->parent() != this)
            qWarning() << "A QAbstractTextureImage was shared, expect a crash, undefined behavior at best";
        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (!textureImage->parent())
            textureImage->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), textureImage);
            change->setPropertyName("textureImage");
            d->notifyObservers(change);
        }
    }
}

/*!
    Removes a Qt3DCore::QAbstractTextureImage \a textureImage from the texture provider.
 */
void QAbstractTexture::removeTextureImage(QAbstractTextureImage *textureImage)
{
    Q_ASSERT(textureImage);
    Q_D(QAbstractTexture);
    if (d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), textureImage);
        change->setPropertyName("textureImage");
        d->notifyObservers(change);
    }
    d->m_textureImages.removeOne(textureImage);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(textureImage);
}

/*!
    \return a list of pointers to QAbstractTextureImage objects contained in
    the texture provider.
 */
QVector<QAbstractTextureImage *> QAbstractTexture::textureImages() const
{
    Q_D(const QAbstractTexture);
    return d->m_textureImages;
}

/*!
    \property Qt3DRender::QAbstractTexture::generateMipMaps

    Holds whether the texture provider should auto generate mipmaps.
 */
void QAbstractTexture::setGenerateMipMaps(bool gen)
{
    Q_D(QAbstractTexture);
    if (d->m_autoMipMap != gen) {
        d->m_autoMipMap = gen;
        emit generateMipMapsChanged(gen);
    }
}

bool QAbstractTexture::generateMipMaps() const
{
    Q_D(const QAbstractTexture);
    return d->m_autoMipMap;
}

/*!
    \property Qt3DRender::QAbstractTexture::minificationFilter

    Holds the minification filter of the texture provider.
 */
void QAbstractTexture::setMinificationFilter(Filter f)
{
    Q_D(QAbstractTexture);
    if (d->m_minFilter != f) {
        d->m_minFilter = f;
        emit minificationFilterChanged(f);
    }
}

/*!
    \property Qt3DRender::QAbstractTexture::magnificationFilter

    Holds the magnification filter of the texture provider.
 */
void QAbstractTexture::setMagnificationFilter(Filter f)
{
    Q_D(QAbstractTexture);
    if (d->m_magFilter != f) {
        d->m_magFilter = f;
        emit magnificationFilterChanged(f);
    }
}

QAbstractTexture::Filter QAbstractTexture::minificationFilter() const
{
    Q_D(const QAbstractTexture);
    return d->m_minFilter;
}

QAbstractTexture::Filter QAbstractTexture::magnificationFilter() const
{
    Q_D(const QAbstractTexture);
    return d->m_magFilter;
}

/*!
    \property Qt3DRender::QAbstractTexture::wrapMode

    Holds the wrap mode of the texture provider.
 */
void QAbstractTexture::setWrapMode(const QTextureWrapMode &wrapMode)
{
    Q_D(QAbstractTexture);
    if (d->m_wrapMode.x() != wrapMode.x()) {
        d->m_wrapMode.setX(wrapMode.x());
        auto e = QPropertyUpdatedChangePtr::create(d->m_id);
        e->setPropertyName("wrapModeX");
        e->setValue(static_cast<int>(d->m_wrapMode.x()));
        d->notifyObservers(e);
    }
    if (d->m_wrapMode.y() != wrapMode.y()) {
        d->m_wrapMode.setY(wrapMode.y());
        auto e = QPropertyUpdatedChangePtr::create(d->m_id);
        e->setPropertyName("wrapModeY");
        e->setValue(static_cast<int>(d->m_wrapMode.y()));
        d->notifyObservers(e);
    }
    if (d->m_wrapMode.z() != wrapMode.z()) {
        d->m_wrapMode.setZ(wrapMode.z());
        auto e = QPropertyUpdatedChangePtr::create(d->m_id);
        e->setPropertyName("wrapModeZ");
        e->setValue(static_cast<int>(d->m_wrapMode.z()));
        d->notifyObservers(e);
    }
}

QTextureWrapMode *QAbstractTexture::wrapMode()
{
    Q_D(QAbstractTexture);
    return &d->m_wrapMode;
}

/*!
    \property Qt3DRender::QAbstractTexture::maximumAnisotropy

    Holds the maximum anisotropy of the texture provider.
 */
void QAbstractTexture::setMaximumAnisotropy(float anisotropy)
{
    Q_D(QAbstractTexture);
    if (!qFuzzyCompare(d->m_maximumAnisotropy, anisotropy)) {
        d->m_maximumAnisotropy = anisotropy;
        emit maximumAnisotropyChanged(anisotropy);
    }
}

/*!
 * \return the current maximum anisotropy
 */
float QAbstractTexture::maximumAnisotropy() const
{
    Q_D(const QAbstractTexture);
    return d->m_maximumAnisotropy;
}

/*!
    \property Qt3DRender::QAbstractTexture::comparisonFunction

    Holds the comparison function of the texture provider.
 */
void QAbstractTexture::setComparisonFunction(QAbstractTexture::ComparisonFunction function)
{
    Q_D(QAbstractTexture);
    if (d->m_comparisonFunction != function) {
        d->m_comparisonFunction = function;
        emit comparisonFunctionChanged(function);
    }
}

/*!
 * \return the current comparison function.
 */
QAbstractTexture::ComparisonFunction QAbstractTexture::comparisonFunction() const
{
    Q_D(const QAbstractTexture);
    return d->m_comparisonFunction;
}

/*!
    \property Qt3DRender::QAbstractTexture::comparisonMode

    Holds the comparison mode of the texture provider.
 */
void QAbstractTexture::setComparisonMode(QAbstractTexture::ComparisonMode mode)
{
    Q_D(QAbstractTexture);
    if (d->m_comparisonMode != mode) {
        d->m_comparisonMode = mode;
        emit comparisonModeChanged(mode);
    }
}

/*!
 * \return the current comparison mode.
 */
QAbstractTexture::ComparisonMode QAbstractTexture::comparisonMode() const
{
    Q_D(const QAbstractTexture);
    return d->m_comparisonMode;
}

/*!
 * \return the current data generator.
 */
QTextureGeneratorPtr QAbstractTexture::dataGenerator() const
{
    Q_D(const QAbstractTexture);
    return d->m_dataFunctor;
}

Qt3DCore::QNodeCreatedChangeBasePtr QAbstractTexture::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QAbstractTextureData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QAbstractTexture);
    data.target = d->m_target;
    data.format = d->m_format;
    data.width = d->m_width;
    data.height = d->m_height;
    data.depth = d->m_depth;
    data.autoMipMap = d->m_autoMipMap;
    data.minFilter = d->m_minFilter;
    data.magFilter = d->m_magFilter;
    data.wrapModeX = d->m_wrapMode.x();
    data.wrapModeY = d->m_wrapMode.y();
    data.wrapModeZ = d->m_wrapMode.z();
    data.maximumAnisotropy = d->m_maximumAnisotropy;
    data.comparisonFunction = d->m_comparisonFunction;
    data.comparisonMode = d->m_comparisonMode;
    data.textureImageIds = qIdsForNodes(d->m_textureImages);
    data.layers = d->m_layers;
    data.samples = d->m_samples;
    data.dataFunctor = d->m_dataFunctor;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
