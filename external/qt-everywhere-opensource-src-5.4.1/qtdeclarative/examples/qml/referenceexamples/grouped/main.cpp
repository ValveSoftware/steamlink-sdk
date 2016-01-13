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
#include <QCoreApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>
#include "birthdayparty.h"
#include "person.h"

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);

    qmlRegisterType<BirthdayParty>("People", 1,0, "BirthdayParty");
    qmlRegisterType<ShoeDescription>();
    qmlRegisterType<Person>();
    qmlRegisterType<Boy>("People", 1,0, "Boy");
    qmlRegisterType<Girl>("People", 1,0, "Girl");

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:example.qml"));
    BirthdayParty *party = qobject_cast<BirthdayParty *>(component.create());

    if (party && party->host()) {
        qWarning() << party->host()->name() << "is having a birthday!";

        if (qobject_cast<Boy *>(party->host()))
            qWarning() << "He is inviting:";
        else
            qWarning() << "She is inviting:";

        Person *bestShoe = 0;
        for (int ii = 0; ii < party->guestCount(); ++ii) {
            Person *guest = party->guest(ii);
            qWarning() << "   " << guest->name();

            if (!bestShoe || bestShoe->shoe()->price() < guest->shoe()->price())
                bestShoe = guest;
        }
        if (bestShoe)
            qWarning() << bestShoe->name() << "is wearing the best shoes!";

    } else {
        qWarning() << component.errors();
    }

    return 0;
}
