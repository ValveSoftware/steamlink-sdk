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

#ifndef OBJECTS_H
#define OBJECTS_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE
class QSubWidget;

//! [0]
class QParentWidget : public QWidget
{
    Q_OBJECT
    Q_CLASSINFO("ClassID", "{d574a747-8016-46db-a07c-b2b4854ee75c}");
    Q_CLASSINFO("InterfaceID", "{4a30719d-d9c2-4659-9d16-67378209f822}");
    Q_CLASSINFO("EventsID", "{4a30719d-d9c2-4659-9d16-67378209f823}");
public:
    QParentWidget(QWidget *parent = 0);

    QSize sizeHint() const;

public slots:
    void createSubWidget( const QString &name );

    QSubWidget *subWidget( const QString &name );

private:
    QVBoxLayout *vbox;
};
//! [0]

//! [1]
class QSubWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY( QString label READ label WRITE setLabel )

    Q_CLASSINFO("ClassID", "{850652f4-8f71-4f69-b745-bce241ccdc30}");
    Q_CLASSINFO("InterfaceID", "{2d76cc2f-3488-417a-83d6-debff88b3c3f}");
    Q_CLASSINFO("ToSuperClass", "QSubWidget");

public:
    QSubWidget(QWidget *parent = 0, const QString &name = QString());

    void setLabel( const QString &text );
    QString label() const;

    QSize sizeHint() const;

protected:
    void paintEvent( QPaintEvent *e );

private:
    QString lbl;
};
//! [1]

#endif // OBJECTS_H
