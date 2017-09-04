/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qscreen.h>
#include <private/qsgrendernode_p.h>

#include "../../shared/util.h"

class tst_rendernode: public QQmlDataTest
{
    Q_OBJECT
public:
    tst_rendernode();

    QImage runTest(const QString &fileName)
    {
        QQuickView view(&outerWindow);
        view.setResizeMode(QQuickView::SizeViewToRootObject);
        view.setSource(testFileUrl(fileName));
        view.setVisible(true);
        QTest::qWaitForWindowExposed(&view);
        return view.grabWindow();
    }

    //It is important for platforms that only are able to show fullscreen windows
    //to have a container for the window that is painted on.
    QQuickWindow outerWindow;

private slots:
    void renderOrder();
    void messUpState();
    void matrix();
};

class ClearNode : public QSGRenderNode
{
public:
    StateFlags changedStates() const override
    {
        return ColorState;
    }

    void render(const RenderState *) override
    {
        // If clip has been set, scissoring will make sure the right area is cleared.
        QOpenGLContext::currentContext()->functions()->glClearColor(color.redF(), color.greenF(), color.blueF(), 1.0f);
        QOpenGLContext::currentContext()->functions()->glClear(GL_COLOR_BUFFER_BIT);
    }

    QColor color;
};

class ClearItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    ClearItem() : m_color(Qt::black)
    {
        setFlag(ItemHasContents, true);
    }

    QColor color() const { return m_color; }
    void setColor(const QColor &color)
    {
        if (color == m_color)
            return;
        m_color = color;
        emit colorChanged();
    }

protected:
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
    {
        ClearNode *node = static_cast<ClearNode *>(oldNode);
        if (!node)
            node = new ClearNode;
        node->color = m_color;
        return node;
    }

Q_SIGNALS:
    void colorChanged();

private:
    QColor m_color;
};

class MessUpNode : public QSGRenderNode, protected QOpenGLFunctions
{
public:
    MessUpNode() : initialized(false) { }

    StateFlags changedStates() const override
    {
        return StateFlags(DepthState) | StencilState | ScissorState | ColorState | BlendState
                | CullState | ViewportState | RenderTargetState;
    }

    void render(const RenderState *) override
    {
        if (!initialized) {
            initializeOpenGLFunctions();
            initialized = true;
        }
        // Don't draw anything, just mess up the state
        glViewport(10, 10, 10, 10);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(true);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_EQUAL);
        glClearDepthf(1);
        glClearStencil(42);
        glClearColor(1.0f, 0.5f, 1.0f, 0.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(190, 190, 10, 10);
        glStencilFunc(GL_EQUAL, 28, 0xff);
        glBlendFunc(GL_ZERO, GL_ZERO);
        GLint frontFace;
        glGetIntegerv(GL_FRONT_FACE, &frontFace);
        glFrontFace(frontFace == GL_CW ? GL_CCW : GL_CW);
        glEnable(GL_CULL_FACE);
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    bool initialized;
};

class MessUpItem : public QQuickItem
{
    Q_OBJECT
public:
    MessUpItem()
    {
        setFlag(ItemHasContents, true);
    }

protected:
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
    {
        MessUpNode *node = static_cast<MessUpNode *>(oldNode);
        if (!node)
            node = new MessUpNode;
        return node;
    }
};

tst_rendernode::tst_rendernode()
{
    qmlRegisterType<ClearItem>("Test", 1, 0, "ClearItem");
    qmlRegisterType<MessUpItem>("Test", 1, 0, "MessUpItem");
    outerWindow.showNormal();
    outerWindow.setGeometry(0,0,400,400);
}

static bool fuzzyCompareColor(QRgb x, QRgb y, QByteArray *errorMessage)
{
    enum { fuzz = 4 };
    if (qAbs(qRed(x) - qRed(y)) >= fuzz || qAbs(qGreen(x) - qGreen(y)) >= fuzz || qAbs(qBlue(x) - qBlue(y)) >= fuzz) {
        QString s;
        QDebug(&s).nospace() << hex << "Color mismatch 0x" << x << " 0x" << y << dec << " (fuzz=" << fuzz << ").";
        *errorMessage = s.toLocal8Bit();
        return false;
    }
    return true;
}

static inline QByteArray msgColorMismatchAt(const QByteArray &colorMsg, int x, int y)
{
    return colorMsg + QByteArrayLiteral(" at ") + QByteArray::number(x) +',' + QByteArray::number(y);
}

/* The test draws four rects, each 100x100 and verifies
 * that a rendernode which calls glClear() is stacked
 * correctly. The red rectangles come under the white
 * and are obscured.
 */
void tst_rendernode::renderOrder()
{
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("RenderOrder.qml");

    const qreal scaleFactor = QGuiApplication::primaryScreen()->devicePixelRatio();
    QCOMPARE(fb.width(), qRound(200 * scaleFactor));
    QCOMPARE(fb.height(), qRound(200 * scaleFactor));

    QCOMPARE(fb.pixel(50 * scaleFactor, 50 * scaleFactor), qRgb(0xff, 0xff, 0xff));
    QCOMPARE(fb.pixel(50 * scaleFactor, 150 * scaleFactor), qRgb(0xff, 0xff, 0xff));
    QCOMPARE(fb.pixel(150 * scaleFactor, 50 * scaleFactor), qRgb(0x00, 0x00, 0xff));

    QByteArray errorMessage;
    const qreal coordinate = 150 * scaleFactor;
    QVERIFY2(fuzzyCompareColor(fb.pixel(coordinate, coordinate), qRgb(0x7f, 0x7f, 0xff), &errorMessage),
             msgColorMismatchAt(errorMessage, coordinate, coordinate).constData());
}

/* The test uses a number of nested rectangles with clipping
 * and rotation to verify that using a render node which messes
 * with the state does not break rendering that comes after it.
 */
void tst_rendernode::messUpState()
{
    if (QGuiApplication::primaryScreen()->depth() < 24)
        QSKIP("This test does not work at display depths < 24");
    QImage fb = runTest("MessUpState.qml");
    int x1 = 0;
    int x2 = fb.width() / 2;
    int x3 = fb.width() - 1;
    int y1 = 0;
    int y2 = fb.height() * 3 / 16;
    int y3 = fb.height() / 2;
    int y4 = fb.height() * 13 / 16;
    int y5 = fb.height() - 1;

    QCOMPARE(fb.pixel(x1, y3), qRgb(0xff, 0xff, 0xff));
    QCOMPARE(fb.pixel(x3, y3), qRgb(0xff, 0xff, 0xff));

    QCOMPARE(fb.pixel(x2, y1), qRgb(0x00, 0x00, 0x00));
    QCOMPARE(fb.pixel(x2, y2), qRgb(0x00, 0x00, 0x00));
    QByteArray errorMessage;
    QVERIFY2(fuzzyCompareColor(fb.pixel(x2, y3), qRgb(0x7f, 0x00, 0x7f), &errorMessage),
             msgColorMismatchAt(errorMessage, x2, y3).constData());
    QCOMPARE(fb.pixel(x2, y4), qRgb(0x00, 0x00, 0x00));
    QCOMPARE(fb.pixel(x2, y5), qRgb(0x00, 0x00, 0x00));
}

class StateRecordingRenderNode : public QSGRenderNode
{
public:
    StateFlags changedStates() const override { return StateFlags(-1); }
    void render(const RenderState *) override {
        matrices[name] = *matrix();

    }

    QString name;
    static QHash<QString, QMatrix4x4> matrices;
};

QHash<QString, QMatrix4x4> StateRecordingRenderNode::matrices;

class StateRecordingRenderNodeItem : public QQuickItem
{
    Q_OBJECT
public:
    StateRecordingRenderNodeItem() { setFlag(ItemHasContents, true); }
    QSGNode *updatePaintNode(QSGNode *r, UpdatePaintNodeData *) {
        if (r)
            return r;
        StateRecordingRenderNode *rn = new StateRecordingRenderNode();
        rn->name = objectName();
        return rn;
    }
};

void tst_rendernode::matrix()
{
    qmlRegisterType<StateRecordingRenderNodeItem>("RenderNode", 1, 0, "StateRecorder");
    StateRecordingRenderNode::matrices.clear();
    runTest("matrix.qml");

    QMatrix4x4 noRotateOffset;
    noRotateOffset.translate(20, 20);
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("no-clip; no-rotation"));
        QCOMPARE(result, noRotateOffset);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("parent-clip; no-rotation"));
        QCOMPARE(result, noRotateOffset);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("self-clip; no-rotation"));
        QCOMPARE(result, noRotateOffset);
    }

    QMatrix4x4 parentRotation;
    parentRotation.translate(10, 10);   // parent at x/y: 10
    parentRotation.translate(5, 5);     // rotate 90 around center (width/height: 10)
    parentRotation.rotate(90, 0, 0, 1);
    parentRotation.translate(-5, -5);
    parentRotation.translate(10, 10);   // StateRecorder at: x/y: 10
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("no-clip; parent-rotation"));
        QCOMPARE(result, parentRotation);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("parent-clip; parent-rotation"));
        QCOMPARE(result, parentRotation);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("self-clip; parent-rotation"));
        QCOMPARE(result, parentRotation);
    }

    QMatrix4x4 selfRotation;
    selfRotation.translate(10, 10);   // parent at x/y: 10
    selfRotation.translate(10, 10);   // StateRecorder at: x/y: 10
    selfRotation.rotate(90, 0, 0, 1); // rotate 90, width/height: 0
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("no-clip; self-rotation"));
        QCOMPARE(result, selfRotation);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("parent-clip; self-rotation"));
        QCOMPARE(result, selfRotation);
    }
    {   QMatrix4x4 result = StateRecordingRenderNode::matrices.value(QStringLiteral("self-clip; self-rotation"));
        QCOMPARE(result, selfRotation);
    }
}


QTEST_MAIN(tst_rendernode)

#include "tst_rendernode.moc"
