/*
  Copyright (C) 2015 Valve Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "hello.h"


//--------------------------------------------------------------------------------------------------
// CHelloWidget constructor
//--------------------------------------------------------------------------------------------------
CHelloWidget::CHelloWidget()
:	QWidget()
{
	QVBoxLayout *pLayout = new QVBoxLayout( this );

	QLabel *pLabel = new QLabel;
	pLabel->setText( tr( "Hello World!" ) );
	pLabel->setAlignment( Qt::AlignCenter );
	pLayout->addWidget( pLabel );

	QWebEngineView *pWebView = new QWebEngineView;
	pWebView->setUrl( QUrl( "http://www.valvesoftware.com/" ) );
	pLayout->addWidget( pWebView );

	m_pButton = new QPushButton;
	m_pButton->setText( tr( "Quit" ) );
	connect( m_pButton, SIGNAL( released() ), this, SLOT( OnQuit() ) );
	pLayout->addWidget( m_pButton );
}


//--------------------------------------------------------------------------------------------------
// Handle controller events
//--------------------------------------------------------------------------------------------------
void CHelloWidget::controllerEvent( QControllerEvent *pEvent )
{
	if ( pEvent->isButtonAPress() )
	{
		m_pButton->click();
	}
}


//--------------------------------------------------------------------------------------------------
// Show a quit confirmation dialog
// This needs to be from a single shot timer so controller events are processed in the dialog
//--------------------------------------------------------------------------------------------------
void CHelloWidget::OnQuit()
{
	QTimer::singleShot( 1, this, SLOT( ShowQuitConfirmation() ) );
}


//--------------------------------------------------------------------------------------------------
// Prompt the user to see if they really want to quit
//--------------------------------------------------------------------------------------------------
void CHelloWidget::ShowQuitConfirmation()
{
	if ( QMessageBox::question( this, QString(), tr( "Are you sure you want to quit?" ), ( QMessageBox::Ok | QMessageBox::Cancel ) ) == QMessageBox::Ok )
	{
		QApplication::quit();
	}
}
