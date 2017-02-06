/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtDataVisualization API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef ABSTRACTDECLARATIVE_P_H
#define ABSTRACTDECLARATIVE_P_H

#include "datavisualizationglobal_p.h"
#include "abstract3dcontroller_p.h"
#include "declarativescene_p.h"

#include <QtQuick/QQuickItem>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>

#if !defined(Q_OS_MAC) && !defined(Q_OS_ANDROID) && !defined(Q_OS_WINRT)
#define USE_SHARED_CONTEXT
#endif

#ifndef USE_SHARED_CONTEXT
#include "glstatestore_p.h"
#endif

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

class AbstractDeclarative : public QQuickItem
{
    Q_OBJECT
    Q_ENUMS(ShadowQuality)
    Q_ENUMS(RenderingMode)
    Q_ENUMS(ElementType)
    Q_FLAGS(SelectionFlag SelectionFlags)
    Q_FLAGS(OptimizationHint OptimizationHints)
    Q_PROPERTY(SelectionFlags selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    Q_PROPERTY(ShadowQuality shadowQuality READ shadowQuality WRITE setShadowQuality NOTIFY shadowQualityChanged)
    Q_PROPERTY(bool shadowsSupported READ shadowsSupported NOTIFY shadowsSupportedChanged)
    Q_PROPERTY(int msaaSamples READ msaaSamples WRITE setMsaaSamples NOTIFY msaaSamplesChanged)
    Q_PROPERTY(Declarative3DScene* scene READ scene NOTIFY sceneChanged)
    Q_PROPERTY(QAbstract3DInputHandler* inputHandler READ inputHandler WRITE setInputHandler NOTIFY inputHandlerChanged)
    Q_PROPERTY(Q3DTheme* theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(RenderingMode renderingMode READ renderingMode WRITE setRenderingMode NOTIFY renderingModeChanged)
    Q_PROPERTY(bool measureFps READ measureFps WRITE setMeasureFps NOTIFY measureFpsChanged REVISION 1)
    Q_PROPERTY(qreal currentFps READ currentFps NOTIFY currentFpsChanged REVISION 1)
    Q_PROPERTY(QQmlListProperty<QCustom3DItem> customItemList READ customItemList REVISION 1)
    Q_PROPERTY(bool orthoProjection READ isOrthoProjection WRITE setOrthoProjection NOTIFY orthoProjectionChanged REVISION 1)
    Q_PROPERTY(ElementType selectedElement READ selectedElement NOTIFY selectedElementChanged REVISION 1)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY aspectRatioChanged REVISION 1)
    Q_PROPERTY(OptimizationHints optimizationHints READ optimizationHints WRITE setOptimizationHints NOTIFY optimizationHintsChanged REVISION 1)
    Q_PROPERTY(bool polar READ isPolar WRITE setPolar NOTIFY polarChanged REVISION 2)
    Q_PROPERTY(float radialLabelOffset READ radialLabelOffset WRITE setRadialLabelOffset NOTIFY radialLabelOffsetChanged REVISION 2)
    Q_PROPERTY(qreal horizontalAspectRatio READ horizontalAspectRatio WRITE setHorizontalAspectRatio NOTIFY horizontalAspectRatioChanged REVISION 2)
    Q_PROPERTY(bool reflection READ isReflection WRITE setReflection NOTIFY reflectionChanged REVISION 2)
    Q_PROPERTY(qreal reflectivity READ reflectivity WRITE setReflectivity NOTIFY reflectivityChanged REVISION 2)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged REVISION 2)
    Q_PROPERTY(QVector3D queriedGraphPosition READ queriedGraphPosition NOTIFY queriedGraphPositionChanged REVISION 2)
    Q_PROPERTY(qreal margin READ margin WRITE setMargin NOTIFY marginChanged REVISION 2)

public:
    enum SelectionFlag {
        SelectionNone              = 0,
        SelectionItem              = 1,
        SelectionRow               = 2,
        SelectionItemAndRow        = SelectionItem | SelectionRow,
        SelectionColumn            = 4,
        SelectionItemAndColumn     = SelectionItem | SelectionColumn,
        SelectionRowAndColumn      = SelectionRow | SelectionColumn,
        SelectionItemRowAndColumn  = SelectionItem | SelectionRow | SelectionColumn,
        SelectionSlice             = 8,
        SelectionMultiSeries       = 16
    };
    Q_DECLARE_FLAGS(SelectionFlags, SelectionFlag)

    enum ShadowQuality {
        ShadowQualityNone = 0,
        ShadowQualityLow,
        ShadowQualityMedium,
        ShadowQualityHigh,
        ShadowQualitySoftLow,
        ShadowQualitySoftMedium,
        ShadowQualitySoftHigh
    };

    enum ElementType {
        ElementNone = 0,
        ElementSeries,
        ElementAxisXLabel,
        ElementAxisYLabel,
        ElementAxisZLabel,
        ElementCustomItem
    };

    enum RenderingMode {
        RenderDirectToBackground = 0,
        RenderDirectToBackground_NoClear,
        RenderIndirect
    };

    enum OptimizationHint {
        OptimizationDefault = 0,
        OptimizationStatic  = 1
    };
    Q_DECLARE_FLAGS(OptimizationHints, OptimizationHint)

public:
    explicit AbstractDeclarative(QQuickItem *parent = 0);
    virtual ~AbstractDeclarative();

    virtual void setRenderingMode(RenderingMode mode);
    virtual AbstractDeclarative::RenderingMode renderingMode() const;

    virtual void setSelectionMode(SelectionFlags mode);
    virtual AbstractDeclarative::SelectionFlags selectionMode() const;

    virtual void setShadowQuality(ShadowQuality quality);
    virtual AbstractDeclarative::ShadowQuality shadowQuality() const;

    virtual AbstractDeclarative::ElementType selectedElement() const;

    virtual bool shadowsSupported() const;

    virtual void setMsaaSamples(int samples);
    virtual int msaaSamples() const;

    virtual Declarative3DScene *scene() const;

    virtual QAbstract3DInputHandler *inputHandler() const;
    virtual void setInputHandler(QAbstract3DInputHandler *inputHandler);

    virtual void setTheme(Q3DTheme *theme);
    virtual Q3DTheme *theme() const;

    Q_INVOKABLE virtual void clearSelection();

    Q_REVISION(1) Q_INVOKABLE virtual int addCustomItem(QCustom3DItem *item);
    Q_REVISION(1) Q_INVOKABLE virtual void removeCustomItems();
    Q_REVISION(1) Q_INVOKABLE virtual void removeCustomItem(QCustom3DItem *item);
    Q_REVISION(1) Q_INVOKABLE virtual void removeCustomItemAt(const QVector3D &position);
    Q_REVISION(1) Q_INVOKABLE virtual void releaseCustomItem(QCustom3DItem *item);

    Q_REVISION(1) Q_INVOKABLE virtual int selectedLabelIndex() const;
    Q_REVISION(1) Q_INVOKABLE virtual QAbstract3DAxis *selectedAxis() const;

    Q_REVISION(1) Q_INVOKABLE virtual int selectedCustomItemIndex() const;
    Q_REVISION(1) Q_INVOKABLE virtual QCustom3DItem *selectedCustomItem() const;

    QQmlListProperty<QCustom3DItem> customItemList();
    static void appendCustomItemFunc(QQmlListProperty<QCustom3DItem> *list,
                                     QCustom3DItem *item);
    static int countCustomItemFunc(QQmlListProperty<QCustom3DItem> *list);
    static QCustom3DItem *atCustomItemFunc(QQmlListProperty<QCustom3DItem> *list, int index);
    static void clearCustomItemFunc(QQmlListProperty<QCustom3DItem> *list);

    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);

    void setSharedController(Abstract3DController *controller);
    // Used to synch up data model from controller to renderer while main thread is locked
    void synchDataToRenderer();
    void render();

    void activateOpenGLContext(QQuickWindow *window);
    void doneOpenGLContext(QQuickWindow *window);

    void checkWindowList(QQuickWindow *window);

    void setMeasureFps(bool enable);
    bool measureFps() const;
    qreal currentFps() const;

    void setOrthoProjection(bool enable);
    bool isOrthoProjection() const;

    void setAspectRatio(qreal ratio);
    qreal aspectRatio() const;

    void setOptimizationHints(OptimizationHints hints);
    OptimizationHints optimizationHints() const;

    void setPolar(bool enable);
    bool isPolar() const;

    void setRadialLabelOffset(float offset);
    float radialLabelOffset() const;

    void setHorizontalAspectRatio(qreal ratio);
    qreal horizontalAspectRatio() const;

    void setReflection(bool enable);
    bool isReflection() const;

    void setReflectivity(qreal reflectivity);
    qreal reflectivity() const;

    void setLocale(const QLocale &locale);
    QLocale locale() const;

    QVector3D queriedGraphPosition() const;

    void setMargin(qreal margin);
    qreal margin() const;

public Q_SLOTS:
    virtual void handleAxisXChanged(QAbstract3DAxis *axis) = 0;
    virtual void handleAxisYChanged(QAbstract3DAxis *axis) = 0;
    virtual void handleAxisZChanged(QAbstract3DAxis *axis) = 0;
    void windowDestroyed(QObject *obj);
    void destroyContext();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void touchEvent(QTouchEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void handleWindowChanged(QQuickWindow *win);
    virtual void itemChange(ItemChange change, const ItemChangeData &value);
    virtual void updateWindowParameters();
    virtual void handleSelectionModeChange(QAbstract3DGraph::SelectionFlags mode);
    virtual void handleShadowQualityChange(QAbstract3DGraph::ShadowQuality quality);
    virtual void handleSelectedElementChange(QAbstract3DGraph::ElementType type);
    virtual void handleOptimizationHintChange(QAbstract3DGraph::OptimizationHints hints);
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

Q_SIGNALS:
    void selectionModeChanged(AbstractDeclarative::SelectionFlags mode);
    void shadowQualityChanged(AbstractDeclarative::ShadowQuality quality);
    void shadowsSupportedChanged(bool supported);
    void msaaSamplesChanged(int samples);
    void sceneChanged(Q3DScene *scene);
    void inputHandlerChanged(QAbstract3DInputHandler *inputHandler);
    void themeChanged(Q3DTheme *theme);
    void renderingModeChanged(AbstractDeclarative::RenderingMode mode);
    Q_REVISION(1) void measureFpsChanged(bool enabled);
    Q_REVISION(1) void currentFpsChanged(qreal fps);
    Q_REVISION(1) void selectedElementChanged(AbstractDeclarative::ElementType type);
    Q_REVISION(1) void orthoProjectionChanged(bool enabled);
    Q_REVISION(1) void aspectRatioChanged(qreal ratio);
    Q_REVISION(1) void optimizationHintsChanged(AbstractDeclarative::OptimizationHints hints);
    Q_REVISION(2) void polarChanged(bool enabled);
    Q_REVISION(2) void radialLabelOffsetChanged(float offset);
    Q_REVISION(2) void horizontalAspectRatioChanged(qreal ratio);
    Q_REVISION(2) void reflectionChanged(bool enabled);
    Q_REVISION(2) void reflectivityChanged(qreal reflectivity);
    Q_REVISION(2) void localeChanged(const QLocale &locale);
    Q_REVISION(2) void queriedGraphPositionChanged(const QVector3D &data);
    Q_REVISION(2) void marginChanged(qreal margin);

protected:
    QSharedPointer<QMutex> m_nodeMutex;

private:
    QPointer<Abstract3DController> m_controller;
    QRectF m_cachedGeometry;
    QPointer<QQuickWindow> m_contextWindow;
    AbstractDeclarative::RenderingMode m_renderMode;
    int m_samples;
    int m_windowSamples;
    QSize m_initialisedSize;
#ifdef USE_SHARED_CONTEXT
    QOpenGLContext *m_context;
#else
    GLStateStore *m_stateStore;
#endif
    QPointer<QOpenGLContext> m_qtContext;
    QThread *m_mainThread;
    QThread *m_contextThread;
    bool m_runningInDesigner;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractDeclarative::SelectionFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractDeclarative::OptimizationHints)

QT_END_NAMESPACE_DATAVISUALIZATION

#endif
