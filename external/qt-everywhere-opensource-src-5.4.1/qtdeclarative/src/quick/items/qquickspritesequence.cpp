/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickspritesequence_p.h"
#include "qquicksprite_p.h"
#include "qquickspriteengine_p.h"
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgtexture.h>
#include <QtQuick/qquickwindow.h>
#include <QtQml/qqmlinfo.h>
#include <QFile>
#include <cmath>
#include <qmath.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

class QQuickSpriteSequenceMaterial : public QSGMaterial
{
public:
    QQuickSpriteSequenceMaterial();
    ~QQuickSpriteSequenceMaterial();
    virtual QSGMaterialType *type() const { static QSGMaterialType type; return &type; }
    virtual QSGMaterialShader *createShader() const;
    virtual int compare(const QSGMaterial *other) const
    {
        return this - static_cast<const QQuickSpriteSequenceMaterial *>(other);
    }

    QSGTexture *texture;

    float animT;
    float animX1;
    float animY1;
    float animX2;
    float animY2;
    float animW;
    float animH;
};

QQuickSpriteSequenceMaterial::QQuickSpriteSequenceMaterial()
    : texture(0)
    , animT(0.0f)
    , animX1(0.0f)
    , animY1(0.0f)
    , animX2(0.0f)
    , animY2(0.0f)
    , animW(1.0f)
    , animH(1.0f)
{
    setFlag(Blending, true);
}

QQuickSpriteSequenceMaterial::~QQuickSpriteSequenceMaterial()
{
    delete texture;
}

class SpriteSequenceMaterialData : public QSGMaterialShader
{
public:
    SpriteSequenceMaterialData()
        : QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/items/shaders/sprite.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/items/shaders/sprite.frag"));
    }

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *)
    {
        QQuickSpriteSequenceMaterial *m = static_cast<QQuickSpriteSequenceMaterial *>(newEffect);
        m->texture->bind();

        program()->setUniformValue(m_opacity_id, state.opacity());
        program()->setUniformValue(m_animData_id, m->animW, m->animH, m->animT);
        program()->setUniformValue(m_animPos_id, m->animX1, m->animY1, m->animX2, m->animY2);

        if (state.isMatrixDirty())
            program()->setUniformValue(m_matrix_id, state.combinedMatrix());
    }

    virtual void initialize() {
        m_matrix_id = program()->uniformLocation("qt_Matrix");
        m_opacity_id = program()->uniformLocation("qt_Opacity");
        m_animData_id = program()->uniformLocation("animData");
        m_animPos_id = program()->uniformLocation("animPos");
    }

    virtual char const *const *attributeNames() const {
        static const char *attr[] = {
           "vPos",
           "vTex",
            0
        };
        return attr;
    }

    int m_matrix_id;
    int m_opacity_id;
    int m_animData_id;
    int m_animPos_id;
};

QSGMaterialShader *QQuickSpriteSequenceMaterial::createShader() const
{
    return new SpriteSequenceMaterialData;
}

struct SpriteVertex {
    float x;
    float y;
    float tx;
    float ty;
};

struct SpriteVertices {
    SpriteVertex v1;
    SpriteVertex v2;
    SpriteVertex v3;
    SpriteVertex v4;
};

/*!
    \qmltype SpriteSequence
    \instantiates QQuickSpriteSequence
    \inqmlmodule QtQuick
    \ingroup qtquick-visual-utility
    \inherits Item
    \brief Draws a sprite animation

    SpriteSequence renders and controls a list of animations defined
    by \l Sprite types.

    For full details, see the \l{Sprite Animations} overview.
*/
/*!
    \qmlproperty bool QtQuick::SpriteSequence::running

    Whether the sprite is animating or not.

    Default is true
*/
/*!
    \qmlproperty bool QtQuick::SpriteSequence::interpolate

    If true, interpolation will occur between sprite frames to make the
    animation appear smoother.

    Default is true.
*/
/*!
    \qmlproperty string QtQuick::SpriteSequence::currentSprite

    The name of the Sprite which is currently animating.
*/
/*!
    \qmlproperty string QtQuick::SpriteSequence::goalSprite

    The name of the Sprite which the animation should move to.

    Sprite states have defined durations and transitions between them, setting goalState
    will cause it to disregard any path weightings (including 0) and head down the path
    which will reach the goalState quickest (fewest animations). It will pass through
    intermediate states on that path, and animate them for their duration.

    If it is possible to return to the goalState from the starting point of the goalState
    it will continue to do so until goalState is set to "" or an unreachable state.
*/
/*! \qmlmethod QtQuick::SpriteSequence::jumpTo(string sprite)

    This function causes the SpriteSequence to jump to the specified sprite immediately, intermediate
    sprites are not played. The \a sprite argument is the name of the sprite you wish to jump to.
*/
/*!
    \qmlproperty list<Sprite> QtQuick::SpriteSequence::sprites

    The sprite or sprites to draw. Sprites will be scaled to the size of this item.
*/

//TODO: Implicitly size element to size of first sprite?
QQuickSpriteSequence::QQuickSpriteSequence(QQuickItem *parent) :
    QQuickItem(parent)
    , m_node(0)
    , m_material(0)
    , m_spriteEngine(0)
    , m_curFrame(0)
    , m_pleaseReset(false)
    , m_running(true)
    , m_interpolate(true)
    , m_curStateIdx(0)
{
    setFlag(ItemHasContents);
    connect(this, SIGNAL(runningChanged(bool)),
            this, SLOT(update()));
    connect(this, SIGNAL(widthChanged()),
            this, SLOT(sizeVertices()));
    connect(this, SIGNAL(heightChanged()),
            this, SLOT(sizeVertices()));
}

void QQuickSpriteSequence::jumpTo(const QString &sprite)
{
    if (!m_spriteEngine)
        return;
    m_spriteEngine->setGoal(m_spriteEngine->stateIndex(sprite), 0, true);
}

void QQuickSpriteSequence::setGoalSprite(const QString &sprite)
{
    if (m_goalState != sprite){
        m_goalState = sprite;
        emit goalSpriteChanged(sprite);
        if (m_spriteEngine)
            m_spriteEngine->setGoal(m_spriteEngine->stateIndex(sprite));
    }
}

QQmlListProperty<QQuickSprite> QQuickSpriteSequence::sprites()
{
    return QQmlListProperty<QQuickSprite>(this, &m_sprites, spriteAppend, spriteCount, spriteAt, spriteClear);
}

void QQuickSpriteSequence::createEngine()
{
    //TODO: delay until component complete
    if (m_spriteEngine)
        delete m_spriteEngine;
    if (m_sprites.count()) {
        m_spriteEngine = new QQuickSpriteEngine(m_sprites, this);
        if (!m_goalState.isEmpty())
            m_spriteEngine->setGoal(m_spriteEngine->stateIndex(m_goalState));
    } else {
        m_spriteEngine = 0;
    }
    reset();
}

static QSGGeometry::Attribute SpriteSequence_Attributes[] = {
    QSGGeometry::Attribute::create(0, 2, GL_FLOAT, true),   // pos
    QSGGeometry::Attribute::create(1, 2, GL_FLOAT),         // tex
};

static QSGGeometry::AttributeSet SpriteSequence_AttributeSet =
{
    2, // Attribute Count
    (2+2) * sizeof(float),
    SpriteSequence_Attributes
};

void QQuickSpriteSequence::sizeVertices()
{
    if (!m_node)
        return;

    SpriteVertices *p = (SpriteVertices *) m_node->geometry()->vertexData();
    p->v1.x = 0;
    p->v1.y = 0;

    p->v2.x = width();
    p->v2.y = 0;

    p->v3.x = 0;
    p->v3.y = height();

    p->v4.x = width();
    p->v4.y = height();
}

QSGGeometryNode* QQuickSpriteSequence::buildNode()
{
    if (!m_spriteEngine) {
        qmlInfo(this) << "No sprite engine...";
        return 0;
    } else if (m_spriteEngine->status() == QQuickPixmap::Null) {
        m_spriteEngine->startAssemblingImage();
        update();//Schedule another update, where we will check again
        return 0;
    } else if (m_spriteEngine->status() == QQuickPixmap::Loading) {
        update();//Schedule another update, where we will check again
        return 0;
    }

    m_material = new QQuickSpriteSequenceMaterial();

    QImage image = m_spriteEngine->assembledImage();
    if (image.isNull())
        return 0;
    m_sheetSize = QSizeF(image.size());
    m_material->texture = window()->createTextureFromImage(image);
    m_material->texture->setFiltering(QSGTexture::Linear);
    m_spriteEngine->start(0);
    m_material->animT = 0;
    m_material->animX1 = m_spriteEngine->spriteX() / m_sheetSize.width();
    m_material->animY1 = m_spriteEngine->spriteY() / m_sheetSize.height();
    m_material->animX2 = m_material->animX1;
    m_material->animY2 = m_material->animY1;
    m_material->animW = m_spriteEngine->spriteWidth() / m_sheetSize.width();
    m_material->animH = m_spriteEngine->spriteHeight() / m_sheetSize.height();
    m_curState = m_spriteEngine->state(m_spriteEngine->curState())->name();
    emit currentSpriteChanged(m_curState);

    int vCount = 4;
    int iCount = 6;
    QSGGeometry *g = new QSGGeometry(SpriteSequence_AttributeSet, vCount, iCount);
    g->setDrawingMode(GL_TRIANGLES);

    SpriteVertices *p = (SpriteVertices *) g->vertexData();
    QRectF texRect = m_material->texture->normalizedTextureSubRect();

    p->v1.tx = texRect.topLeft().x();
    p->v1.ty = texRect.topLeft().y();

    p->v2.tx = texRect.topRight().x();
    p->v2.ty = texRect.topRight().y();

    p->v3.tx = texRect.bottomLeft().x();
    p->v3.ty = texRect.bottomLeft().y();

    p->v4.tx = texRect.bottomRight().x();
    p->v4.ty = texRect.bottomRight().y();

    quint16 *indices = g->indexDataAsUShort();
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 1;
    indices[4] = 3;
    indices[5] = 2;


    m_timestamp.start();
    m_node = new QSGGeometryNode();
    m_node->setGeometry(g);
    m_node->setMaterial(m_material);
    m_node->setFlag(QSGGeometryNode::OwnsMaterial);
    sizeVertices();
    return m_node;
}

void QQuickSpriteSequence::reset()
{
    m_pleaseReset = true;
}

QSGNode *QQuickSpriteSequence::updatePaintNode(QSGNode *, UpdatePaintNodeData *)
{
    if (m_pleaseReset) {
        delete m_node;

        m_node = 0;
        m_material = 0;
        m_pleaseReset = false;
    }

    prepareNextFrame();

    if (m_running) {
        update();
        if (m_node)
            m_node->markDirty(QSGNode::DirtyMaterial);
    }

    return m_node;
}

void QQuickSpriteSequence::prepareNextFrame()
{
    if (m_node == 0)
        m_node = buildNode();
    if (m_node == 0) //error creating node
        return;

    uint timeInt = m_timestamp.elapsed();
    qreal time =  timeInt / 1000.;

    //Advance State
    m_spriteEngine->updateSprites(timeInt);
    if (m_curStateIdx != m_spriteEngine->curState()) {
        m_curStateIdx = m_spriteEngine->curState();
        m_curState = m_spriteEngine->state(m_spriteEngine->curState())->name();
        emit currentSpriteChanged(m_curState);
        m_curFrame= -1;
    }

    //Advance Sprite
    qreal animT = m_spriteEngine->spriteStart()/1000.0;
    qreal frameCount = m_spriteEngine->spriteFrames();
    qreal frameDuration = m_spriteEngine->spriteDuration()/frameCount;
    double frameAt;
    qreal progress;
    if (frameDuration > 0) {
        qreal frame = (time - animT)/(frameDuration / 1000.0);
        frame = qBound(qreal(0.0), frame, frameCount - qreal(1.0));//Stop at count-1 frames until we have between anim interpolation
        progress = modf(frame,&frameAt);
    } else {
        m_curFrame++;
        if (m_curFrame >= frameCount){
            m_curFrame = 0;
            m_spriteEngine->advance();
        }
        frameAt = m_curFrame;
        progress = 0;
    }
    if (m_spriteEngine->sprite()->reverse())
        frameAt = (m_spriteEngine->spriteFrames() - 1) - frameAt;
    qreal y = m_spriteEngine->spriteY() / m_sheetSize.height();
    qreal w = m_spriteEngine->spriteWidth() / m_sheetSize.width();
    qreal h = m_spriteEngine->spriteHeight() / m_sheetSize.height();
    qreal x1 = m_spriteEngine->spriteX() / m_sheetSize.width();
    x1 += frameAt * w;
    qreal x2 = x1;
    if (frameAt < (frameCount-1))
        x2 += w;

    m_material->animX1 = x1;
    m_material->animY1 = y;
    m_material->animX2 = x2;
    m_material->animY2 = y;
    m_material->animW = w;
    m_material->animH = h;
    m_material->animT = m_interpolate ? progress : 0.0;
}

QT_END_NAMESPACE
