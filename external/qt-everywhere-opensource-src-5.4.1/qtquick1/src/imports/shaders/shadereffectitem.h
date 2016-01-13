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

#ifndef SHADEREFFECTITEM_H
#define SHADEREFFECTITEM_H

#include <QDeclarativeItem>
#include <QSignalMapper>
#include "shadereffectsource.h"
#include "scenegraph/qsggeometry.h"

QT_BEGIN_NAMESPACE

class QGLFramebufferObject;
class QGLShaderProgram;

class ShaderEffectItem : public QDeclarativeItem
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativeParserStatus)
    Q_PROPERTY(QString fragmentShader READ fragmentShader WRITE setFragmentShader NOTIFY fragmentShaderChanged)
    Q_PROPERTY(QString vertexShader READ vertexShader WRITE setVertexShader NOTIFY vertexShaderChanged)
    Q_PROPERTY(bool blending READ blending WRITE setBlending NOTIFY blendingChanged)
    Q_PROPERTY(QSize meshResolution READ meshResolution WRITE setMeshResolution NOTIFY meshResolutionChanged)

public:
    ShaderEffectItem(QDeclarativeItem* parent = 0);
    ~ShaderEffectItem();

    virtual void componentComplete();
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    QString fragmentShader() const { return m_fragment_code; }
    void setFragmentShader(const QString &code);

    QString vertexShader() const { return m_vertex_code; }
    void setVertexShader(const QString &code);

    bool blending() const { return m_blending; }
    void setBlending(bool enable);

    QSize meshResolution() const { return m_meshResolution; }
    void setMeshResolution(const QSize &size);

    void preprocess();

Q_SIGNALS:
    void fragmentShaderChanged();
    void vertexShaderChanged();
    void blendingChanged();
    void activeChanged();
    void meshResolutionChanged();

protected:
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);

private Q_SLOTS:
    void changeSource(int index);
    void handleVisibilityChange();
    void markDirty();

private:
    void checkViewportUpdateMode();
    void renderEffect(QPainter *painter, const QMatrix4x4 &matrix);
    void updateEffectState(const QMatrix4x4 &matrix);
    void updateGeometry();
    void bindGeometry();
    void setSource(const QVariant &var, int index);
    void disconnectPropertySignals();
    void connectPropertySignals();
    void reset();
    void updateProperties();
    void updateShaderProgram();
    void lookThroughShaderCode(const QString &code);
    bool active() const { return m_active; }
    void setActive(bool enable);

private:
    QString m_fragment_code;
    QString m_vertex_code;
    QGLShaderProgram* m_program;
    QVector<const char *> m_attributeNames;
    QSet<QByteArray> m_uniformNames;
    QSize m_meshResolution;
    QSGGeometry m_geometry;

    struct SourceData
    {
        QSignalMapper *mapper;
        QPointer<ShaderEffectSource> source;
        QPointer<QDeclarativeItem> item;
        QByteArray name;
    };

    QVector<SourceData> m_sources;

    bool m_changed : 1;
    bool m_blending : 1;
    bool m_program_dirty : 1;
    bool m_active : 1;
    bool m_respectsMatrix : 1;
    bool m_respectsOpacity : 1;
    bool m_checkedViewportUpdateMode : 1;
    bool m_checkedOpenGL : 1;
    bool m_checkedShaderPrograms : 1;
    bool m_hasShaderPrograms : 1;
    bool m_mirrored : 1;
    bool m_defaultVertexShader : 1;
};

QT_END_NAMESPACE

#endif // SHADEREFFECTITEM_H
