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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <qobject.h>
#include <qlist.h>
#include <qhash.h>
#include <QTimerEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScriptEngine>
#include <QScriptable>
class QContext2DCanvas;

//! [0]
class Environment : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QScriptValue document READ document)
public:
    Environment(QObject *parent = 0);
    ~Environment();

    QScriptValue document() const;

    void addCanvas(QContext2DCanvas *canvas);
    QContext2DCanvas *canvasByName(const QString &name) const;
    QList<QContext2DCanvas*> canvases() const;

    QScriptValue evaluate(const QString &code,
                          const QString &fileName = QString());

    QScriptValue toWrapper(QObject *object);

    void handleEvent(QContext2DCanvas *canvas, QMouseEvent *e);
    void handleEvent(QContext2DCanvas *canvas, QKeyEvent *e);

    void reset();
//! [0]

    QScriptEngine *engine() const;
    bool hasIntervalTimers() const;
    void triggerTimers();

//! [1]
public slots:
    int setInterval(const QScriptValue &expression, int delay);
    void clearInterval(int timerId);

    int setTimeout(const QScriptValue &expression, int delay);
    void clearTimeout(int timerId);
//! [1]

//! [2]
signals:
    void scriptError(const QScriptValue &error);
//! [2]

protected:
    void timerEvent(QTimerEvent *event);

private:
    QScriptValue eventHandler(QContext2DCanvas *canvas,
                              const QString &type, QScriptValue *who);
    QScriptValue newFakeDomEvent(const QString &type,
                                 const QScriptValue &target);
    void maybeEmitScriptError();

    QScriptEngine *m_engine;
    QScriptValue m_originalGlobalObject;
    QScriptValue m_document;
    QList<QContext2DCanvas*> m_canvases;
    QHash<int, QScriptValue> m_intervalHash;
    QHash<int, QScriptValue> m_timeoutHash;
};

//! [3]
class Document : public QObject
{
    Q_OBJECT
public:
    Document(Environment *env);
    ~Document();

public slots:
    QScriptValue getElementById(const QString &id) const;
    QScriptValue getElementsByTagName(const QString &name) const;

    // EventTarget
    void addEventListener(const QString &type, const QScriptValue &listener,
                          bool useCapture);
};
//! [3]

class CanvasGradientPrototype : public QObject, public QScriptable
{
    Q_OBJECT
protected:
    CanvasGradientPrototype(QObject *parent = 0);
public:
    static void setup(QScriptEngine *engine);

public slots:
    void addColorStop(qreal offset, const QString &color);
};

#endif
