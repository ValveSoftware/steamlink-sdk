/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include "qhelpengine.h"
#include "qhelpengine_p.h"
#include "qhelpdbreader_p.h"
#include "qhelpcontentwidget.h"
#include "qhelpindexwidget.h"
#include "qhelpsearchengine.h"
#include "qhelpcollectionhandler_p.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QLibrary>
#include <QtCore/QPluginLoader>
#include <QtWidgets/QApplication>
#include <QtSql/QSqlQuery>

QT_BEGIN_NAMESPACE

QHelpEnginePrivate::QHelpEnginePrivate()
    : QHelpEngineCorePrivate()
    , contentModel(0)
    , contentWidget(0)
    , indexModel(0)
    , indexWidget(0)
    , searchEngine(0)
{
}

QHelpEnginePrivate::~QHelpEnginePrivate()
{
}

void QHelpEnginePrivate::init(const QString &collectionFile,
                              QHelpEngineCore *helpEngineCore)
{
    QHelpEngineCorePrivate::init(collectionFile, helpEngineCore);

    if (!contentModel)
        contentModel = new QHelpContentModel(this);
    if (!indexModel)
        indexModel = new QHelpIndexModel(this);

    connect(helpEngineCore, SIGNAL(setupFinished()), this,
        SLOT(applyCurrentFilter()));
    connect(helpEngineCore, SIGNAL(currentFilterChanged(QString)), this,
        SLOT(applyCurrentFilter()));
}

void QHelpEnginePrivate::applyCurrentFilter()
{
    if (!error.isEmpty())
        return;
    contentModel->createContents(currentFilter);
    indexModel->createIndex(currentFilter);
}

void QHelpEnginePrivate::setContentsWidgetBusy()
{
#ifndef QT_NO_CURSOR
    contentWidget->setCursor(Qt::WaitCursor);
#endif
}

void QHelpEnginePrivate::unsetContentsWidgetBusy()
{
#ifndef QT_NO_CURSOR
    contentWidget->unsetCursor();
#endif
}

void QHelpEnginePrivate::setIndexWidgetBusy()
{
#ifndef QT_NO_CURSOR
    indexWidget->setCursor(Qt::WaitCursor);
#endif
}

void QHelpEnginePrivate::unsetIndexWidgetBusy()
{
#ifndef QT_NO_CURSOR
    indexWidget->unsetCursor();
#endif
}

void QHelpEnginePrivate::stopDataCollection()
{
    contentModel->invalidateContents(true);
    indexModel->invalidateIndex(true);
}



/*!
    \class QHelpEngine
    \since 4.4
    \inmodule QtHelp
    \brief The QHelpEngine class provides access to contents and
    indices of the help engine.


*/

/*!
    Constructs a new help engine with the given \a parent. The help
    engine uses the information stored in the \a collectionFile for
    providing help. If the collection file does not already exist,
    it will be created.
*/
QHelpEngine::QHelpEngine(const QString &collectionFile, QObject *parent)
    : QHelpEngineCore(d = new QHelpEnginePrivate(), parent)
{
    d->init(collectionFile, this);
}

/*!
    Destroys the help engine object.
*/
QHelpEngine::~QHelpEngine()
{
    d->stopDataCollection();
}

/*!
    Returns the content model.
*/
QHelpContentModel *QHelpEngine::contentModel() const
{
    return d->contentModel;
}

/*!
    Returns the index model.
*/
QHelpIndexModel *QHelpEngine::indexModel() const
{
    return d->indexModel;
}

/*!
    Returns the content widget.
*/
QHelpContentWidget *QHelpEngine::contentWidget()
{
    if (!d->contentWidget) {
        d->contentWidget = new QHelpContentWidget();
        d->contentWidget->setModel(d->contentModel);
        connect(d->contentModel, SIGNAL(contentsCreationStarted()),
            d, SLOT(setContentsWidgetBusy()));
        connect(d->contentModel, SIGNAL(contentsCreated()),
            d, SLOT(unsetContentsWidgetBusy()));
    }
    return d->contentWidget;
}

/*!
    Returns the index widget.
*/
QHelpIndexWidget *QHelpEngine::indexWidget()
{
    if (!d->indexWidget) {
        d->indexWidget = new QHelpIndexWidget();
        d->indexWidget->setModel(d->indexModel);
        connect(d->indexModel, SIGNAL(indexCreationStarted()),
            d, SLOT(setIndexWidgetBusy()));
        connect(d->indexModel, SIGNAL(indexCreated()),
            d, SLOT(unsetIndexWidgetBusy()));
    }
    return d->indexWidget;
}

/*!
    Returns the default search engine.
*/
QHelpSearchEngine* QHelpEngine::searchEngine()
{
    if (!d->searchEngine)
        d->searchEngine = new QHelpSearchEngine(this, this);
    return d->searchEngine;
}

QT_END_NAMESPACE
