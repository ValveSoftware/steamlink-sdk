/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <QAxBindable>
#include <QAxFactory>
#include <QMainWindow>
#include <QQuickWidget>
#include <QQmlContext>

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QColor color READ color NOTIFY valueChanged)
public:
    explicit Controller(QWidget *parent = 0) :
        QObject(parent),
        m_value(0)
    { }

    qreal value() const { return m_value; }

    void setValue(qreal value)
    {
        if (value < 0) value = 0;
        if (value > 1) value = 1;

        m_value = value;
        valueChanged();
    }

    QColor color() const
    {
        QColor start = Qt::yellow;
        QColor end = Qt::magenta;

        // Linear interpolation between two colors in HSV space
        return QColor::fromHsvF(
            start.hueF()        * (1.0f - m_value) + end.hueF()        * m_value,
            start.saturationF() * (1.0f - m_value) + end.saturationF() * m_value,
            start.valueF()      * (1.0f - m_value) + end.valueF()      * m_value,
            start.alphaF()      * (1.0f - m_value) + end.alphaF()      * m_value
        );
    }

signals:
    void valueChanged();

private:
    qreal m_value;
};

class QSimpleQmlAx : public QMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("ClassID", "{50477337-58FE-4898-8FFC-6F6199CEAE08}")
    Q_CLASSINFO("InterfaceID", "{A5EC7D99-CEC9-4BD1-8336-ED15A579B185}")
    Q_CLASSINFO("EventsID", "{5BBFBCFD-20FD-48A3-96C7-1F6649CD1F52}")
public:
    explicit QSimpleQmlAx(QWidget *parent = 0) :
        QMainWindow(parent)
    {
        auto ui = new QQuickWidget(this);

        // Register our type to qml
        qmlRegisterType<Controller>("app", 1, 0, "Controller");

        // Initialize view
        ui->rootContext()->setContextProperty("context", QVariant::fromValue(new Controller(this)));
        ui->setMinimumSize(200, 200);
        ui->setResizeMode(QQuickWidget::SizeRootObjectToView);
        ui->setSource(QUrl("qrc:/main.qml"));
        setCentralWidget(ui);
    }
};

#include "main.moc"

QAXFACTORY_BEGIN(
    "{E544E321-EF8B-4CD4-91F6-DB55A59DBADB}", // type library ID
    "{E37E3131-DEA2-44EB-97A2-01CDD09A5A4D}") // application ID
    QAXCLASS(QSimpleQmlAx)
QAXFACTORY_END()
