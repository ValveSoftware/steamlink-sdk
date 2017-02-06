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

#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qquickitem_p.h>
#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgtexture.h>
#include <QFile>
#include "qquickimageparticle_p.h"
#include "qquickparticleemitter_p.h"
#include <private/qquicksprite_p.h>
#include <private/qquickspriteengine_p.h>
#include <QOpenGLFunctions>
#include <QSGRendererInterface>
#include <QtQuick/private/qsgshadersourcebuilder_p.h>
#include <QtQuick/private/qsgtexture_p.h>
#include <private/qqmlglobal_p.h>
#include <QtQml/qqmlinfo.h>
#include <cmath>

QT_BEGIN_NAMESPACE

#if defined(Q_OS_BLACKBERRY)
#define SHADER_PLATFORM_DEFINES "Q_OS_BLACKBERRY\n"
#else
#define SHADER_PLATFORM_DEFINES
#endif

//TODO: Make it larger on desktop? Requires fixing up shader code with the same define
#define UNIFORM_ARRAY_SIZE 64

const qreal CONV = 0.017453292519943295;
class ImageMaterialData
{
    public:
    ImageMaterialData()
        : texture(0), colorTable(0)
    {}

    ~ImageMaterialData(){
        delete texture;
        delete colorTable;
    }

    QSGTexture *texture;
    QSGTexture *colorTable;
    float sizeTable[UNIFORM_ARRAY_SIZE];
    float opacityTable[UNIFORM_ARRAY_SIZE];

    qreal timestamp;
    qreal entry;
    QSizeF animSheetSize;
};

class TabledMaterialData : public ImageMaterialData {};
class TabledMaterial : public QSGSimpleMaterialShader<TabledMaterialData>
{
    QSG_DECLARE_SIMPLE_SHADER(TabledMaterial, TabledMaterialData)

public:
    TabledMaterial()
    {
        QSGShaderSourceBuilder builder;
        const bool isES = QOpenGLContext::currentContext()->isOpenGLES();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.vert"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("TABLE"));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_vertex_code = builder.source();
        builder.clear();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.frag"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("TABLE"));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_fragment_code = builder.source();

        Q_ASSERT(!m_vertex_code.isNull());
        Q_ASSERT(!m_fragment_code.isNull());
    }

    const char *vertexShader() const { return m_vertex_code.constData(); }
    const char *fragmentShader() const { return m_fragment_code.constData(); }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "vPosTex" << "vData" << "vVec"
            << "vColor" << "vDeformVec" << "vRotation";
    };

    void initialize() {
        QSGSimpleMaterialShader<TabledMaterialData>::initialize();
        program()->bind();
        program()->setUniformValue("_qt_texture", 0);
        program()->setUniformValue("colortable", 1);
        glFuncs = QOpenGLContext::currentContext()->functions();
        m_timestamp_id = program()->uniformLocation("timestamp");
        m_entry_id = program()->uniformLocation("entry");
        m_sizetable_id = program()->uniformLocation("sizetable");
        m_opacitytable_id = program()->uniformLocation("opacitytable");
    }

    void updateState(const TabledMaterialData* d, const TabledMaterialData*) {
        glFuncs->glActiveTexture(GL_TEXTURE1);
        d->colorTable->bind();

        glFuncs->glActiveTexture(GL_TEXTURE0);
        d->texture->bind();

        program()->setUniformValue(m_timestamp_id, (float) d->timestamp);
        program()->setUniformValue(m_entry_id, (float) d->entry);
        program()->setUniformValueArray(m_sizetable_id, (const float*) d->sizeTable, UNIFORM_ARRAY_SIZE, 1);
        program()->setUniformValueArray(m_opacitytable_id, (const float*) d->opacityTable, UNIFORM_ARRAY_SIZE, 1);
    }

    int m_entry_id;
    int m_timestamp_id;
    int m_sizetable_id;
    int m_opacitytable_id;
    QByteArray m_vertex_code;
    QByteArray m_fragment_code;
    QOpenGLFunctions* glFuncs;
};

class DeformableMaterialData : public ImageMaterialData {};
class DeformableMaterial : public QSGSimpleMaterialShader<DeformableMaterialData>
{
    QSG_DECLARE_SIMPLE_SHADER(DeformableMaterial, DeformableMaterialData)

public:
    DeformableMaterial()
    {
        QSGShaderSourceBuilder builder;
        const bool isES = QOpenGLContext::currentContext()->isOpenGLES();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.vert"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_vertex_code = builder.source();
        builder.clear();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.frag"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_fragment_code = builder.source();

        Q_ASSERT(!m_vertex_code.isNull());
        Q_ASSERT(!m_fragment_code.isNull());
    }

    const char *vertexShader() const { return m_vertex_code.constData(); }
    const char *fragmentShader() const { return m_fragment_code.constData(); }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "vPosTex" << "vData" << "vVec"
            << "vColor" << "vDeformVec" << "vRotation";
    };

    void initialize() {
        QSGSimpleMaterialShader<DeformableMaterialData>::initialize();
        program()->bind();
        program()->setUniformValue("_qt_texture", 0);
        glFuncs = QOpenGLContext::currentContext()->functions();
        m_timestamp_id = program()->uniformLocation("timestamp");
        m_entry_id = program()->uniformLocation("entry");
    }

    void updateState(const DeformableMaterialData* d, const DeformableMaterialData*) {
        d->texture->bind();

        program()->setUniformValue(m_timestamp_id, (float) d->timestamp);
        program()->setUniformValue(m_entry_id, (float) d->entry);
    }

    int m_entry_id;
    int m_timestamp_id;
    QByteArray m_vertex_code;
    QByteArray m_fragment_code;
    QOpenGLFunctions* glFuncs;
};

class SpriteMaterialData : public ImageMaterialData {};
class SpriteMaterial : public QSGSimpleMaterialShader<SpriteMaterialData>
{
    QSG_DECLARE_SIMPLE_SHADER(SpriteMaterial, SpriteMaterialData)

public:
    SpriteMaterial()
    {
        QSGShaderSourceBuilder builder;
        const bool isES = QOpenGLContext::currentContext()->isOpenGLES();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.vert"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("SPRITE"));
        builder.addDefinition(QByteArrayLiteral("TABLE"));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_vertex_code = builder.source();
        builder.clear();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.frag"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("SPRITE"));
        builder.addDefinition(QByteArrayLiteral("TABLE"));
        builder.addDefinition(QByteArrayLiteral("DEFORM"));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_fragment_code = builder.source();

        Q_ASSERT(!m_vertex_code.isNull());
        Q_ASSERT(!m_fragment_code.isNull());
    }

    const char *vertexShader() const { return m_vertex_code.constData(); }
    const char *fragmentShader() const { return m_fragment_code.constData(); }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "vPosTex" << "vData" << "vVec"
            << "vColor" << "vDeformVec" << "vRotation" << "vAnimData" << "vAnimPos";
    }

    void initialize() {
        QSGSimpleMaterialShader<SpriteMaterialData>::initialize();
        program()->bind();
        program()->setUniformValue("_qt_texture", 0);
        program()->setUniformValue("colortable", 1);
        glFuncs = QOpenGLContext::currentContext()->functions();
        //Don't actually expose the animSheetSize in the shader, it's currently only used for CPU calculations.
        m_timestamp_id = program()->uniformLocation("timestamp");
        m_entry_id = program()->uniformLocation("entry");
        m_sizetable_id = program()->uniformLocation("sizetable");
        m_opacitytable_id = program()->uniformLocation("opacitytable");
    }

    void updateState(const SpriteMaterialData* d, const SpriteMaterialData*) {
        glFuncs->glActiveTexture(GL_TEXTURE1);
        d->colorTable->bind();

        // make sure we end by setting GL_TEXTURE0 as active texture
        glFuncs->glActiveTexture(GL_TEXTURE0);
        d->texture->bind();

        program()->setUniformValue(m_timestamp_id, (float) d->timestamp);
        program()->setUniformValue(m_entry_id, (float) d->entry);
        program()->setUniformValueArray(m_sizetable_id, (const float*) d->sizeTable, 64, 1);
        program()->setUniformValueArray(m_opacitytable_id, (const float*) d->opacityTable, UNIFORM_ARRAY_SIZE, 1);
    }

    int m_timestamp_id;
    int m_entry_id;
    int m_sizetable_id;
    int m_opacitytable_id;
    QByteArray m_vertex_code;
    QByteArray m_fragment_code;
    QOpenGLFunctions* glFuncs;
};

class ColoredMaterialData : public ImageMaterialData {};
class ColoredMaterial : public QSGSimpleMaterialShader<ColoredMaterialData>
{
    QSG_DECLARE_SIMPLE_SHADER(ColoredMaterial, ColoredMaterialData)

public:
    ColoredMaterial()
    {
        QSGShaderSourceBuilder builder;
        const bool isES = QOpenGLContext::currentContext()->isOpenGLES();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.vert"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_vertex_code = builder.source();
        builder.clear();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.frag"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        builder.addDefinition(QByteArrayLiteral("COLOR"));
        if (isES)
            builder.removeVersion();

        m_fragment_code = builder.source();

        Q_ASSERT(!m_vertex_code.isNull());
        Q_ASSERT(!m_fragment_code.isNull());
    }

    const char *vertexShader() const { return m_vertex_code.constData(); }
    const char *fragmentShader() const { return m_fragment_code.constData(); }

    void activate() {
        QSGSimpleMaterialShader<ColoredMaterialData>::activate();
#if !defined(QT_OPENGL_ES_2) && !defined(Q_OS_WIN)
        glEnable(GL_POINT_SPRITE);
        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
    }

    void deactivate() {
        QSGSimpleMaterialShader<ColoredMaterialData>::deactivate();
#if !defined(QT_OPENGL_ES_2) && !defined(Q_OS_WIN)
        glDisable(GL_POINT_SPRITE);
        glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
    }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "vPos" << "vData" << "vVec" << "vColor";
    }

    void initialize() {
        QSGSimpleMaterialShader<ColoredMaterialData>::initialize();
        program()->bind();
        program()->setUniformValue("_qt_texture", 0);
        glFuncs = QOpenGLContext::currentContext()->functions();
        m_timestamp_id = program()->uniformLocation("timestamp");
        m_entry_id = program()->uniformLocation("entry");
    }

    void updateState(const ColoredMaterialData* d, const ColoredMaterialData*) {
        d->texture->bind();

        program()->setUniformValue(m_timestamp_id, (float) d->timestamp);
        program()->setUniformValue(m_entry_id, (float) d->entry);
    }

    int m_timestamp_id;
    int m_entry_id;
    QByteArray m_vertex_code;
    QByteArray m_fragment_code;
    QOpenGLFunctions* glFuncs;
};

class SimpleMaterialData : public ImageMaterialData {};
class SimpleMaterial : public QSGSimpleMaterialShader<SimpleMaterialData>
{
    QSG_DECLARE_SIMPLE_SHADER(SimpleMaterial, SimpleMaterialData)

public:
    SimpleMaterial()
    {
        QSGShaderSourceBuilder builder;
        const bool isES = QOpenGLContext::currentContext()->isOpenGLES();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.vert"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        if (isES)
            builder.removeVersion();

        m_vertex_code = builder.source();
        builder.clear();

        builder.appendSourceFile(QStringLiteral(":/particles/shaders/imageparticle.frag"));
        builder.addDefinition(QByteArray(SHADER_PLATFORM_DEFINES));
        if (isES)
            builder.removeVersion();

        m_fragment_code = builder.source();

        Q_ASSERT(!m_vertex_code.isNull());
        Q_ASSERT(!m_fragment_code.isNull());
    }

    const char *vertexShader() const { return m_vertex_code.constData(); }
    const char *fragmentShader() const { return m_fragment_code.constData(); }

    void activate() {
        QSGSimpleMaterialShader<SimpleMaterialData>::activate();
#if !defined(QT_OPENGL_ES_2) && !defined(Q_OS_WIN)
        glEnable(GL_POINT_SPRITE);
        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
    }

    void deactivate() {
        QSGSimpleMaterialShader<SimpleMaterialData>::deactivate();
#if !defined(QT_OPENGL_ES_2) && !defined(Q_OS_WIN)
        glDisable(GL_POINT_SPRITE);
        glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif
    }

    QList<QByteArray> attributes() const {
        return QList<QByteArray>() << "vPos" << "vData" << "vVec";
    }

    void initialize() {
        QSGSimpleMaterialShader<SimpleMaterialData>::initialize();
        program()->bind();
        program()->setUniformValue("_qt_texture", 0);
        glFuncs = QOpenGLContext::currentContext()->functions();
        m_timestamp_id = program()->uniformLocation("timestamp");
        m_entry_id = program()->uniformLocation("entry");
    }

    void updateState(const SimpleMaterialData* d, const SimpleMaterialData*) {
        d->texture->bind();

        program()->setUniformValue(m_timestamp_id, (float) d->timestamp);
        program()->setUniformValue(m_entry_id, (float) d->entry);
    }

    int m_timestamp_id;
    int m_entry_id;
    QByteArray m_vertex_code;
    QByteArray m_fragment_code;
    QOpenGLFunctions* glFuncs;
};

void fillUniformArrayFromImage(float* array, const QImage& img, int size)
{
    if (img.isNull()){
        for (int i=0; i<size; i++)
            array[i] = 1.0;
        return;
    }
    QImage scaled = img.scaled(size,1);
    for (int i=0; i<size; i++)
        array[i] = qAlpha(scaled.pixel(i,0))/255.0;
}

/*!
    \qmltype ImageParticle
    \instantiates QQuickImageParticle
    \inqmlmodule QtQuick.Particles
    \inherits ParticlePainter
    \brief For visualizing logical particles using an image
    \ingroup qtquick-particles

    This element renders a logical particle as an image. The image can be
    \list
    \li colorized
    \li rotated
    \li deformed
    \li a sprite-based animation
    \endlist

    ImageParticles implictly share data on particles if multiple ImageParticles are painting
    the same logical particle group. This is broken down along the four capabilities listed
    above. So if one ImageParticle defines data for rendering the particles in one of those
    capabilities, and the other does not, then both will draw the particles the same in that
    aspect automatically. This is primarily useful when there is some random variation on
    the particle which is supposed to stay with it when switching painters. If both ImageParticles
    define how they should appear for that aspect, they diverge and each appears as it is defined.

    This sharing of data happens behind the scenes based off of whether properties were implicitly or explicitly
    set. One drawback of the current implementation is that it is only possible to reset the capabilities as a whole.
    So if you explicitly set an attribute affecting color, such as redVariation, and then reset it (by setting redVariation
    to undefined), all color data will be reset and it will begin to have an implicit value of any shared color from
    other ImageParticles.
*/
/*!
    \qmlproperty url QtQuick.Particles::ImageParticle::source

    The source image to be used.

    If the image is a sprite animation, use the sprite property instead.

    Since Qt 5.2, some default images are provided as resources to aid prototyping:
    \table
    \row
    \li qrc:///particleresources/star.png
    \li \inlineimage particles/star.png
    \row
    \li qrc:///particleresources/glowdot.png
    \li \inlineimage particles/glowdot.png
    \row
    \li qrc:///particleresources/fuzzydot.png
    \li \inlineimage particles/fuzzydot.png
    \endtable

    Note that the images are white and semi-transparent, to allow colorization
    and alpha levels to have maximum effect.
*/
/*!
    \qmlproperty list<Sprite> QtQuick.Particles::ImageParticle::sprites

    The sprite or sprites used to draw this particle.

    Note that the sprite image will be scaled to a square based on the size of
    the particle being rendered.

    For full details, see the \l{Sprite Animations} overview.
*/
/*!
    \qmlproperty url QtQuick.Particles::ImageParticle::colorTable

    An image whose color will be used as a 1D texture to determine color over life. E.g. when
    the particle is halfway through its lifetime, it will have the color specified halfway
    across the image.

    This color is blended with the color property and the color of the source image.
*/
/*!
    \qmlproperty url QtQuick.Particles::ImageParticle::sizeTable

    An image whose opacity will be used as a 1D texture to determine size over life.

    This property is expected to be removed shortly, in favor of custom easing curves to determine size over life.
*/
/*!
    \qmlproperty url QtQuick.Particles::ImageParticle::opacityTable

    An image whose opacity will be used as a 1D texture to determine size over life.

    This property is expected to be removed shortly, in favor of custom easing curves to determine opacity over life.
*/
/*!
    \qmlproperty color QtQuick.Particles::ImageParticle::color

    If a color is specified, the provided image will be colorized with it.

    Default is white (no change).
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::colorVariation

    This number represents the color variation applied to individual particles.
    Setting colorVariation is the same as setting redVariation, greenVariation,
    and blueVariation to the same number.

    Each channel can vary between particle by up to colorVariation from its usual color.

    Color is measured, per channel, from 0.0 to 1.0.

    Default is 0.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::redVariation
    The variation in the red color channel between particles.

    Color is measured, per channel, from 0.0 to 1.0.

    Default is 0.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::greenVariation
    The variation in the green color channel between particles.

    Color is measured, per channel, from 0.0 to 1.0.

    Default is 0.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::blueVariation
    The variation in the blue color channel between particles.

    Color is measured, per channel, from 0.0 to 1.0.

    Default is 0.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::alpha
    An alpha to be applied to the image. This value is multiplied by the value in
    the image, and the value in the color property.

    Particles have additive blending, so lower alpha on single particles leads
    to stronger effects when multiple particles overlap.

    Alpha is measured from 0.0 to 1.0.

    Default is 1.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::alphaVariation
    The variation in the alpha channel between particles.

    Alpha is measured from 0.0 to 1.0.

    Default is 0.0
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::rotation

    If set the image will be rotated by this many degrees before it is drawn.

    The particle coordinates are not transformed.
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::rotationVariation

    If set the rotation of individual particles will vary by up to this much
    between particles.

*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::rotationVelocity

    If set particles will rotate at this velocity in degrees/second.
*/
/*!
    \qmlproperty real QtQuick.Particles::ImageParticle::rotationVelocityVariation

    If set the rotationVelocity of individual particles will vary by up to this much
    between particles.

*/
/*!
    \qmlproperty bool QtQuick.Particles::ImageParticle::autoRotation

    If set to true then a rotation will be applied on top of the particles rotation, so
    that it faces the direction of travel. So to face away from the direction of travel,
    set autoRotation to true and rotation to 180.

    Default is false
*/
/*!
    \qmlproperty StochasticDirection QtQuick.Particles::ImageParticle::xVector

    Allows you to deform the particle image when drawn. The rectangular image will
    be deformed so that the horizontal sides are in the shape of this vector instead
    of (1,0).
*/
/*!
    \qmlproperty StochasticDirection QtQuick.Particles::ImageParticle::yVector

    Allows you to deform the particle image when drawn. The rectangular image will
    be deformed so that the vertical sides are in the shape of this vector instead
    of (0,1).
*/
/*!
    \qmlproperty EntryEffect QtQuick.Particles::ImageParticle::entryEffect

    This property provides basic and cheap entrance and exit effects for the particles.
    For fine-grained control, see sizeTable and opacityTable.

    Acceptable values are
    \list
    \li ImageParticle.None: Particles just appear and disappear.
    \li ImageParticle.Fade: Particles fade in from 0 opacity at the start of their life, and fade out to 0 at the end.
    \li ImageParticle.Scale: Particles scale in from 0 size at the start of their life, and scale back to 0 at the end.
    \endlist

    Default value is Fade.
*/
/*!
    \qmlproperty bool QtQuick.Particles::ImageParticle::spritesInterpolate

    If set to true, sprite particles will interpolate between sprite frames each rendered frame, making
    the sprites look smoother.

    Default is true.
*/

/*!
    \qmlproperty Status QtQuick.Particles::ImageParticle::status

    The status of loading the image.
*/


QQuickImageParticle::QQuickImageParticle(QQuickItem* parent)
    : QQuickParticlePainter(parent)
    , m_color_variation(0.0)
    , m_material(0)
    , m_alphaVariation(0.0)
    , m_alpha(1.0)
    , m_redVariation(0.0)
    , m_greenVariation(0.0)
    , m_blueVariation(0.0)
    , m_rotation(0)
    , m_rotationVariation(0)
    , m_rotationVelocity(0)
    , m_rotationVelocityVariation(0)
    , m_autoRotation(false)
    , m_xVector(0)
    , m_yVector(0)
    , m_spriteEngine(0)
    , m_spritesInterpolate(true)
    , m_explicitColor(false)
    , m_explicitRotation(false)
    , m_explicitDeformation(false)
    , m_explicitAnimation(false)
    , m_bypassOptimizations(false)
    , perfLevel(Unknown)
    , m_lastLevel(Unknown)
    , m_debugMode(false)
    , m_entryEffect(Fade)
    , m_startedImageLoading(0)
{
    setFlag(ItemHasContents);
}

QQuickImageParticle::~QQuickImageParticle()
{
    clearShadows();
}

QQmlListProperty<QQuickSprite> QQuickImageParticle::sprites()
{
    return QQmlListProperty<QQuickSprite>(this, &m_sprites, spriteAppend, spriteCount, spriteAt, spriteClear);
}

void QQuickImageParticle::sceneGraphInvalidated()
{
    m_nodes.clear();
    m_material = 0;
}

void QQuickImageParticle::setImage(const QUrl &image)
{
    if (image.isEmpty()){
        if (m_image) {
            m_image.reset();
            emit imageChanged();
        }
        return;
    }

    if (!m_image)
        m_image.reset(new ImageData);
    if (image == m_image->source)
        return;
    m_image->source = image;
    emit imageChanged();
    reset();
}


void QQuickImageParticle::setColortable(const QUrl &table)
{
    if (table.isEmpty()){
        if (m_colorTable) {
            m_colorTable.reset();
            emit colortableChanged();
        }
        return;
    }

    if (!m_colorTable)
        m_colorTable.reset(new ImageData);
    if (table == m_colorTable->source)
        return;
    m_colorTable->source = table;
    emit colortableChanged();
    reset();
}

void QQuickImageParticle::setSizetable(const QUrl &table)
{
    if (table.isEmpty()){
        if (m_sizeTable) {
            m_sizeTable.reset();
            emit sizetableChanged();
        }
        return;
    }

    if (!m_sizeTable)
        m_sizeTable.reset(new ImageData);
    if (table == m_sizeTable->source)
        return;
    m_sizeTable->source = table;
    emit sizetableChanged();
    reset();
}

void QQuickImageParticle::setOpacitytable(const QUrl &table)
{
    if (table.isEmpty()){
        if (m_opacityTable) {
            m_opacityTable.reset();
            emit opacitytableChanged();
        }
        return;
    }

    if (!m_opacityTable)
        m_opacityTable.reset(new ImageData);
    if (table == m_opacityTable->source)
        return;
    m_opacityTable->source = table;
    emit opacitytableChanged();
    reset();
}

void QQuickImageParticle::setColor(const QColor &color)
{
    if (color == m_color)
        return;
    m_color = color;
    emit colorChanged();
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setColorVariation(qreal var)
{
    if (var == m_color_variation)
        return;
    m_color_variation = var;
    emit colorVariationChanged();
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setAlphaVariation(qreal arg)
{
    if (m_alphaVariation != arg) {
        m_alphaVariation = arg;
        emit alphaVariationChanged(arg);
    }
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setAlpha(qreal arg)
{
    if (m_alpha != arg) {
        m_alpha = arg;
        emit alphaChanged(arg);
    }
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setRedVariation(qreal arg)
{
    if (m_redVariation != arg) {
        m_redVariation = arg;
        emit redVariationChanged(arg);
    }
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setGreenVariation(qreal arg)
{
    if (m_greenVariation != arg) {
        m_greenVariation = arg;
        emit greenVariationChanged(arg);
    }
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setBlueVariation(qreal arg)
{
    if (m_blueVariation != arg) {
        m_blueVariation = arg;
        emit blueVariationChanged(arg);
    }
    m_explicitColor = true;
    if (perfLevel < Colored)
        reset();
}

void QQuickImageParticle::setRotation(qreal arg)
{
    if (m_rotation != arg) {
        m_rotation = arg;
        emit rotationChanged(arg);
    }
    m_explicitRotation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setRotationVariation(qreal arg)
{
    if (m_rotationVariation != arg) {
        m_rotationVariation = arg;
        emit rotationVariationChanged(arg);
    }
    m_explicitRotation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setRotationVelocity(qreal arg)
{
    if (m_rotationVelocity != arg) {
        m_rotationVelocity = arg;
        emit rotationVelocityChanged(arg);
    }
    m_explicitRotation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setRotationVelocityVariation(qreal arg)
{
    if (m_rotationVelocityVariation != arg) {
        m_rotationVelocityVariation = arg;
        emit rotationVelocityVariationChanged(arg);
    }
    m_explicitRotation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setAutoRotation(bool arg)
{
    if (m_autoRotation != arg) {
        m_autoRotation = arg;
        emit autoRotationChanged(arg);
    }
    m_explicitRotation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setXVector(QQuickDirection* arg)
{
    if (m_xVector != arg) {
        m_xVector = arg;
        emit xVectorChanged(arg);
    }
    m_explicitDeformation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setYVector(QQuickDirection* arg)
{
    if (m_yVector != arg) {
        m_yVector = arg;
        emit yVectorChanged(arg);
    }
    m_explicitDeformation = true;
    if (perfLevel < Deformable)
        reset();
}

void QQuickImageParticle::setSpritesInterpolate(bool arg)
{
    if (m_spritesInterpolate != arg) {
        m_spritesInterpolate = arg;
        emit spritesInterpolateChanged(arg);
    }
}

void QQuickImageParticle::setBypassOptimizations(bool arg)
{
    if (m_bypassOptimizations != arg) {
        m_bypassOptimizations = arg;
        emit bypassOptimizationsChanged(arg);
    }
    // Applies regardless of perfLevel
    reset();
}

void QQuickImageParticle::setEntryEffect(EntryEffect arg)
{
    if (m_entryEffect != arg) {
        m_entryEffect = arg;
        if (m_material)
            getState<ImageMaterialData>(m_material)->entry = (qreal) m_entryEffect;
        emit entryEffectChanged(arg);
    }
}

void QQuickImageParticle::resetColor()
{
    m_explicitColor = false;
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* d : qAsConst(m_system->groupData[groupId]->data)) {
            if (d->colorOwner == this) {
                d->colorOwner = 0;
            }
        }
    }
    m_color = QColor();
    m_color_variation = 0.0f;
    m_redVariation = 0.0f;
    m_blueVariation = 0.0f;
    m_greenVariation = 0.0f;
    m_alpha = 1.0f;
    m_alphaVariation = 0.0f;
}

void QQuickImageParticle::resetRotation()
{
    m_explicitRotation = false;
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* d : qAsConst(m_system->groupData[groupId]->data)) {
            if (d->rotationOwner == this) {
                d->rotationOwner = 0;
            }
        }
    }
    m_rotation = 0;
    m_rotationVariation = 0;
    m_rotationVelocity = 0;
    m_rotationVelocityVariation = 0;
    m_autoRotation = false;
}

void QQuickImageParticle::resetDeformation()
{
    m_explicitDeformation = false;
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* d : qAsConst(m_system->groupData[groupId]->data)) {
            if (d->deformationOwner == this) {
                d->deformationOwner = 0;
            }
        }
    }
    if (m_xVector)
        delete m_xVector;
    if (m_yVector)
        delete m_yVector;
    m_xVector = 0;
    m_yVector = 0;
}

void QQuickImageParticle::reset()
{
    QQuickParticlePainter::reset();
    m_pleaseReset = true;
    update();
}

void QQuickImageParticle::createEngine()
{
    if (m_spriteEngine)
        delete m_spriteEngine;
    if (m_sprites.count()) {
        m_spriteEngine = new QQuickSpriteEngine(m_sprites, this);
        connect(m_spriteEngine, SIGNAL(stateChanged(int)),
                this, SLOT(spriteAdvance(int)), Qt::DirectConnection);
        m_explicitAnimation = true;
    } else {
        m_spriteEngine = 0;
        m_explicitAnimation = false;
    }
    reset();
}

static QSGGeometry::Attribute SimpleParticle_Attributes[] = {
    QSGGeometry::Attribute::create(0, 2, GL_FLOAT, true),             // Position
    QSGGeometry::Attribute::create(1, 4, GL_FLOAT),             // Data
    QSGGeometry::Attribute::create(2, 4, GL_FLOAT)             // Vectors
};

static QSGGeometry::AttributeSet SimpleParticle_AttributeSet =
{
    3, // Attribute Count
    ( 2 + 4 + 4 ) * sizeof(float),
    SimpleParticle_Attributes
};

static QSGGeometry::Attribute ColoredParticle_Attributes[] = {
    QSGGeometry::Attribute::create(0, 2, GL_FLOAT, true),             // Position
    QSGGeometry::Attribute::create(1, 4, GL_FLOAT),             // Data
    QSGGeometry::Attribute::create(2, 4, GL_FLOAT),             // Vectors
    QSGGeometry::Attribute::create(3, 4, GL_UNSIGNED_BYTE),     // Colors
};

static QSGGeometry::AttributeSet ColoredParticle_AttributeSet =
{
    4, // Attribute Count
    ( 2 + 4 + 4 ) * sizeof(float) + 4 * sizeof(uchar),
    ColoredParticle_Attributes
};

static QSGGeometry::Attribute DeformableParticle_Attributes[] = {
    QSGGeometry::Attribute::create(0, 4, GL_FLOAT),             // Position & TexCoord
    QSGGeometry::Attribute::create(1, 4, GL_FLOAT),             // Data
    QSGGeometry::Attribute::create(2, 4, GL_FLOAT),             // Vectors
    QSGGeometry::Attribute::create(3, 4, GL_UNSIGNED_BYTE),     // Colors
    QSGGeometry::Attribute::create(4, 4, GL_FLOAT),             // DeformationVectors
    QSGGeometry::Attribute::create(5, 3, GL_FLOAT),             // Rotation
};

static QSGGeometry::AttributeSet DeformableParticle_AttributeSet =
{
    6, // Attribute Count
    (4 + 4 + 4 + 4 + 3) * sizeof(float) + 4 * sizeof(uchar),
    DeformableParticle_Attributes
};

static QSGGeometry::Attribute SpriteParticle_Attributes[] = {
    QSGGeometry::Attribute::create(0, 4, GL_FLOAT),       // Position & TexCoord
    QSGGeometry::Attribute::create(1, 4, GL_FLOAT),             // Data
    QSGGeometry::Attribute::create(2, 4, GL_FLOAT),             // Vectors
    QSGGeometry::Attribute::create(3, 4, GL_UNSIGNED_BYTE),     // Colors
    QSGGeometry::Attribute::create(4, 4, GL_FLOAT),             // DeformationVectors
    QSGGeometry::Attribute::create(5, 3, GL_FLOAT),             // Rotation
    QSGGeometry::Attribute::create(6, 3, GL_FLOAT),             // Anim Data
    QSGGeometry::Attribute::create(7, 4, GL_FLOAT)              // Anim Pos
};

static QSGGeometry::AttributeSet SpriteParticle_AttributeSet =
{
    8, // Attribute Count
    (4 + 4 + 4 + 4 + 3 + 3 + 4) * sizeof(float) + 4 * sizeof(uchar),
    SpriteParticle_Attributes
};

void QQuickImageParticle::clearShadows()
{
    foreach (const QVector<QQuickParticleData*> data, m_shadowData)
        qDeleteAll(data);
    m_shadowData.clear();
}

//Only call if you need to, may initialize the whole array first time
QQuickParticleData* QQuickImageParticle::getShadowDatum(QQuickParticleData* datum)
{
    //Will return datum if the datum is a sentinel or uninitialized, to centralize that one check
    if (datum->systemIndex == -1)
        return datum;
    QQuickParticleGroupData* gd = m_system->groupData[datum->groupId];
    if (!m_shadowData.contains(datum->groupId)) {
        QVector<QQuickParticleData*> data;
        const int gdSize = gd->size();
        data.reserve(gdSize);
        for (int i = 0; i < gdSize; i++) {
            QQuickParticleData* datum = new QQuickParticleData;
            *datum = *(gd->data[i]);
            data << datum;
        }
        m_shadowData.insert(datum->groupId, data);
    }
    //### If dynamic resize is added, remember to potentially resize the shadow data on out-of-bounds access request

    return m_shadowData[datum->groupId][datum->index];
}

bool QQuickImageParticle::loadingSomething()
{
    return (m_image && m_image->pix.isLoading())
        || (m_colorTable && m_colorTable->pix.isLoading())
        || (m_sizeTable && m_sizeTable->pix.isLoading())
        || (m_opacityTable && m_opacityTable->pix.isLoading())
        || (m_spriteEngine && m_spriteEngine->isLoading());
}

void QQuickImageParticle::mainThreadFetchImageData()
{
    if (m_image) {//ImageData created on setSource
        m_image->pix.clear(this);
        m_image->pix.load(qmlEngine(this), m_image->source);
    }

    if (m_spriteEngine)
        m_spriteEngine->startAssemblingImage();

    if (m_colorTable)
        m_colorTable->pix.load(qmlEngine(this), m_colorTable->source);

    if (m_sizeTable)
        m_sizeTable->pix.load(qmlEngine(this), m_sizeTable->source);

    if (m_opacityTable)
        m_opacityTable->pix.load(qmlEngine(this), m_opacityTable->source);

    m_startedImageLoading = 2;
}

void QQuickImageParticle::buildParticleNodes(QSGNode** passThrough)
{
    // Starts async parts, like loading images, on gui thread
    // Not on individual properties, because we delay until system is running
    if (*passThrough || loadingSomething())
        return;

    if (m_startedImageLoading == 0) {
        m_startedImageLoading = 1;
        //stage 1 is in gui thread
        QQuickImageParticle::staticMetaObject.invokeMethod(this, "mainThreadFetchImageData", Qt::QueuedConnection);
    } else if (m_startedImageLoading == 2) {
        finishBuildParticleNodes(passThrough); //rest happens in render thread
    }

    //No mutex, because it's slow and a compare that fails due to a race condition means just a dropped frame
}

void QQuickImageParticle::finishBuildParticleNodes(QSGNode** node)
{
    if (!QOpenGLContext::currentContext())
        return;

    if (QOpenGLContext::currentContext()->isOpenGLES() && m_count * 4 > 0xffff) {
        printf("ImageParticle: Too many particles - maximum 16,000 per ImageParticle.\n");//ES 2 vertex count limit is ushort
        return;
    }

    if (count() <= 0)
        return;

    m_debugMode = m_system->m_debugMode;

    if (m_sprites.count() || m_bypassOptimizations) {
        perfLevel = Sprites;
    } else if (m_colorTable || m_sizeTable || m_opacityTable) {
        perfLevel = Tabled;
    } else if (m_autoRotation || m_rotation || m_rotationVariation
               || m_rotationVelocity || m_rotationVelocityVariation
               || m_xVector || m_yVector) {
        perfLevel = Deformable;
    } else if (m_alphaVariation || m_alpha != 1.0 || m_color.isValid() || m_color_variation
               || m_redVariation || m_blueVariation || m_greenVariation) {
        perfLevel = Colored;
    } else {
        perfLevel = Simple;
    }

    for (auto groupId : groupIds()) {
        //For sharing higher levels, need to have highest used so it renders
        for (QQuickParticlePainter* p : qAsConst(m_system->groupData[groupId]->painters)) {
            QQuickImageParticle* other = qobject_cast<QQuickImageParticle*>(p);
            if (other){
                if (other->perfLevel > perfLevel) {
                    if (other->perfLevel >= Tabled){//Deformable is the highest level needed for this, anything higher isn't shared (or requires your own sprite)
                        if (perfLevel < Deformable)
                            perfLevel = Deformable;
                    } else {
                        perfLevel = other->perfLevel;
                    }
                } else if (other->perfLevel < perfLevel) {
                    other->reset();
                }
            }
        }
    }
#ifdef Q_OS_WIN
    if (perfLevel < Deformable) //QTBUG-24540 , point sprite 'extension' isn't working on windows.
        perfLevel = Deformable;
#endif

#ifdef Q_OS_MAC
    // OS X 10.8.3 introduced a bug in the AMD drivers, for at least the 2011 macbook pros,
    // causing point sprites who read gl_PointCoord in the frag shader to come out as
    // green-red blobs.
    const GLubyte *glVendor = QOpenGLContext::currentContext()->functions()->glGetString(GL_VENDOR);
    if (perfLevel < Deformable && glVendor && strstr((char *) glVendor, "ATI")) {
        perfLevel = Deformable;
    }
#endif

#ifdef Q_OS_LINUX
    // Nouveau drivers can potentially freeze a machine entirely when taking the point-sprite path.
    const GLubyte *glVendor = QOpenGLContext::currentContext()->functions()->glGetString(GL_VENDOR);
    if (perfLevel < Deformable && glVendor && strstr((const char *) glVendor, "nouveau"))
        perfLevel = Deformable;
#endif

    if (perfLevel >= Colored  && !m_color.isValid())
        m_color = QColor(Qt::white);//Hidden default, but different from unset

    clearShadows();
    if (m_material)
        m_material = 0;

    //Setup material
    QImage colortable;
    QImage sizetable;
    QImage opacitytable;
    QImage image;
    bool imageLoaded = false;
    switch (perfLevel) {//Fallthrough intended
    case Sprites:
        if (!m_spriteEngine) {
            qWarning() << "ImageParticle: No sprite engine...";
            //Sprite performance mode with static image is supported, but not advised
            //Note that in this case it always uses shadow data
        } else {
            image = m_spriteEngine->assembledImage();
            if (image.isNull())//Warning is printed in engine
                return;
            imageLoaded = true;
        }
        m_material = SpriteMaterial::createMaterial();
        if (imageLoaded)
            getState<ImageMaterialData>(m_material)->texture = QSGPlainTexture::fromImage(image);
        getState<ImageMaterialData>(m_material)->animSheetSize = QSizeF(image.size());
        if (m_spriteEngine)
            m_spriteEngine->setCount(m_count);
    case Tabled:
        if (!m_material)
            m_material = TabledMaterial::createMaterial();

        if (m_colorTable) {
            if (m_colorTable->pix.isReady())
                colortable = m_colorTable->pix.image();
            else
                qmlInfo(this) << "Error loading color table: " << m_colorTable->pix.error();
        }

        if (m_sizeTable) {
            if (m_sizeTable->pix.isReady())
                sizetable = m_sizeTable->pix.image();
            else
                qmlInfo(this) << "Error loading size table: " << m_sizeTable->pix.error();
        }

        if (m_opacityTable) {
            if (m_opacityTable->pix.isReady())
                opacitytable = m_opacityTable->pix.image();
            else
                qmlInfo(this) << "Error loading opacity table: " << m_opacityTable->pix.error();
        }

        if (colortable.isNull()){//###Goes through image just for this
            colortable = QImage(1,1,QImage::Format_ARGB32_Premultiplied);
            colortable.fill(Qt::white);
        }
        getState<ImageMaterialData>(m_material)->colorTable = QSGPlainTexture::fromImage(colortable);
        fillUniformArrayFromImage(getState<ImageMaterialData>(m_material)->sizeTable, sizetable, UNIFORM_ARRAY_SIZE);
        fillUniformArrayFromImage(getState<ImageMaterialData>(m_material)->opacityTable, opacitytable, UNIFORM_ARRAY_SIZE);
    case Deformable:
        if (!m_material)
            m_material = DeformableMaterial::createMaterial();
    case Colored:
        if (!m_material)
            m_material = ColoredMaterial::createMaterial();
    default://Also Simple
        if (!m_material)
            m_material = SimpleMaterial::createMaterial();
        if (!imageLoaded) {
            if (!m_image || !m_image->pix.isReady()) {
                if (m_image)
                    qmlInfo(this) << m_image->pix.error();
                delete m_material;
                return;
            }
            //getState<ImageMaterialData>(m_material)->texture //TODO: Shouldn't this be better? But not crash?
            //    = QQuickItemPrivate::get(this)->sceneGraphContext()->textureForFactory(m_imagePix.textureFactory());
            getState<ImageMaterialData>(m_material)->texture = QSGPlainTexture::fromImage(m_image->pix.image());
        }
        getState<ImageMaterialData>(m_material)->texture->setFiltering(QSGTexture::Linear);
        getState<ImageMaterialData>(m_material)->entry = (qreal) m_entryEffect;
        m_material->setFlag(QSGMaterial::Blending | QSGMaterial::RequiresFullMatrix);
    }

    m_nodes.clear();
    for (auto groupId : groupIds()) {
        int count = m_system->groupData[groupId]->size();
        QSGGeometryNode* node = new QSGGeometryNode();
        node->setMaterial(m_material);
        node->markDirty(QSGNode::DirtyMaterial);

        m_nodes.insert(groupId, node);
        m_idxStarts.insert(groupId, m_lastIdxStart);
        m_startsIdx.append(qMakePair<int,int>(m_lastIdxStart, groupId));
        m_lastIdxStart += count;

        //Create Particle Geometry
        int vCount = count * 4;
        int iCount = count * 6;

        QSGGeometry *g;
        if (perfLevel == Sprites)
            g = new QSGGeometry(SpriteParticle_AttributeSet, vCount, iCount);
        else if (perfLevel == Tabled)
            g = new QSGGeometry(DeformableParticle_AttributeSet, vCount, iCount);
        else if (perfLevel == Deformable)
            g = new QSGGeometry(DeformableParticle_AttributeSet, vCount, iCount);
        else if (perfLevel == Colored)
            g = new QSGGeometry(ColoredParticle_AttributeSet, count, 0);
        else //Simple
            g = new QSGGeometry(SimpleParticle_AttributeSet, count, 0);

        node->setFlag(QSGNode::OwnsGeometry);
        node->setGeometry(g);
        if (perfLevel <= Colored){
            g->setDrawingMode(GL_POINTS);
            if (m_debugMode){
                GLfloat pointSizeRange[2];
                QOpenGLContext::currentContext()->functions()->glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, pointSizeRange);
                qDebug() << "Using point sprites, GL_ALIASED_POINT_SIZE_RANGE " <<pointSizeRange[0] << ":" << pointSizeRange[1];
            }
        }else
            g->setDrawingMode(GL_TRIANGLES);

        for (int p=0; p < count; ++p)
            commit(groupId, p);//commit sets geometry for the node, has its own perfLevel switch

        if (perfLevel == Sprites)
            initTexCoords<SpriteVertex>((SpriteVertex*)g->vertexData(), vCount);
        else if (perfLevel == Tabled)
            initTexCoords<DeformableVertex>((DeformableVertex*)g->vertexData(), vCount);
        else if (perfLevel == Deformable)
            initTexCoords<DeformableVertex>((DeformableVertex*)g->vertexData(), vCount);

        if (perfLevel > Colored){
            quint16 *indices = g->indexDataAsUShort();
            for (int i=0; i < count; ++i) {
                int o = i * 4;
                indices[0] = o;
                indices[1] = o + 1;
                indices[2] = o + 2;
                indices[3] = o + 1;
                indices[4] = o + 3;
                indices[5] = o + 2;
                indices += 6;
            }
        }
    }

    if (perfLevel == Sprites)
        spritesUpdate();//Gives all vertexes the initial sprite data, then maintained per frame

    foreach (QSGGeometryNode* node, m_nodes){
        if (node == *(m_nodes.begin()))
            node->setFlag(QSGGeometryNode::OwnsMaterial);//Root node owns the material for memory management purposes
        else
            (*(m_nodes.begin()))->appendChildNode(node);
    }

    *node = *(m_nodes.begin());
    update();
}

static inline bool isOpenGL(QSGRenderContext *rc)
{
    QSGRendererInterface *rif = rc->sceneGraphContext()->rendererInterface(rc);
    return !rif || rif->graphicsApi() == QSGRendererInterface::OpenGL;
}

QSGNode *QQuickImageParticle::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    if (!node && !isOpenGL(QQuickItemPrivate::get(this)->sceneGraphRenderContext()))
        return 0;

    if (m_pleaseReset){
        if (node)
            delete node;
        node = 0;

        m_lastLevel = perfLevel;
        m_nodes.clear();

        m_idxStarts.clear();
        m_startsIdx.clear();
        m_lastIdxStart = 0;

        m_material = 0;

        m_pleaseReset = false;
        m_startedImageLoading = 0;//Cancel a part-way build (may still have a pending load)
    }

    if (m_system && m_system->isRunning() && !m_system->isPaused()){
        prepareNextFrame(&node);
        if (node) {
            update();
            foreach (QSGGeometryNode* n, m_nodes)
                n->markDirty(QSGNode::DirtyGeometry);
        } else if (m_startedImageLoading < 2) {
            update();//To call prepareNextFrame() again from the renderThread
        }
    }

    return node;
}

void QQuickImageParticle::prepareNextFrame(QSGNode **node)
{
    if (*node == 0){//TODO: Staggered loading (as emitted)
        buildParticleNodes(node);
        if (m_debugMode) {
            qDebug() << "QQuickImageParticle Feature level: " << perfLevel;
            qDebug() << "QQuickImageParticle Nodes: ";
            int count = 0;
            for (auto it = m_nodes.keyBegin(), end = m_nodes.keyEnd(); it != end; ++it) {
                qDebug() << "Group " << *it << " (" << m_system->groupData[*it]->size()
                         << " particles)";
                count += m_system->groupData[*it]->size();
            }
            qDebug() << "Total count: " << count;
        }
        if (*node == 0)
            return;
    }
    qint64 timeStamp = m_system->systemSync(this);

    qreal time = timeStamp / 1000.;

    switch (perfLevel){//Fall-through intended
    case Sprites:
        //Advance State
        if (m_spriteEngine)
            m_spriteEngine->updateSprites(timeStamp);//fires signals if anim changed
        spritesUpdate(time);
    case Tabled:
    case Deformable:
    case Colored:
    case Simple:
    default: //Also Simple
        getState<ImageMaterialData>(m_material)->timestamp = time;
        break;
    }
    foreach (QSGGeometryNode* node, m_nodes)
        node->markDirty(QSGNode::DirtyMaterial);
}

void QQuickImageParticle::spritesUpdate(qreal time)
{
    // Sprite progression handled CPU side, so as to have per-frame control.
    for (auto groupId : groupIds()) {
        for (QQuickParticleData* mainDatum : qAsConst(m_system->groupData[groupId]->data)) {
            QSGGeometryNode *node = m_nodes[groupId];
            if (!node)
                continue;
            //TODO: Interpolate between two different animations if it's going to transition next frame
            //      This is particularly important for cut-up sprites.
            QQuickParticleData* datum = (mainDatum->animationOwner == this ? mainDatum : getShadowDatum(mainDatum));
            int spriteIdx = 0;
            for (int i = 0; i<m_startsIdx.count(); i++) {
                if (m_startsIdx[i].second == groupId){
                    spriteIdx = m_startsIdx[i].first + datum->index;
                    break;
                }
            }

            double frameAt;
            qreal progress = 0;

            if (datum->frameDuration > 0) {
                qreal frame = (time - datum->animT)/(datum->frameDuration / 1000.0);
                frame = qBound((qreal)0.0, frame, (qreal)((qreal)datum->frameCount - 1.0));//Stop at count-1 frames until we have between anim interpolation
                if (m_spritesInterpolate)
                    progress = std::modf(frame,&frameAt);
                else
                    std::modf(frame,&frameAt);
            } else {
                datum->frameAt++;
                if (datum->frameAt >= datum->frameCount){
                    datum->frameAt = 0;
                    m_spriteEngine->advance(spriteIdx);
                }
                frameAt = datum->frameAt;
            }
            if (m_spriteEngine->sprite(spriteIdx)->reverse())//### Store this in datum too?
                frameAt = (datum->frameCount - 1) - frameAt;
            QSizeF sheetSize = getState<ImageMaterialData>(m_material)->animSheetSize;
            qreal y = datum->animY / sheetSize.height();
            qreal w = datum->animWidth / sheetSize.width();
            qreal h = datum->animHeight / sheetSize.height();
            qreal x1 = datum->animX / sheetSize.width();
            x1 += frameAt * w;
            qreal x2 = x1;
            if (frameAt < (datum->frameCount-1))
                x2 += w;

            SpriteVertex *spriteVertices = (SpriteVertex *) node->geometry()->vertexData();
            spriteVertices += datum->index*4;
            for (int i=0; i<4; i++) {
                spriteVertices[i].animX1 = x1;
                spriteVertices[i].animY1 = y;
                spriteVertices[i].animX2 = x2;
                spriteVertices[i].animY2 = y;
                spriteVertices[i].animW = w;
                spriteVertices[i].animH = h;
                spriteVertices[i].animProgress = progress;
            }
        }
    }
}

void QQuickImageParticle::spriteAdvance(int spriteIdx)
{
    if (!m_startsIdx.count())//Probably overly defensive
        return;

    int gIdx = -1;
    int i;
    for (i = 0; i<m_startsIdx.count(); i++) {
        if (spriteIdx < m_startsIdx[i].first) {
            gIdx = m_startsIdx[i-1].second;
            break;
        }
    }
    if (gIdx == -1)
        gIdx = m_startsIdx[i-1].second;
    int pIdx = spriteIdx - m_startsIdx[i-1].first;

    QQuickParticleData* mainDatum = m_system->groupData[gIdx]->data[pIdx];
    QQuickParticleData* datum = (mainDatum->animationOwner == this ? mainDatum : getShadowDatum(mainDatum));

    datum->animIdx = m_spriteEngine->spriteState(spriteIdx);
    datum->animT = m_spriteEngine->spriteStart(spriteIdx)/1000.0;
    datum->frameCount = m_spriteEngine->spriteFrames(spriteIdx);
    datum->frameDuration = m_spriteEngine->spriteDuration(spriteIdx) / datum->frameCount;
    datum->animX = m_spriteEngine->spriteX(spriteIdx);
    datum->animY = m_spriteEngine->spriteY(spriteIdx);
    datum->animWidth = m_spriteEngine->spriteWidth(spriteIdx);
    datum->animHeight = m_spriteEngine->spriteHeight(spriteIdx);
}

void QQuickImageParticle::reloadColor(const Color4ub &c, QQuickParticleData* d)
{
    d->color = c;
    //TODO: get index for reload - or make function take an index
}

void QQuickImageParticle::initialize(int gIdx, int pIdx)
{
    Color4ub color;
    QQuickParticleData* datum = m_system->groupData[gIdx]->data[pIdx];
    qreal redVariation = m_color_variation + m_redVariation;
    qreal greenVariation = m_color_variation + m_greenVariation;
    qreal blueVariation = m_color_variation + m_blueVariation;
    int spriteIdx = 0;
    if (m_spriteEngine) {
        spriteIdx = m_idxStarts[gIdx] + datum->index;
        if (spriteIdx >= m_spriteEngine->count())
            m_spriteEngine->setCount(spriteIdx+1);
    }

    float rotation;
    float rotationVelocity;
    float autoRotate;
    switch (perfLevel){//Fall-through is intended on all of them
        case Sprites:
            // Initial Sprite State
            if (m_explicitAnimation && m_spriteEngine){
                if (!datum->animationOwner)
                    datum->animationOwner = this;
                QQuickParticleData* writeTo = (datum->animationOwner == this ? datum : getShadowDatum(datum));
                writeTo->animT = writeTo->t;
                //writeTo->animInterpolate = m_spritesInterpolate;
                if (m_spriteEngine){
                    m_spriteEngine->start(spriteIdx);
                    writeTo->frameCount = m_spriteEngine->spriteFrames(spriteIdx);
                    writeTo->frameDuration = m_spriteEngine->spriteDuration(spriteIdx) / writeTo->frameCount;
                    writeTo->animIdx = 0;//Always starts at 0
                    writeTo->frameAt = -1;
                    writeTo->animX = m_spriteEngine->spriteX(spriteIdx);
                    writeTo->animY = m_spriteEngine->spriteY(spriteIdx);
                    writeTo->animWidth = m_spriteEngine->spriteWidth(spriteIdx);
                    writeTo->animHeight = m_spriteEngine->spriteHeight(spriteIdx);
                }
            } else {
                QQuickParticleData* writeTo = getShadowDatum(datum);
                writeTo->animT = datum->t;
                writeTo->frameCount = 1;
                writeTo->frameDuration = 60000000.0;
                writeTo->frameAt = -1;
                writeTo->animIdx = 0;
                writeTo->animT = 0;
                writeTo->animX = writeTo->animY = 0;
                writeTo->animWidth = getState<ImageMaterialData>(m_material)->animSheetSize.width();
                writeTo->animHeight = getState<ImageMaterialData>(m_material)->animSheetSize.height();
            }
        case Tabled:
        case Deformable:
            //Initial Rotation
            if (m_explicitDeformation){
                if (!datum->deformationOwner)
                    datum->deformationOwner = this;
                if (m_xVector){
                    const QPointF &ret = m_xVector->sample(QPointF(datum->x, datum->y));
                    if (datum->deformationOwner == this) {
                        datum->xx = ret.x();
                        datum->xy = ret.y();
                    } else {
                        getShadowDatum(datum)->xx = ret.x();
                        getShadowDatum(datum)->xy = ret.y();
                    }
                }
                if (m_yVector){
                    const QPointF &ret = m_yVector->sample(QPointF(datum->x, datum->y));
                    if (datum->deformationOwner == this) {
                        datum->yx = ret.x();
                        datum->yy = ret.y();
                    } else {
                        getShadowDatum(datum)->yx = ret.x();
                        getShadowDatum(datum)->yy = ret.y();
                    }
                }
            }

            if (m_explicitRotation){
                if (!datum->rotationOwner)
                    datum->rotationOwner = this;
                rotation =
                        (m_rotation + (m_rotationVariation - 2*((qreal)rand()/RAND_MAX)*m_rotationVariation) ) * CONV;
                rotationVelocity =
                        (m_rotationVelocity + (m_rotationVelocityVariation - 2*((qreal)rand()/RAND_MAX)*m_rotationVelocityVariation) ) * CONV;
                autoRotate = m_autoRotation?1.0:0.0;
                if (datum->rotationOwner == this) {
                    datum->rotation = rotation;
                    datum->rotationVelocity = rotationVelocity;
                    datum->autoRotate = autoRotate;
                } else {
                    getShadowDatum(datum)->rotation = rotation;
                    getShadowDatum(datum)->rotationVelocity = rotationVelocity;
                    getShadowDatum(datum)->autoRotate = autoRotate;
                }
            }
        case Colored:
            //Color initialization
            // Particle color
            if (m_explicitColor) {
                if (!datum->colorOwner)
                    datum->colorOwner = this;
                color.r = m_color.red() * (1 - redVariation) + rand() % 256 * redVariation;
                color.g = m_color.green() * (1 - greenVariation) + rand() % 256 * greenVariation;
                color.b = m_color.blue() * (1 - blueVariation) + rand() % 256 * blueVariation;
                color.a = m_alpha * m_color.alpha() * (1 - m_alphaVariation) + rand() % 256 * m_alphaVariation;
                if (datum->colorOwner == this)
                    datum->color = color;
                else
                    getShadowDatum(datum)->color = color;
            }
        default:
            break;
    }
}

void QQuickImageParticle::commit(int gIdx, int pIdx)
{
    if (m_pleaseReset)
        return;
    QSGGeometryNode *node = m_nodes[gIdx];
    if (!node)
        return;
    QQuickParticleData* datum = m_system->groupData[gIdx]->data[pIdx];
    SpriteVertex *spriteVertices = (SpriteVertex *) node->geometry()->vertexData();
    DeformableVertex *deformableVertices = (DeformableVertex *) node->geometry()->vertexData();
    ColoredVertex *coloredVertices = (ColoredVertex *) node->geometry()->vertexData();
    SimpleVertex *simpleVertices = (SimpleVertex *) node->geometry()->vertexData();
    switch (perfLevel){//No automatic fall through intended on this one
    case Sprites:
        spriteVertices += pIdx*4;
        for (int i=0; i<4; i++){
            spriteVertices[i].x = datum->x  - m_systemOffset.x();
            spriteVertices[i].y = datum->y  - m_systemOffset.y();
            spriteVertices[i].t = datum->t;
            spriteVertices[i].lifeSpan = datum->lifeSpan;
            spriteVertices[i].size = datum->size;
            spriteVertices[i].endSize = datum->endSize;
            spriteVertices[i].vx = datum->vx;
            spriteVertices[i].vy = datum->vy;
            spriteVertices[i].ax = datum->ax;
            spriteVertices[i].ay = datum->ay;
            if (m_explicitDeformation && datum->deformationOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                spriteVertices[i].xx = shadow->xx;
                spriteVertices[i].xy = shadow->xy;
                spriteVertices[i].yx = shadow->yx;
                spriteVertices[i].yy = shadow->yy;
            } else {
                spriteVertices[i].xx = datum->xx;
                spriteVertices[i].xy = datum->xy;
                spriteVertices[i].yx = datum->yx;
                spriteVertices[i].yy = datum->yy;
            }
            if (m_explicitRotation && datum->rotationOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                spriteVertices[i].rotation = shadow->rotation;
                spriteVertices[i].rotationVelocity = shadow->rotationVelocity;
                spriteVertices[i].autoRotate = shadow->autoRotate;
            } else {
                spriteVertices[i].rotation = datum->rotation;
                spriteVertices[i].rotationVelocity = datum->rotationVelocity;
                spriteVertices[i].autoRotate = datum->autoRotate;
            }
            //Sprite-related vertices updated per-frame in spritesUpdate(), not on demand
            if (m_explicitColor && datum->colorOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                spriteVertices[i].color.r = shadow->color.r;
                spriteVertices[i].color.g = shadow->color.g;
                spriteVertices[i].color.b = shadow->color.b;
                spriteVertices[i].color.a = shadow->color.a;
            } else {
                spriteVertices[i].color.r = datum->color.r;
                spriteVertices[i].color.g = datum->color.g;
                spriteVertices[i].color.b = datum->color.b;
                spriteVertices[i].color.a = datum->color.a;
            }
        }
        break;
    case Tabled: //Fall through until it has its own vertex class
    case Deformable:
        deformableVertices += pIdx*4;
        for (int i=0; i<4; i++){
            deformableVertices[i].x = datum->x  - m_systemOffset.x();
            deformableVertices[i].y = datum->y  - m_systemOffset.y();
            deformableVertices[i].t = datum->t;
            deformableVertices[i].lifeSpan = datum->lifeSpan;
            deformableVertices[i].size = datum->size;
            deformableVertices[i].endSize = datum->endSize;
            deformableVertices[i].vx = datum->vx;
            deformableVertices[i].vy = datum->vy;
            deformableVertices[i].ax = datum->ax;
            deformableVertices[i].ay = datum->ay;
            if (m_explicitDeformation && datum->deformationOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                deformableVertices[i].xx = shadow->xx;
                deformableVertices[i].xy = shadow->xy;
                deformableVertices[i].yx = shadow->yx;
                deformableVertices[i].yy = shadow->yy;
            } else {
                deformableVertices[i].xx = datum->xx;
                deformableVertices[i].xy = datum->xy;
                deformableVertices[i].yx = datum->yx;
                deformableVertices[i].yy = datum->yy;
            }
            if (m_explicitRotation && datum->rotationOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                deformableVertices[i].rotation = shadow->rotation;
                deformableVertices[i].rotationVelocity = shadow->rotationVelocity;
                deformableVertices[i].autoRotate = shadow->autoRotate;
            } else {
                deformableVertices[i].rotation = datum->rotation;
                deformableVertices[i].rotationVelocity = datum->rotationVelocity;
                deformableVertices[i].autoRotate = datum->autoRotate;
            }
            if (m_explicitColor && datum->colorOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                deformableVertices[i].color.r = shadow->color.r;
                deformableVertices[i].color.g = shadow->color.g;
                deformableVertices[i].color.b = shadow->color.b;
                deformableVertices[i].color.a = shadow->color.a;
            } else {
                deformableVertices[i].color.r = datum->color.r;
                deformableVertices[i].color.g = datum->color.g;
                deformableVertices[i].color.b = datum->color.b;
                deformableVertices[i].color.a = datum->color.a;
            }
        }
        break;
    case Colored:
        coloredVertices += pIdx*1;
        for (int i=0; i<1; i++){
            coloredVertices[i].x = datum->x  - m_systemOffset.x();
            coloredVertices[i].y = datum->y  - m_systemOffset.y();
            coloredVertices[i].t = datum->t;
            coloredVertices[i].lifeSpan = datum->lifeSpan;
            coloredVertices[i].size = datum->size;
            coloredVertices[i].endSize = datum->endSize;
            coloredVertices[i].vx = datum->vx;
            coloredVertices[i].vy = datum->vy;
            coloredVertices[i].ax = datum->ax;
            coloredVertices[i].ay = datum->ay;
            if (m_explicitColor && datum->colorOwner != this) {
                QQuickParticleData* shadow = getShadowDatum(datum);
                coloredVertices[i].color.r = shadow->color.r;
                coloredVertices[i].color.g = shadow->color.g;
                coloredVertices[i].color.b = shadow->color.b;
                coloredVertices[i].color.a = shadow->color.a;
            } else {
                coloredVertices[i].color.r = datum->color.r;
                coloredVertices[i].color.g = datum->color.g;
                coloredVertices[i].color.b = datum->color.b;
                coloredVertices[i].color.a = datum->color.a;
            }
        }
        break;
    case Simple:
        simpleVertices += pIdx*1;
        for (int i=0; i<1; i++){
            simpleVertices[i].x = datum->x - m_systemOffset.x();
            simpleVertices[i].y = datum->y - m_systemOffset.y();
            simpleVertices[i].t = datum->t;
            simpleVertices[i].lifeSpan = datum->lifeSpan;
            simpleVertices[i].size = datum->size;
            simpleVertices[i].endSize = datum->endSize;
            simpleVertices[i].vx = datum->vx;
            simpleVertices[i].vy = datum->vy;
            simpleVertices[i].ax = datum->ax;
            simpleVertices[i].ay = datum->ay;
        }
        break;
    default:
        break;
    }
}



QT_END_NAMESPACE
