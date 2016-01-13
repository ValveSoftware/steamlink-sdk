/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QWidget>

#include <qmacpasteboardmime.h>

class VCardMime : public QMacPasteboardMime
{
public:
    VCardMime() : QMacPasteboardMime(MIME_ALL)
    { }

    QString convertorName()
    {
        return QString("VCardMime");
    }

    bool canConvert(const QString &mime, QString flav)
    {
        return mimeFor(flav) == mime;
    }

    QString mimeFor(QString flav)
    {
        if (flav == QString("public.vcard"))
            return QString("application/x-mycompany-VCard");
        return QString();
    }

    QString flavorFor(const QString &mime)
    {
        if (mime == QString("application/x-mycompany-VCard"))
            return QString("public.vcard");
        return QString();
    }

    QVariant convertToMime(const QString &mime, QList<QByteArray> data, QString flav)
    {
        Q_UNUSED(mime);
        Q_UNUSED(flav);

        QByteArray all;
        foreach (QByteArray i, data) {
            all += i;
        }
        return QVariant(all);
    }

    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString flav)
    {
        Q_UNUSED(mime);
        Q_UNUSED(data);
        Q_UNUSED(flav);
        // Todo: implement!
        return QList<QByteArray>();
    }

};

class TestWidget : public QWidget
{
public:
    TestWidget() : QWidget(0)
    {
        vcardMime = new VCardMime();
        setAcceptDrops(true);
    }

    ~TestWidget()
    {
        delete vcardMime;
    }

    void dragEnterEvent(QDragEnterEvent *e)
    {
        e->accept();

    }

    virtual void dropEvent(QDropEvent *e)
    {
        e->accept();
        contentsDropEvent(e);
    }

    void contentsDropEvent(QDropEvent* e)
    {
        if ( e->mimeData()->hasFormat( "application/x-mycompany-VCard" ) )
        {
            QString s = QString( e->mimeData()->data( "application/x-mycompany-VCard" ) );

            // s now contains text of vcard
            qDebug() << "got vcard" << s.count();

            e->acceptProposedAction();
        }
    }
private:
    VCardMime *vcardMime;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    qRegisterDraggedTypes(QStringList() << QLatin1String("public.vcard"));

    TestWidget wid1;
    wid1.show();

    return app.exec();
}
