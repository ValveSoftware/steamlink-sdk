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

#include "webview.h"
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineProfile>
#include <QWebEngineContextMenuData>

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    m_spellCheckLanguages["English"] = "en-US";
    m_spellCheckLanguages["German"] = "de-DE";
    QWebEngineProfile *profile = page()->profile();
    profile->setSpellCheckEnabled(true);
    profile->setSpellCheckLanguages({"en-US"});
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    const QWebEngineContextMenuData &data = page()->contextMenuData();
    Q_ASSERT(data.isValid());

    if (!data.isContentEditable()) {
        QWebEngineView::contextMenuEvent(event);
        return;
    }

    QWebEngineProfile *profile = page()->profile();
    const QString &language = profile->spellCheckLanguages().first();
    QMenu *menu = page()->createStandardContextMenu();
    menu->addSeparator();

    QAction *spellcheckAction = new QAction(tr("Check Spelling"));
    spellcheckAction->setCheckable(true);
    spellcheckAction->setChecked(profile->isSpellCheckEnabled());
    connect(spellcheckAction, &QAction::toggled, this, [profile](bool toogled) {
        profile->setSpellCheckEnabled(toogled);
    });
    menu->addAction(spellcheckAction);

    if (profile->isSpellCheckEnabled()) {
        QMenu *subMenu = menu->addMenu(tr("Select Language"));
        foreach (const QString &str, m_spellCheckLanguages.keys()) {
            QAction *action = subMenu->addAction(str);
            action->setCheckable(true);
            QString lang = m_spellCheckLanguages[str];
            action->setChecked(language == lang);
            connect(action, &QAction::triggered, this, [profile, lang](){
               profile->setSpellCheckLanguages(QStringList()<<lang);
            });
        }
    }
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(event->globalPos());
}


