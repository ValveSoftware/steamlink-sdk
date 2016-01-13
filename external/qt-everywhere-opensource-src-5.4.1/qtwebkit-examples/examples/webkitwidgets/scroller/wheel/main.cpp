/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QtWidgets>
#include <qmath.h>

#include "wheelwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(bool touch)
        : QMainWindow()
    {
        makeSlotMachine(touch);
        setCentralWidget(m_slotMachine);
    }

    void makeSlotMachine(bool touch)
    {
        if (QApplication::desktop()->width() > 1000) {
            QFont f = font();
            f.setPointSize(f.pointSize() * 2);
            setFont(f);
        }

        m_slotMachine = new QWidget(this);
        QGridLayout *grid = new QGridLayout(m_slotMachine);
        grid->setSpacing(20);

        QStringList colors;
        colors << "Red" << "Magenta" << "Peach" << "Orange" << "Yellow" << "Citro" << "Green" << "Cyan" << "Blue" << "Violet";

        m_wheel1 = new StringWheelWidget(touch);
        m_wheel1->setItems( colors );
        grid->addWidget( m_wheel1, 0, 0 );

        m_wheel2 = new StringWheelWidget(touch);
        m_wheel2->setItems( colors );
        grid->addWidget( m_wheel2, 0, 1 );

        m_wheel3 = new StringWheelWidget(touch);
        m_wheel3->setItems( colors );
        grid->addWidget( m_wheel3, 0, 2 );

        QPushButton *shakeButton = new QPushButton(tr("Shake"));
        connect(shakeButton, SIGNAL(clicked()), this, SLOT(rotateRandom()));

        grid->addWidget( shakeButton, 1, 0, 1, 3 );
    }

private slots:
    void rotateRandom()
    {
        m_wheel1->scrollTo(m_wheel1->currentIndex() + (qrand() % 200));
        m_wheel2->scrollTo(m_wheel2->currentIndex() + (qrand() % 200));
        m_wheel3->scrollTo(m_wheel3->currentIndex() + (qrand() % 200));
    }

private:
    QWidget *m_slotMachine;

    StringWheelWidget *m_wheel1;
    StringWheelWidget *m_wheel2;
    StringWheelWidget *m_wheel3;
};

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    bool touch = a.arguments().contains(QLatin1String("--touch"));
    MainWindow mw(touch);
#ifdef Q_WS_S60
    mw.showMaximized();
#else
    mw.show();
#endif
#ifdef Q_WS_MAC
    mw.raise();
#endif
    return a.exec();
}

#include "main.moc"
