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

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <SDL2/SDL.h>

#include "controllermanager.h"
#include "hello.h"


//--------------------------------------------------------------------------------------------------
// Get the language selected by the user during first-time setup
//--------------------------------------------------------------------------------------------------
static QString GetLanguage()
{
	QFile file( "/mnt/config/system/locale.txt" );
	if ( file.open( QIODevice::ReadOnly ) )
		return file.readAll().trimmed();

	// Default language en_US
	return "en_US";
}


//--------------------------------------------------------------------------------------------------
// Load a default Latin-1 font
// Other fonts are already set up on the system for the current language
//--------------------------------------------------------------------------------------------------
static void LoadFonts()
{
	QFile font( ":/resources/fonts/DejaVuSerif.ttf" );
	font.open( QIODevice::ReadOnly );
	QByteArray fontData = font.readAll();

	QFontDatabase::addApplicationFontFromData( fontData );
}


//--------------------------------------------------------------------------------------------------
// Execution starts here!
//--------------------------------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
	//
	// Initialize SDL to set up display mode and scale
	//
	SDL_Init(SDL_INIT_VIDEO);

	//
	// Create the Qt application
	//
	// For simplicity, the entire application and support objects are created here on the stack
	//
	QApplication app(argc, argv);

	//
	// Set up directory path for QSettings
	//
	app.setOrganizationName( "Example" );
	app.setOrganizationDomain( "example.com" );
	app.setApplicationName( "Qt Example" );

	//
	// Load fonts and set default application font settings
	//
	LoadFonts();

	QFont font = QApplication::font();
	font.setHintingPreference( QFont::PreferVerticalHinting );
	QApplication::setFont( font );

	//
	// Set up localization
	//
	QLocale locale = GetLanguage();
	QLocale::setDefault( locale );

	QTranslator qtTranslator;
	qtTranslator.load( locale, "qt", "_", QLibraryInfo::location( QLibraryInfo::TranslationsPath ) );
	app.installTranslator( &qtTranslator );

	QTranslator qtBaseTranslator;
	qtBaseTranslator.load( locale, "qtbase", "_", QLibraryInfo::location( QLibraryInfo::TranslationsPath ) );
	app.installTranslator( &qtBaseTranslator );

	QTranslator appTranslator;
	appTranslator.load( locale, "example", "_", ":/translations" );
	app.installTranslator( &appTranslator );

	//
	// Set up the stylesheet for the UI
	//
	QFile stylesheetFile( ":/resources/style.qss" );
	stylesheetFile.open( QFile::ReadOnly );
	QString stylesheetString = QLatin1String( stylesheetFile.readAll() );
	stylesheetFile.close();
	app.setStyleSheet( stylesheetString );

	//
	// Move the mouse to the lower right so it's hidden when using a controller
	//
	QCursor().setPos( 0xFFFF, 0xFFFF );

	//
	// Set up game controller support
	//
	CControllerManager controllerManager( &app );
	app.installEventFilter( &controllerManager );

	//
	// Load and show our UI
	//
	CHelloWidget widget;
	widget.show();

	return app.exec();
}
