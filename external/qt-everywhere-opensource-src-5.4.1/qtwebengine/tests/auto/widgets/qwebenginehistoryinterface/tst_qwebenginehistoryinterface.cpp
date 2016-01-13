/*
    Copyright (C) 2008 Holger Hans Peter Freyther

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

#include <qwebenginepage.h>
#include <qwebengineview.h>
#if defined(QWEBENGINEHISTORYINTERFACE)
#include <qwebenginehistoryinterface.h>
#endif
#include <QDebug>

class tst_QWebEngineHistoryInterface : public QObject
{
    Q_OBJECT

public:
    tst_QWebEngineHistoryInterface();
    virtual ~tst_QWebEngineHistoryInterface();

public Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void visitedLinks();

private:


private:
    QWebEngineView* m_view;
    QWebEnginePage* m_page;
};

tst_QWebEngineHistoryInterface::tst_QWebEngineHistoryInterface()
{
}

tst_QWebEngineHistoryInterface::~tst_QWebEngineHistoryInterface()
{
}

void tst_QWebEngineHistoryInterface::initTestCase()
{
}

void tst_QWebEngineHistoryInterface::init()
{
    m_view = new QWebEngineView();
    m_page = m_view->page();
}

void tst_QWebEngineHistoryInterface::cleanup()
{
    delete m_view;
}

#if defined(QWEBENGINEHISTORYINTERFACE)
class FakeHistoryImplementation : public QWebEngineHistoryInterface {
public:
    void addHistoryEntry(const QString&) {}
    bool historyContains(const QString& url) const {
        return url == QLatin1String("http://www.trolltech.com/");
    }
};
#endif

/*
 * Test that visited links are properly colored. http://www.trolltech.com is marked
 * as visited, so the below website should have exactly one element in the a:visited
 * state.
 */
void tst_QWebEngineHistoryInterface::visitedLinks()
{
#if !defined(QWEBENGINEELEMENT)
    QSKIP("QWEBENGINEELEMENT");
#else
    QWebEngineHistoryInterface::setDefaultInterface(new FakeHistoryImplementation);
    m_view->setHtml("<html><style>:link{color:green}:visited{color:red}</style><body><a href='http://www.trolltech.com' id='vlink'>Trolltech</a></body></html>");
    QWebEngineElement anchor = m_view->page()->mainFrame()->findFirstElement("a[id=vlink]");
    QString linkColor = anchor.styleProperty("color", QWebEngineElement::ComputedStyle);
    QCOMPARE(linkColor, QString::fromLatin1("rgb(255, 0, 0)"));
#endif
}

QTEST_MAIN(tst_QWebEngineHistoryInterface)
#include "tst_qwebenginehistoryinterface.moc"
