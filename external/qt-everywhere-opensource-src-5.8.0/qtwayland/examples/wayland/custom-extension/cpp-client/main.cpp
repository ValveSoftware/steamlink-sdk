/****************************************************************************
 **
 ** Copyright (C) 2015 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** This file is part of the examples of the Qt Wayland module
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
#include <QRasterWindow>
#include <QPainter>
#include <QMouseEvent>
#include <QPlatformSurfaceEvent>

#include "../client-common/customextension.h"

#include <QDebug>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QTimer>

class TestWindow : public QRasterWindow
{
    Q_OBJECT

public:
    TestWindow(CustomExtension *customExtension)
        : m_extension(customExtension)
        , rect1(50, 50, 100, 100)
        , rect2(50, 200, 100, 100)
        , rect3(50, 350, 100, 100)
    {
        m_extension->registerWindow(this);
        connect(m_extension, &CustomExtension::fontSize, this, &TestWindow::handleSetFontSize);
    }

public slots:
    void doSpin()
    {
        if (!m_extension->isActive()) {
            qWarning() << "Extension is not active";
            return;
        }
        qDebug() << "sending spin...";
        m_extension->sendSpin(this, 500);
    }
    void doBounce()
    {
        if (!m_extension->isActive()) {
            qWarning() << "Extension is not active";
            return;
        }
        qDebug() << "sending bounce...";
        m_extension->sendBounce(this, 500);
    }

    void newWindow()
    {
        auto w = new TestWindow(m_extension);
        w->show();
    }

    void handleSetFontSize(QWindow *w, uint pixelSize)
    {
        if (w == this) {
            m_font.setPixelSize(pixelSize);
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE
    {
        QPainter p(this);
        p.setFont(m_font);
        p.fillRect(QRect(0,0,width(),height()),Qt::gray);
        p.fillRect(rect1, QColor("#C0FFEE"));
        p.drawText(rect1, Qt::TextWordWrap, "Press here to send spin request.");
        p.fillRect(rect2, QColor("#decaff"));
        p.drawText(rect2, Qt::TextWordWrap, "Press here to send bounce request.");
        p.fillRect(rect3, QColor("#7EA"));
        p.drawText(rect3, Qt::TextWordWrap, "Create new window.");
    }

    void mousePressEvent(QMouseEvent *ev) Q_DECL_OVERRIDE
    {
        if (rect1.contains(ev->pos()))
            doSpin();
        else if (rect2.contains(ev->pos()))
            doBounce();
        else if (rect3.contains(ev->pos()))
            newWindow();
    }

private:
    CustomExtension *m_extension;
    QRect rect1;
    QRect rect2;
    QRect rect3;
    QFont m_font;
};

int main (int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    CustomExtension customExtension;

    TestWindow window(&customExtension);
    window.show();

    return app.exec();
}

#include "main.moc"
