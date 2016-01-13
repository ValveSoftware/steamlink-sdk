/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QML Shaders plugin of the Qt Toolkit.
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

#ifndef SHADEREFFECTSOURCE_H
#define SHADEREFFECTSOURCE_H

#include <QDeclarativeItem>
#include <QGLFunctions>
#include <QPointer>

QT_BEGIN_NAMESPACE

class ShaderEffectBuffer;

class ShaderEffectSource : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QDeclarativeItem *sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(QSize textureSize READ textureSize WRITE setTextureSize NOTIFY textureSizeChanged)
    Q_PROPERTY(bool live READ isLive WRITE setLive NOTIFY liveChanged)
    Q_PROPERTY(bool hideSource READ hideSource WRITE setHideSource NOTIFY hideSourceChanged)
    Q_PROPERTY(WrapMode wrapMode READ wrapMode WRITE setWrapMode NOTIFY wrapModeChanged)
    Q_ENUMS(WrapMode)
    Q_ENUMS(Format)

public:
    enum WrapMode {
            ClampToEdge,
            RepeatHorizontally,
            RepeatVertically,
            Repeat
        };

    enum Format {
        Alpha = GL_ALPHA,
        RGB = GL_RGB,
        RGBA = GL_RGBA
    };

    ShaderEffectSource(QDeclarativeItem *parent = 0);
    virtual ~ShaderEffectSource();

    QDeclarativeItem *sourceItem() const { return m_sourceItem.data(); }
    void setSourceItem(QDeclarativeItem *item);

    QRectF sourceRect() const { return m_sourceRect; };
    void setSourceRect(const QRectF &rect);

    QSize textureSize() const { return m_textureSize; }
    void setTextureSize(const QSize &size);

    bool isLive() const { return m_live; }
    void setLive(bool s);

    bool hideSource() const { return m_hideSource; }
    void setHideSource(bool hide);

    WrapMode wrapMode() const { return m_wrapMode; };
    void setWrapMode(WrapMode mode);

    bool isActive() const { return m_refs; }
    void bind();
    void refFromEffectItem();
    void derefFromEffectItem();
    void updateBackbuffer();

    ShaderEffectBuffer* fbo() { return m_fbo; }
    bool isDirtyTexture() { return m_dirtyTexture; }
    bool isMirrored() { return m_mirrored; }

    Q_INVOKABLE void grab();

Q_SIGNALS:
    void sourceItemChanged();
    void sourceRectChanged();
    void textureSizeChanged();
    void formatChanged();
    void liveChanged();
    void hideSourceChanged();
    void activeChanged();
    void repaintRequired();
    void wrapModeChanged();

public Q_SLOTS:
    void markSceneGraphDirty();
    void markSourceSizeDirty();
    void markSourceItemDirty();

private:
    void updateSizeAndTexture();
    void attachSourceItem();
    void detachSourceItem();

private:
    QPointer<QDeclarativeItem> m_sourceItem;
    WrapMode m_wrapMode;
    QRectF m_sourceRect;
    QSize m_textureSize;
    Format m_format;
    QSize m_size;

    ShaderEffectBuffer *m_fbo;
    ShaderEffectBuffer *m_multisampledFbo;
    int m_refs;
    bool m_dirtyTexture : 1;
    bool m_dirtySceneGraph : 1;
    bool m_multisamplingSupported : 1;
    bool m_checkedForMultisamplingSupport : 1;
    bool m_live : 1;
    bool m_hideSource : 1;
    bool m_mirrored : 1;
};

QT_END_NAMESPACE


#endif // SHADEREFFECTSOURCE_H
