/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
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

#include "qquickfontlistmodel_p.h"
#include <QtGui/qfontdatabase.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE

class QQuickFontListModelPrivate
{
    Q_DECLARE_PUBLIC(QQuickFontListModel)

public:
    QQuickFontListModelPrivate(QQuickFontListModel *q)
        : q_ptr(q), ws(QFontDatabase::Any)
        , options(QSharedPointer<QFontDialogOptions>(new QFontDialogOptions()))
    {}

    QQuickFontListModel *q_ptr;
    QFontDatabase db;
    QFontDatabase::WritingSystem ws;
    QSharedPointer<QFontDialogOptions> options;
    QStringList families;
    QHash<int, QByteArray> roleNames;
    ~QQuickFontListModelPrivate() {}
    void init();
};


void QQuickFontListModelPrivate::init()
{
    Q_Q(QQuickFontListModel);

    families = db.families();

    emit q->rowCountChanged();
    emit q->writingSystemChanged();
}

QQuickFontListModel::QQuickFontListModel(QObject *parent)
    : QAbstractListModel(parent), d_ptr(new QQuickFontListModelPrivate(this))
{
    Q_D(QQuickFontListModel);
    d->roleNames[FontFamilyRole] = "family";
    d->init();
}

QQuickFontListModel::~QQuickFontListModel()
{
}

QVariant QQuickFontListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QQuickFontListModel);
    QVariant rv;

    if (index.row() >= d->families.size())
        return rv;

    switch (role)
    {
        case FontFamilyRole:
            rv = d->families.at(index.row());
            break;
        default:
            break;
    }
    return rv;
}

QHash<int, QByteArray> QQuickFontListModel::roleNames() const
{
    Q_D(const QQuickFontListModel);
    return d->roleNames;
}

int QQuickFontListModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const QQuickFontListModel);
    Q_UNUSED(parent);
    return d->families.size();
}

QModelIndex QQuickFontListModel::index(int row, int , const QModelIndex &) const
{
    return createIndex(row, 0);
}

QString QQuickFontListModel::writingSystem() const
{
    Q_D(const QQuickFontListModel);
    return QFontDatabase::writingSystemName(d->ws);
}

void QQuickFontListModel::setWritingSystem(const QString &wSystem)
{
    Q_D(QQuickFontListModel);

    if (wSystem == writingSystem())
        return;

    QList<QFontDatabase::WritingSystem> wss;
    wss << QFontDatabase::Any;
    wss << d->db.writingSystems();
    QFontDatabase::WritingSystem ws;
    foreach (ws, wss) {
        if (wSystem == QFontDatabase::writingSystemName(ws)) {
            d->ws = ws;
            updateFamilies();
            return;
        }
    }
}

void QQuickFontListModel::updateFamilies()
{
    Q_D(QQuickFontListModel);

    beginResetModel();
    const QFontDialogOptions::FontDialogOptions scalableMask = (QFontDialogOptions::FontDialogOptions)(QFontDialogOptions::ScalableFonts | QFontDialogOptions::NonScalableFonts);
    const QFontDialogOptions::FontDialogOptions spacingMask = (QFontDialogOptions::FontDialogOptions)(QFontDialogOptions::ProportionalFonts | QFontDialogOptions::MonospacedFonts);
    const QFontDialogOptions::FontDialogOptions options = d->options->options();

    d->families.clear();
    foreach (const QString &family, d->db.families(d->ws)) {
        if ((options & scalableMask) && (options & scalableMask) != scalableMask) {
            if (bool(options & QFontDialogOptions::ScalableFonts) != d->db.isSmoothlyScalable(family))
                continue;
        }
        if ((options & spacingMask) && (options & spacingMask) != spacingMask) {
            if (bool(options & QFontDialogOptions::MonospacedFonts) != d->db.isFixedPitch(family))
                continue;
        }
        d->families << family;
    }
    endResetModel();
}

bool QQuickFontListModel::scalableFonts() const
{
    Q_D(const QQuickFontListModel);
    return d->options->testOption(QFontDialogOptions::ScalableFonts);
}

bool QQuickFontListModel::nonScalableFonts() const
{
    Q_D(const QQuickFontListModel);
    return d->options->testOption(QFontDialogOptions::NonScalableFonts);
}

bool QQuickFontListModel::monospacedFonts() const
{
    Q_D(const QQuickFontListModel);
    return d->options->testOption(QFontDialogOptions::MonospacedFonts);
}

bool QQuickFontListModel::proportionalFonts() const
{
    Q_D(const QQuickFontListModel);
    return d->options->testOption(QFontDialogOptions::ProportionalFonts);
}

QJSValue QQuickFontListModel::get(int idx) const
{
    Q_D(const QQuickFontListModel);

    QJSEngine *engine = qmlEngine(this);

    if (idx < 0 || idx >= count())
        return engine->newObject();

    QJSValue result = engine->newObject();
    int count = d->roleNames.keys().count();
    for (int i = 0; i < count; ++i)
        result.setProperty(QString(d->roleNames[Qt::UserRole + i + 1]), data(index(idx, 0), Qt::UserRole + i + 1).toString());

    return result;
}

QJSValue QQuickFontListModel::pointSizes()
{
    QJSEngine *engine = qmlEngine(this);

    QList<int> pss = QFontDatabase::standardSizes();
    int size = pss.size();

    QJSValue result = engine->newArray(size);
    for (int i = 0; i < size; ++i)
        result.setProperty(i, pss.at(i));

    return result;
}

void QQuickFontListModel::classBegin()
{
}

void QQuickFontListModel::componentComplete()
{
}

void QQuickFontListModel::setScalableFonts(bool arg)
{
    Q_D(QQuickFontListModel);
    d->options->setOption(QFontDialogOptions::ScalableFonts, arg);
    updateFamilies();
    emit scalableFontsChanged();
}

void QQuickFontListModel::setNonScalableFonts(bool arg)
{
    Q_D(QQuickFontListModel);
    d->options->setOption(QFontDialogOptions::NonScalableFonts, arg);
    updateFamilies();
    emit nonScalableFontsChanged();
}

void QQuickFontListModel::setMonospacedFonts(bool arg)
{
    Q_D(QQuickFontListModel);
    d->options->setOption(QFontDialogOptions::MonospacedFonts, arg);
    updateFamilies();
    emit monospacedFontsChanged();
}

void QQuickFontListModel::setProportionalFonts(bool arg)
{
    Q_D(QQuickFontListModel);
    d->options->setOption(QFontDialogOptions::ProportionalFonts, arg);
    updateFamilies();
    emit proportionalFontsChanged();
}

QT_END_NAMESPACE
