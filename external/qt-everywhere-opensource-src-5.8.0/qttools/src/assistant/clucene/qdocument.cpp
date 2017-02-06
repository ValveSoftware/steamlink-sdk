/****************************************************************************
**
** Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team.
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdocument_p.h"
#include "qreader_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/util/Reader.h>
#include <CLucene/document/Document.h>

QT_BEGIN_NAMESPACE

QCLuceneDocumentPrivate::QCLuceneDocumentPrivate()
    : QSharedData()
{
    document = 0;
    deleteCLuceneDocument = true;
}

QCLuceneDocumentPrivate::QCLuceneDocumentPrivate(const QCLuceneDocumentPrivate &other)
    : QSharedData()
{
    document = _CL_POINTER(other.document);
    deleteCLuceneDocument = other.deleteCLuceneDocument;
}

QCLuceneDocumentPrivate::~QCLuceneDocumentPrivate()
{
    if (deleteCLuceneDocument)
        _CLDECDELETE(document);
}


QCLuceneDocument::QCLuceneDocument()
    : d(new QCLuceneDocumentPrivate())
{
    // nothing todo
    d->document = new lucene::document::Document();
}

QCLuceneDocument::~QCLuceneDocument()
{
    qDeleteAll(fieldList);
    fieldList.clear();
}

void QCLuceneDocument::add(QCLuceneField *field)
{
    field->d->deleteCLuceneField = false;
    d->document->add(*field->d->field);
    fieldList.append(field);
}

QCLuceneField* QCLuceneDocument::getField(const QString &name) const
{
    QCLuceneField* field = 0;
    foreach (field, fieldList) {
        if (field->name() == name && field->d->field != 0)
            return field;
    }

    field = 0;
    TCHAR *fieldName = QStringToTChar(name);
    lucene::document::Field *f = d->document->getField(fieldName);
    if (f) {
        field = new QCLuceneField();
        field->d->field = f;
        fieldList.append(field);
        field->d->deleteCLuceneField = false;

        lucene::util::Reader *r = f->readerValue();
        if (r) {
            field->reader->d->reader = r;
            field->reader->d->deleteCLuceneReader = false;
        }
    }
    delete [] fieldName;

    return field;
}

QString QCLuceneDocument::get(const QString &name) const
{
    QCLuceneField* field = getField(name);
    if (field)
        return field->stringValue();

    return QString();
}

QString QCLuceneDocument::toString() const
{
    return TCharToQString(d->document->toString());
}

void QCLuceneDocument::setBoost(qreal boost)
{
    d->document->setBoost(qreal(boost));
}

qreal QCLuceneDocument::getBoost() const
{
    return qreal(d->document->getBoost());
}

void QCLuceneDocument::removeField(const QString &name)
{
    TCHAR *fieldName = QStringToTChar(name);
    d->document->removeField(fieldName);
    delete [] fieldName;

    QList<QCLuceneField*> tmp;
    lucene::document::DocumentFieldEnumeration *dfe = d->document->fields();
    while (dfe->hasMoreElements()) {
        const lucene::document::Field* f = dfe->nextElement();
        foreach (QCLuceneField* field, fieldList) {
            if (f == field->d->field) {
                tmp.append(field);
                break;
            }
        }
    }
    _CLDELETE(dfe);
    fieldList = tmp;
}

void QCLuceneDocument::removeFields(const QString &name)
{
    for (qint32 i = fieldList.count() -1; i >= 0; --i) {
        QCLuceneField* field = fieldList.at(i);
        if (field->name() == name)
            delete fieldList.takeAt(i);
    }

    TCHAR *fieldName = QStringToTChar(name);
    d->document->removeFields(fieldName);
    delete [] fieldName;
}

QStringList QCLuceneDocument::getValues(const QString &name) const
{
    TCHAR *fieldName = QStringToTChar(name);
    TCHAR **values = d->document->getValues(fieldName);

    QStringList retValue;
    if (values) {
        for (qint32 i = 0; 0 != values[i]; ++i) {
            retValue.append(TCharToQString((const TCHAR*)values[i]));
            delete [] values[i]; values[i] = 0;
        }
        delete values;
    }

    delete [] fieldName;
    return retValue;
}

void QCLuceneDocument::clear()
{
    d->document->clear();
    qDeleteAll(fieldList);
    fieldList.clear();
}

QT_END_NAMESPACE
