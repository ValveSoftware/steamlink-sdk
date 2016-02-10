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
#include "SDL.h"
#include "controllermanager.h"


//--------------------------------------------------------------------------------------------------
// The controller axis range
//--------------------------------------------------------------------------------------------------
#define CONTROLLER_AXIS_MAX		32767


//--------------------------------------------------------------------------------------------------
// Function to normalize an axis value outside of the dead zone
//--------------------------------------------------------------------------------------------------
static float NormalizeAxisValue( float flValue )
{
	const float CONTROLLER_DEAD_ZONE = 0.5f;

	if ( flValue >= 0.0f )
	{
		if ( flValue <= CONTROLLER_DEAD_ZONE )
		{
			return 0.0f;
		}
		return ( flValue - CONTROLLER_DEAD_ZONE ) / ( 1.0f - CONTROLLER_DEAD_ZONE );
	}
	else
	{
		if ( flValue <= CONTROLLER_DEAD_ZONE )
		{
			return 0.0f;
		}
		return ( flValue + CONTROLLER_DEAD_ZONE ) / ( 1.0f - CONTROLLER_DEAD_ZONE );
	}
}


//--------------------------------------------------------------------------------------------------
// Game controller state structure
//--------------------------------------------------------------------------------------------------
struct CControllerManager::GameController_t
{
	SDL_GameController *m_pController;
	int m_nJoystickID;
	bool m_arrThumbstickTilted[ 2 ];
	bool m_arrTriggerTilted[ 2 ];
	int m_arrAxisValues[ SDL_CONTROLLER_AXIS_MAX ];
	bool m_arrButtonStates[ SDL_CONTROLLER_BUTTON_MAX ];
	uint32_t m_nAttachedTime;
};


//--------------------------------------------------------------------------------------------------
// CControllerManager constructor
//--------------------------------------------------------------------------------------------------
CControllerManager::CControllerManager( QObject *pParent )
:	QObject( pParent )
,	m_bInitalizedSDL( false )
{
	BInitGameControllers();

	memset( m_arrArrowKeyState, 0, sizeof( m_arrArrowKeyState ) );

	// Check for controller updates at 60 FPS to be nice and responsive
	m_nUpdateTimerID = startTimer( 16 );
}


//--------------------------------------------------------------------------------------------------
// CControllerManager destructor
//--------------------------------------------------------------------------------------------------
CControllerManager::~CControllerManager()
{
	QuitGameControllers();
}


//--------------------------------------------------------------------------------------------------
// Initialize game controller list
//--------------------------------------------------------------------------------------------------
bool CControllerManager::BInitGameControllers()
{
	if ( !m_bInitalizedSDL )
	{
		if ( SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER ) < 0 )
		{
			qWarning() << "Couldn't initialize SDL:" << SDL_GetError();
			return false;
		}
		m_bInitalizedSDL = true;
	}
	return true;
}


//--------------------------------------------------------------------------------------------------
// Return true if we should ignore this controller
//--------------------------------------------------------------------------------------------------
bool CControllerManager::BShouldIgnoreController( const char *pszGUID )
{
	bool bIgnored = false;
	int nGUIDLen = strlen( pszGUID );

	FILE *pFile = fopen( "/etc/controller_ignore.txt", "r" );
	if ( pFile )
	{
		char line[ 1024 ];
		while ( fgets( line, sizeof( line ), pFile ) )
		{
			if ( strncmp( line, pszGUID, nGUIDLen ) == 0 )
			{
				bIgnored = true;
				break;
			}
		}
		fclose( pFile );
	}
	return bIgnored;
}


//--------------------------------------------------------------------------------------------------
// Check for game controller updates
//--------------------------------------------------------------------------------------------------
void CControllerManager::CheckGameControllers()
{
	SDL_PumpEvents();

	SDL_Event event;
	while ( SDL_PollEvent( &event ) > 0 )
	{
		switch ( event.type )
		{
		case SDL_JOYDEVICEADDED:
			// If we don't recognize it, try to use it as an XBox controller
			if ( !SDL_IsGameController( event.jdevice.which ) )
			{
				char pszGUID[ 33 ];
				SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID( event.jdevice.which );
				SDL_JoystickGetGUIDString( guid, pszGUID, sizeof( pszGUID ) );
				if ( !BShouldIgnoreController( pszGUID ) )
				{
					SDL_GameControllerAddMapping( QString( "%1,XInput Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3," ).arg( pszGUID ).toUtf8() );
					OnGameControllerAdded( event.jdevice.which );
				}
			}
			break;
		case SDL_CONTROLLERDEVICEADDED:
			OnGameControllerAdded( event.cdevice.which );
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			OnGameControllerRemoved( event.cdevice.which );
			break;
		case SDL_CONTROLLERAXISMOTION:
			OnGameControllerAxis( GetGameController( event.caxis.which ), event.caxis.axis, event.caxis.value );
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			OnGameControllerButton( GetGameController( event.cbutton.which ), event.cbutton.button, true );
			break;
		case SDL_CONTROLLERBUTTONUP:
			OnGameControllerButton( GetGameController( event.cbutton.which ), event.cbutton.button, false );
			break;
		case SDL_QUIT:
			QApplication::quit();
			break;
		}
	}
}


//--------------------------------------------------------------------------------------------------
// Handle game controller hotplug insertion
//--------------------------------------------------------------------------------------------------
void CControllerManager::OnGameControllerAdded( int iJoystick )
{
	GameController_t *pController = new GameController_t;
	memset( pController, 0, sizeof( *pController ) );
	pController->m_pController = SDL_GameControllerOpen( iJoystick );
	if ( !pController->m_pController )
	{
		delete pController;
		return;
	}
	pController->m_nAttachedTime = SDL_GetTicks();
	pController->m_nJoystickID = SDL_JoystickInstanceID( SDL_GameControllerGetJoystick( pController->m_pController ) );

	m_vecGameControllers << pController;
}


//--------------------------------------------------------------------------------------------------
// Handle game controller hotplug removal
//--------------------------------------------------------------------------------------------------
void CControllerManager::OnGameControllerRemoved( int nJoystickID )
{
	for ( int iIndex = 0; iIndex < m_vecGameControllers.count(); ++iIndex )
	{
		GameController_t *pController = m_vecGameControllers[ iIndex ];
		if ( pController->m_nJoystickID == nJoystickID )
		{
			// Send reset events for any controllers that are removed
			for ( int iAxis = 0; iAxis < SDL_CONTROLLER_AXIS_MAX; ++iAxis )
				OnGameControllerAxis( pController, iAxis, 0 );

			for ( int iButton = 0; iButton < SDL_CONTROLLER_BUTTON_MAX; ++iButton )
				OnGameControllerButton( pController, iButton, false );
			
			m_vecGameControllers.remove( iIndex );
			SDL_GameControllerClose( pController->m_pController );
			delete pController;
			return;
		}
	}
}


//--------------------------------------------------------------------------------------------------
// Get a game controller for a specific joystick ID
//--------------------------------------------------------------------------------------------------
CControllerManager::GameController_t *CControllerManager::GetGameController( int nJoystickID )
{
	Q_FOREACH( GameController_t *pController, m_vecGameControllers )
	{
		if ( pController->m_nJoystickID == nJoystickID )
			return pController;
	}
	return NULL;
}


//--------------------------------------------------------------------------------------------------
// Ignore events that come right after opening the controller as the kernel appears to send spurious DPad and axis events for the XBox 360 controller
//--------------------------------------------------------------------------------------------------
bool CControllerManager::BIgnoreGameControllerEvent( GameController_t *pController )
{
	if ( !pController )
		return true;

	const int ATTACH_EVENT_DELAY = 200;
	if ( pController->m_nAttachedTime &&
		 ( SDL_GetTicks() - pController->m_nAttachedTime ) <= ATTACH_EVENT_DELAY )
	{
		//qWarning() << "Ignoring spurious controller event";
		return true;
	}
	else
	{
		pController->m_nAttachedTime = 0;
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
// Handle game controller button events
//--------------------------------------------------------------------------------------------------
void CControllerManager::OnGameControllerButton( GameController_t *pController, int nButton, bool bPressed )
{
	if ( BIgnoreGameControllerEvent( pController ) )
		return;

	if ( pController->m_arrButtonStates[ nButton ] == bPressed )
		return;

	pController->m_arrButtonStates[ nButton ] = bPressed;

	switch ( nButton )
	{
	case SDL_CONTROLLER_BUTTON_A:
		SendControllerButtonEvent( QControllerEvent::BUTTON_A, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_B:
		SendControllerButtonEvent( QControllerEvent::BUTTON_B, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_X:
		SendControllerButtonEvent( QControllerEvent::BUTTON_X, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_Y:
		SendControllerButtonEvent( QControllerEvent::BUTTON_Y, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_BACK:
		SendControllerButtonEvent( QControllerEvent::BUTTON_BACK, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_GUIDE:
		SendControllerButtonEvent( QControllerEvent::BUTTON_GUIDE, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_START:
		SendControllerButtonEvent( QControllerEvent::BUTTON_START, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_LEFTSTICK:
		SendControllerButtonEvent( QControllerEvent::BUTTON_LEFTSTICK, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
		SendControllerButtonEvent( QControllerEvent::BUTTON_RIGHTSTICK, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
		SendControllerButtonEvent( QControllerEvent::BUTTON_LEFTSHOULDER, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
		SendControllerButtonEvent( QControllerEvent::BUTTON_RIGHTSHOULDER, bPressed );
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		SendGameControllerDPadEvent( pController );
		break;
	}
}


//--------------------------------------------------------------------------------------------------
// Handle game controller axis events
//--------------------------------------------------------------------------------------------------
void CControllerManager::OnGameControllerAxis( GameController_t *pController, int nAxis, int nValue )
{
	if ( BIgnoreGameControllerEvent( pController ) )
		return;

	if ( pController->m_arrAxisValues[ nAxis ] == nValue )
		return;

	pController->m_arrAxisValues[ nAxis ] = nValue;

	if ( nAxis == SDL_CONTROLLER_AXIS_LEFTX || nAxis == SDL_CONTROLLER_AXIS_LEFTY )
	{
		SendGameControllerThumbstickEvent( pController, QControllerEvent::THUMBSTICK_LEFT );
	}
	else if ( nAxis == SDL_CONTROLLER_AXIS_RIGHTX || nAxis == SDL_CONTROLLER_AXIS_RIGHTY )
	{
		SendGameControllerThumbstickEvent( pController, QControllerEvent::THUMBSTICK_RIGHT );
	}
	else if ( nAxis == SDL_CONTROLLER_AXIS_TRIGGERLEFT )
	{
		SendGameControllerTriggerEvent( pController, QControllerEvent::TRIGGER_LEFT );
	}
	else if ( nAxis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT )
	{
		SendGameControllerTriggerEvent( pController, QControllerEvent::TRIGGER_RIGHT );
	}
}


//--------------------------------------------------------------------------------------------------
// Send a game controller dpad event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendGameControllerDPadEvent( GameController_t *pController )
{
	SendControllerDPadEvent(
		pController->m_arrButtonStates[ SDL_CONTROLLER_BUTTON_DPAD_UP ],
		pController->m_arrButtonStates[ SDL_CONTROLLER_BUTTON_DPAD_DOWN ],
		pController->m_arrButtonStates[ SDL_CONTROLLER_BUTTON_DPAD_LEFT ],
		pController->m_arrButtonStates[ SDL_CONTROLLER_BUTTON_DPAD_RIGHT ] );
}


//--------------------------------------------------------------------------------------------------
// Send a game controller thumbstick event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendGameControllerThumbstickEvent( GameController_t *pController, QControllerEvent::EventType eEvent )
{
	float flX, flY;
	float flAngle, flDistance;

	int iThumbstick;
	if ( eEvent == QControllerEvent::THUMBSTICK_LEFT )
	{
		iThumbstick = 0;
		flX = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_LEFTX ] ) / CONTROLLER_AXIS_MAX;
		flY = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_LEFTY ] ) / CONTROLLER_AXIS_MAX;
	}
	else
	{
		iThumbstick = 1;
		flX = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_RIGHTX ] ) / CONTROLLER_AXIS_MAX;
		flY = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_RIGHTY ] ) / CONTROLLER_AXIS_MAX;
	}

	flAngle = ( flX == 0.0f && flY == 0.0f ) ? 0.0f : atan2( flX, -flY );
	if ( flAngle < 0 )
		flAngle += 2 * M_PI;

	flDistance = sqrt( flX * flX + flY * flY );
	flDistance = NormalizeAxisValue( flDistance );

	bool bTilted = ( flDistance > 0.0f );
	if ( !bTilted && !pController->m_arrThumbstickTilted[ iThumbstick ] )
	{
		// Don't send multiple centered events
		return;
	}
	pController->m_arrThumbstickTilted[ iThumbstick ] = bTilted;

	SendControllerThumbstickEvent( eEvent, flAngle, flDistance );
}


//--------------------------------------------------------------------------------------------------
// Send a game controller trigger event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendGameControllerTriggerEvent( GameController_t *pController, QControllerEvent::EventType eEvent )
{
	int iTrigger;
	float flDistance;

	// The trigger range is -CONTROLLER_AXIS_MAX - 1 ... CONTROLLER_AXIS_MAX, so checking > 0 already includes a 50% dead zone
	if ( eEvent == QControllerEvent::TRIGGER_LEFT )
	{
		iTrigger = 0;
		flDistance = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_TRIGGERLEFT ] ) / CONTROLLER_AXIS_MAX;
	}
	else
	{
		iTrigger = 1;
		flDistance = static_cast<float>( pController->m_arrAxisValues[ SDL_CONTROLLER_AXIS_TRIGGERRIGHT ] ) / CONTROLLER_AXIS_MAX;
	}

	bool bTilted = ( flDistance > 0.0f );
	if ( !bTilted && !pController->m_arrTriggerTilted[ iTrigger ] )
	{
		// Don't send multiple centered events
		return;
	}
	pController->m_arrTriggerTilted[ iTrigger ] = bTilted;

	SendControllerTriggerEvent( eEvent, flDistance );
}


//--------------------------------------------------------------------------------------------------
// Cleanup game controller list
//--------------------------------------------------------------------------------------------------
void CControllerManager::QuitGameControllers()
{
	while ( m_vecGameControllers.count() > 0 )
		OnGameControllerRemoved( m_vecGameControllers[ 0 ]->m_nJoystickID );

	if ( m_bInitalizedSDL )
	{
		SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER );
		m_bInitalizedSDL = false;
	}
}


//--------------------------------------------------------------------------------------------------
// Send Qt a controller button event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendControllerButtonEvent( QControllerEvent::EventType eEvent, bool bPressed )
{
	QControllerEvent event;
	event.setButton( eEvent, bPressed );
	SendControllerEvent( &event );
}


//--------------------------------------------------------------------------------------------------
// Send Qt a controller dpad event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendControllerDPadEvent( bool bUp, bool bDown, bool bLeft, bool bRight )
{
	QControllerEvent::Direction direction = QControllerEvent::DIRECTION_CENTER;

	if ( bUp && !bDown )
	{
		if ( bLeft && !bRight )
		{
			direction = QControllerEvent::DIRECTION_NW;
		}
		else if ( bRight && !bLeft )
		{
			direction = QControllerEvent::DIRECTION_NE;
		}
		else
		{
			direction = QControllerEvent::DIRECTION_N;
		}
	}
	else if ( bDown && !bUp )
	{
		if ( bLeft && !bRight )
		{
			direction = QControllerEvent::DIRECTION_SW;
		}
		else if ( bRight && !bLeft )
		{
			direction = QControllerEvent::DIRECTION_SE;
		}
		else
		{
			direction = QControllerEvent::DIRECTION_S;
		}
	}
	else if ( bLeft && !bRight )
	{
		direction = QControllerEvent::DIRECTION_W;
	}
	else if ( bRight && !bLeft )
	{
		direction = QControllerEvent::DIRECTION_E;
	}

	QControllerEvent event;
	event.setDPad( direction );
	SendControllerEvent( &event );
}


//--------------------------------------------------------------------------------------------------
// Send Qt a controller thumbstick event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendControllerThumbstickEvent( QControllerEvent::EventType eEvent, float flAngle, float flDistance )
{
	QControllerEvent event;
	event.setThumbstick( eEvent, flAngle, flDistance );
	SendControllerEvent( &event );
}


//--------------------------------------------------------------------------------------------------
// Send Qt a controller trigger event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendControllerTriggerEvent( QControllerEvent::EventType eEvent, float flDistance )
{
	QControllerEvent event;
	event.setTrigger( eEvent, flDistance );
	SendControllerEvent( &event );
}


//--------------------------------------------------------------------------------------------------
// Send a controller event
//--------------------------------------------------------------------------------------------------
void CControllerManager::SendControllerEvent( QEvent *pEvent )
{
	QWidget *receiver = QApplication::focusWidget();
	while ( receiver )
	{
		if ( QApplication::sendEvent( receiver, pEvent ) && pEvent->isAccepted() && !static_cast<QControllerEvent *>( pEvent )->isButtonRelease() )
		{
			return;
		}

		receiver = receiver->parentWidget();
	}
}


//--------------------------------------------------------------------------------------------------
// Turn Enter / Escape keys into controller buttons so they go to the right UI elements in panels
//--------------------------------------------------------------------------------------------------
bool CControllerManager::eventFilter( QObject *, QEvent *pEvent )
{
	if ( pEvent->type() == QEvent::KeyPress || pEvent->type() == QEvent::KeyRelease )
	{
		QKeyEvent *pKeyEvent = static_cast<QKeyEvent *>( pEvent );
		if ( pKeyEvent->key() == Qt::Key_Enter || pKeyEvent->key() == Qt::Key_Return )
		{
			if ( pEvent->type() == QEvent::KeyPress )
			{
				SendControllerButtonEvent( QControllerEvent::BUTTON_A, true );
			}
			else
			{
				SendControllerButtonEvent( QControllerEvent::BUTTON_A, false );
			}
			return true;
		}
		else if ( pKeyEvent->key() == Qt::Key_Escape )
		{
			if ( pEvent->type() == QEvent::KeyPress )
			{
				SendControllerButtonEvent( QControllerEvent::BUTTON_B, true );
			}
			else
			{
				SendControllerButtonEvent( QControllerEvent::BUTTON_B, false );
			}
			return true;
		}
		else if ( pKeyEvent->key() == Qt::Key_Up || 
				  pKeyEvent->key() == Qt::Key_Down || 
				  pKeyEvent->key() == Qt::Key_Left || 
				  pKeyEvent->key() == Qt::Key_Right )
		{
			if ( pKeyEvent->key() == Qt::Key_Up )
				m_arrArrowKeyState[ k_EArrowKeyUp ] = ( pEvent->type() == QEvent::KeyPress );
			else if ( pKeyEvent->key() == Qt::Key_Down )
				m_arrArrowKeyState[ k_EArrowKeyDown ] = ( pEvent->type() == QEvent::KeyPress );
			else if ( pKeyEvent->key() == Qt::Key_Left )
				m_arrArrowKeyState[ k_EArrowKeyLeft ] = ( pEvent->type() == QEvent::KeyPress );
			else if ( pKeyEvent->key() == Qt::Key_Right )
				m_arrArrowKeyState[ k_EArrowKeyRight ] = ( pEvent->type() == QEvent::KeyPress );

			SendControllerDPadEvent(
				m_arrArrowKeyState[ k_EArrowKeyUp ],
				m_arrArrowKeyState[ k_EArrowKeyDown ],
				m_arrArrowKeyState[ k_EArrowKeyLeft ],
				m_arrArrowKeyState[ k_EArrowKeyRight ] );

			// Left and right arrow keys are used by edit boxes and shouldn't be filtered out
			// Up and down arrow keys are used by list boxes through the controller manager
			if ( pKeyEvent->key() == Qt::Key_Up || pKeyEvent->key() == Qt::Key_Down )
				return true;
		}
	}
	return false;
}


//--------------------------------------------------------------------------------------------------
// Check for controller updates
//--------------------------------------------------------------------------------------------------
void CControllerManager::timerEvent( QTimerEvent *pEvent )
{
	if ( pEvent->timerId() == m_nUpdateTimerID )
	{
		CheckGameControllers();
		return;
	}

	QObject::timerEvent( pEvent );
}
