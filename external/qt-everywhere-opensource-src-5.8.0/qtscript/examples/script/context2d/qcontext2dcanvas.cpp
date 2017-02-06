/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcontext2dcanvas.h"

#include "context2d.h"
#include "environment.h"
#include "domimage.h"

#include <QPainter>
#include <QPaintEvent>

//! [3]
QContext2DCanvas::QContext2DCanvas(Context2D *context, Environment *env, QWidget *parent)
    : QWidget(parent), m_context(context), m_env(env)
{
    QObject::connect(context, SIGNAL(changed(QImage)), this, SLOT(contentsChanged(QImage)));
    setMouseTracking(true);
}
//! [3]

QContext2DCanvas::~QContext2DCanvas()
{
}

Context2D *QContext2DCanvas::context() const
{
    return m_context;
}

//! [0]
QScriptValue QContext2DCanvas::getContext(const QString &str)
{
    if (str != "2d")
        return QScriptValue();
    return m_env->toWrapper(m_context);
}
//! [0]

//! [1]
void QContext2DCanvas::contentsChanged(const QImage &image)
{
    m_image = image;
    update();
}

void QContext2DCanvas::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setClipRect(e->rect());
    p.drawImage(0, 0, m_image);
}
//! [1]

//! [2]
void QContext2DCanvas::mouseMoveEvent(QMouseEvent *e)
{
    m_env->handleEvent(this, e);
}

void QContext2DCanvas::mousePressEvent(QMouseEvent *e)
{
    m_env->handleEvent(this, e);
}

void QContext2DCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    m_env->handleEvent(this, e);
}

void QContext2DCanvas::keyPressEvent(QKeyEvent *e)
{
    m_env->handleEvent(this, e);
}

void QContext2DCanvas::keyReleaseEvent(QKeyEvent *e)
{
    m_env->handleEvent(this, e);
}
//! [2]

void QContext2DCanvas::resizeEvent(QResizeEvent *e)
{
    m_context->setSize(e->size().width(), e->size().height());
}

void QContext2DCanvas::resize(int width, int height)
{
    QWidget::resize(width, height);
}

void QContext2DCanvas::reset()
{
    m_context->reset();
}

void QContext2DCanvas::addEventListener(const QString &type, const QScriptValue &listener,
                                        bool useCapture)
{
    Q_UNUSED(useCapture);
    if (listener.isFunction()) {
        QScriptValue self = m_env->toWrapper(this);
        self.setProperty("on" + type, listener);
    }
}
