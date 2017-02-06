/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#ifndef XBEL_H
#define XBEL_H

#include <QtCore/QXmlStreamReader>
#include <QtCore/QDateTime>

class BookmarkNode
{
public:
    enum Type {
        Root,
        Folder,
        Bookmark,
        Separator
    };

    BookmarkNode(Type type = Root, BookmarkNode *parent = 0);
    ~BookmarkNode();
    bool operator==(const BookmarkNode &other);

    Type type() const;
    void setType(Type type);
    QList<BookmarkNode *> children() const;
    BookmarkNode *parent() const;

    void add(BookmarkNode *child, int offset = -1);
    void remove(BookmarkNode *child);

    QString url;
    QString title;
    QString desc;
    bool expanded;

private:
    BookmarkNode *m_parent;
    Type m_type;
    QList<BookmarkNode *> m_children;

};

class XbelReader : public QXmlStreamReader
{
public:
    XbelReader();
    BookmarkNode *read(const QString &fileName);
    BookmarkNode *read(QIODevice *device);

private:
    void readXBEL(BookmarkNode *parent);
    void readTitle(BookmarkNode *parent);
    void readDescription(BookmarkNode *parent);
    void readSeparator(BookmarkNode *parent);
    void readFolder(BookmarkNode *parent);
    void readBookmarkNode(BookmarkNode *parent);
};

#include <QtCore/QXmlStreamWriter>

class XbelWriter : public QXmlStreamWriter
{
public:
    XbelWriter();
    bool write(const QString &fileName, const BookmarkNode *root);
    bool write(QIODevice *device, const BookmarkNode *root);

private:
    void writeItem(const BookmarkNode *parent);
};

#endif // XBEL_H
