/****************************************************************************
**
** Copyright (C) 2015 LG Electronics Inc, author: <mikko.levonmaa@lge.com>
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

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlFileSelector>
#include <QQmlContext>
#include <QQuickView>

#include <QtPlatformHeaders/qwaylandwindowfunctions.h>

#include "shmwindow.h"

class Filter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sync READ getSync NOTIFY syncChanged)

public:
    Filter()
    {
        sync = false;
    }

    bool eventFilter(QObject *object, QEvent *event)
    {
        Q_UNUSED(object);
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Space) {
                toggleSync(quick);
                toggleSync(shm);
            }
        }
        return false;
    }

    void toggleSync(QWindow *w)
    {
        sync = !QWaylandWindowFunctions::isSync(w);
        if (QWaylandWindowFunctions::isSync(w))
            QWaylandWindowFunctions::setDeSync(w);
        else
            QWaylandWindowFunctions::setSync(w);
        emit syncChanged();
    }

    bool getSync() const
    {
        return sync;
    }

Q_SIGNALS:
    void syncChanged();

public:
    QWindow *quick;
    QWindow *shm;
    bool sync;
};

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QQuickView view;
    view.connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));
    view.setResizeMode(QQuickView::SizeRootObjectToView);

    Filter f;
    view.rootContext()->setContextProperty("syncStatus", &f);
    view.installEventFilter(&f);

    view.setSource(QUrl("qrc:/main.qml"));
    view.show();

    QQuickView child(&view);
    child.connect(child.engine(), SIGNAL(quit()), &app, SLOT(quit()));
    child.setSource(QUrl("qrc:/child.qml"));
    child.setResizeMode(QQuickView::SizeRootObjectToView);
    child.setGeometry(QRect(150, 70, 100, 100));
    child.show();

    ShmWindow shm(&view);
    shm.setGeometry(QRect(30, 30, 50, 50));
    shm.show();

    f.quick = &child;
    f.shm = &shm;

    return app.exec();
}

#include "main.moc"

