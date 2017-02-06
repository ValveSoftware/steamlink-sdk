/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "util.h"
#include <QtTest/QtTest>
#include <QtWebEngineWidgets/qwebenginecontextmenudata.h>
#include <QtWebEngineWidgets/qwebengineprofile.h>
#include <QtWebEngineWidgets/qwebenginepage.h>
#include <QtWebEngineWidgets/qwebengineview.h>

class WebView : public QWebEngineView
{
    Q_OBJECT
public:
    void activateMenu(QWidget *widget, const QPoint &position)
    {
        QTest::mouseMove(widget, position);
        QTest::mousePress(widget, Qt::RightButton, 0, position);
        QContextMenuEvent evcont(QContextMenuEvent::Mouse, position, mapToGlobal(position));
        event(&evcont);
        QTest::mouseRelease(widget, Qt::RightButton, 0, position);
    }

    const QWebEngineContextMenuData& data()
    {
        return m_data;
    }

signals:
    void menuReady();

protected:
    void contextMenuEvent(QContextMenuEvent *)
    {
        m_data = page()->contextMenuData();
        emit menuReady();
    }
private:
    QWebEngineContextMenuData m_data;
};

class tst_QWebEngineSpellcheck : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();
    void spellCheckLanguage();
    void spellCheckLanguages();
    void spellCheckEnabled();
    void spellcheck();
    void spellcheck_data();

private:
    void load();
    WebView *m_view;
};

void tst_QWebEngineSpellcheck::initTestCase()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    QVERIFY(!profile->isSpellCheckEnabled());
    QVERIFY(profile->spellCheckLanguages().isEmpty());
}

void tst_QWebEngineSpellcheck::init()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    profile->setSpellCheckEnabled(false);
    profile->setSpellCheckLanguages(QStringList());
    m_view = new WebView();
}

void tst_QWebEngineSpellcheck::load()
{
    m_view->page()->load(QUrl("qrc:///resources/index.html"));
    m_view->show();
    waitForSignal(m_view->page(), SIGNAL(loadFinished(bool)));
}

void tst_QWebEngineSpellcheck::cleanup()
{
    delete m_view;
}

void tst_QWebEngineSpellcheck::spellCheckLanguage()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    profile->setSpellCheckLanguages({"en-US"});
    QVERIFY(profile->spellCheckLanguages() == QStringList({"en-US"}));
}

void tst_QWebEngineSpellcheck::spellCheckLanguages()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    profile->setSpellCheckLanguages({"en-US","de-DE"});
    QVERIFY(profile->spellCheckLanguages() == QStringList({"en-US","de-DE"}));
}


void tst_QWebEngineSpellcheck::spellCheckEnabled()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    profile->setSpellCheckEnabled(true);
    QVERIFY(profile->isSpellCheckEnabled());
}

void tst_QWebEngineSpellcheck::spellcheck()
{
    QFETCH(QStringList, languages);
    QFETCH(QStringList, suggestions);

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    profile->setSpellCheckLanguages(languages);
    profile->setSpellCheckEnabled(true);
    load();

    // make textarea editable
    evaluateJavaScriptSync(m_view->page(), "makeEditable();");

    // calcuate position of misspelled word
    QVariantList list = evaluateJavaScriptSync(m_view->page(), "findWordPosition('I lovee Qt ....','lovee');").toList();
    QRect rect(list[0].value<int>(),list[1].value<int>(),list[2].value<int>(),list[3].value<int>());

    //type text, spellchecker needs time
    QTest::mouseMove(m_view->focusWidget(), QPoint(20,20));
    QTest::mousePress(m_view->focusWidget(), Qt::LeftButton, 0, QPoint(20,20));
    QTest::mouseRelease(m_view->focusWidget(), Qt::LeftButton, 0, QPoint(20,20));
    QString text("I lovee Qt ....");
    for (int i = 0; i < text.length(); i++) {
        QTest::keyClicks(m_view->focusWidget(), text.at(i));
        QTest::qWait(60);
    }

    // make sure text is there
    QString result = evaluateJavaScriptSync(m_view->page(), "text();").toString();
    QVERIFY(result == text);

    // open menu on misspelled word
    m_view->activateMenu(m_view->focusWidget(), rect.center());
    waitForSignal(m_view, SIGNAL(menuReady()));

    // check if menu is valid
    QVERIFY(m_view->data().isValid());
    QVERIFY(m_view->data().isContentEditable());

    // check misspelled word
    QVERIFY(m_view->data().misspelledWord() == "lovee");

    // check suggestions
    QVERIFY(m_view->data().spellCheckerSuggestions() == suggestions);

    // check replace word
    m_view->page()->replaceMisspelledWord("love");
    text = "I love Qt ....";
    QTRY_VERIFY(evaluateJavaScriptSync(m_view->page(), "text();").toString() == text);
}

void tst_QWebEngineSpellcheck::spellcheck_data()
{
    QTest::addColumn<QStringList>("languages");
    QTest::addColumn<QStringList>("suggestions");
    QTest::newRow("en-US") << QStringList({"en-US"}) << QStringList({"love", "loves"});
    QTest::newRow("en-US,de-DE") << QStringList({"en-US","de-DE"}) << QStringList({"love", "liebe", "loves"});
}

QTEST_MAIN(tst_QWebEngineSpellcheck)
#include "tst_qwebenginespellcheck.moc"
