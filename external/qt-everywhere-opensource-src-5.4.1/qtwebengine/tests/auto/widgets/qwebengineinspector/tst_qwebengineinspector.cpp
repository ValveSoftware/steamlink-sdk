/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>

#include <qdir.h>
#if defined(QWEBENGINEINSPECTOR)
#include <qwebengineinspector.h>
#endif
#include <qwebenginepage.h>
#if defined(QWEBENGINESETTINGS)
#include <qwebenginesettings.h>
#endif

class tst_QWebEngineInspector : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void attachAndDestroyPageFirst();
    void attachAndDestroyInspectorFirst();
    void attachAndDestroyInternalInspector();
};

void tst_QWebEngineInspector::attachAndDestroyPageFirst()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // External inspector + manual destruction of page first
    QWebEnginePage* page = new QWebEnginePage();
    page->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    QWebEngineInspector* inspector = new QWebEngineInspector();
    inspector->setPage(page);
    page->updatePositionDependentActions(QPoint(0, 0));
    page->triggerAction(QWebEnginePage::InspectElement);

    delete page;
    delete inspector;
#endif
}

void tst_QWebEngineInspector::attachAndDestroyInspectorFirst()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // External inspector + manual destruction of inspector first
    QWebEnginePage* page = new QWebEnginePage();
    page->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    QWebEngineInspector* inspector = new QWebEngineInspector();
    inspector->setPage(page);
    page->updatePositionDependentActions(QPoint(0, 0));
    page->triggerAction(QWebEnginePage::InspectElement);

    delete inspector;
    delete page;
#endif
}

void tst_QWebEngineInspector::attachAndDestroyInternalInspector()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // Internal inspector
    QWebEnginePage page;
    page.settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    page.updatePositionDependentActions(QPoint(0, 0));
    page.triggerAction(QWebEnginePage::InspectElement);
#endif
}

QTEST_MAIN(tst_QWebEngineInspector)

#include "tst_qwebengineinspector.moc"
