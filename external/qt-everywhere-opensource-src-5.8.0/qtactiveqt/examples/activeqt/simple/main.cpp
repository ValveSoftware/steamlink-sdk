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
#include <QApplication>
#include <QLayout>
#include <QSlider>
#include <QLCDNumber>
#include <QLineEdit>
#include <QMessageBox>

//! [0]
class QSimpleAX : public QWidget, public QAxBindable
{
    Q_OBJECT
    Q_CLASSINFO("ClassID",     "{DF16845C-92CD-4AAB-A982-EB9840E74669}")
    Q_CLASSINFO("InterfaceID", "{616F620B-91C5-4410-A74E-6B81C76FFFE0}")
    Q_CLASSINFO("EventsID",    "{E1816BBA-BF5D-4A31-9855-D6BA432055FF}")
    Q_PROPERTY( QString text READ text WRITE setText )
    Q_PROPERTY( int value READ value WRITE setValue )
public:
    QSimpleAX(QWidget *parent = 0)
    : QWidget(parent)
    {
        QVBoxLayout *vbox = new QVBoxLayout( this );

        slider = new QSlider( Qt::Horizontal, this );
        LCD = new QLCDNumber( 3, this );
        edit = new QLineEdit( this );

        connect( slider, &QAbstractSlider::valueChanged, this, &QSimpleAX::setValue );
        connect( edit, &QLineEdit::textChanged, this, &QSimpleAX::setText );

        vbox->addWidget( slider );
        vbox->addWidget( LCD );
        vbox->addWidget( edit );
    }

    QString text() const
    {
        return edit->text();
    }
    int value() const
    {
        return slider->value();
    }

signals:
    void someSignal();
    void valueChanged(int);
    void textChanged(const QString&);

public slots:
    void setText( const QString &string )
    {
        if ( !requestPropertyChange( "text" ) )
            return;

        edit->blockSignals( true );
        edit->setText( string );
        edit->blockSignals( false );
        emit someSignal();
        emit textChanged( string );

        propertyChanged( "text" );
    }
    void about()
    {
        QMessageBox::information( this, "About QSimpleAX", "This is a Qt widget, and this slot has been\n"
                                                          "called through ActiveX/OLE automation!" );
    }
    void setValue( int i )
    {
        if ( !requestPropertyChange( "value" ) )
            return;
        slider->blockSignals( true );
        slider->setValue( i );
        slider->blockSignals( false );
        LCD->display( i );
        emit valueChanged( i );

        propertyChanged( "value" );
    }

private:
    QSlider *slider;
    QLCDNumber *LCD;
    QLineEdit *edit;
};

//! [0]
#include "main.moc"

//! [1]
QAXFACTORY_BEGIN(
    "{EC08F8FC-2754-47AB-8EFE-56A54057F34E}", // type library ID
    "{A095BA0C-224F-4933-A458-2DD7F6B85D8F}") // application ID
    QAXCLASS(QSimpleAX)
QAXFACTORY_END()
//! [1]
