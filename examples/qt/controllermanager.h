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
#pragma once

#include <QtWidgets>


//--------------------------------------------------------------------------------------------------
// Class to handle controller input
//--------------------------------------------------------------------------------------------------
class CControllerManager : public QObject
{
public:
	CControllerManager( QObject *pParent );
	virtual ~CControllerManager();

private:
	virtual bool eventFilter( QObject *pObject, QEvent *pEvent );
	virtual void timerEvent( QTimerEvent *pEvent );

private:
	struct GameController_t;

	bool BInitGameControllers();
	bool BShouldIgnoreController( const char *pszGUID );
	void CheckGameControllers();
	void OnGameControllerAdded( int iJoystick );
	void OnGameControllerRemoved( int nJoystickID );
	GameController_t *GetGameController( int nJoystickID );
	bool BIgnoreGameControllerEvent( GameController_t *pController );
	void OnGameControllerButton( GameController_t *pController, int nButton, bool bPressed );
	void OnGameControllerAxis( GameController_t *pController, int nAxis, int nValue );
	void SendGameControllerDPadEvent( GameController_t *pController );
	void SendGameControllerThumbstickEvent( GameController_t *pController, QControllerEvent::EventType eEvent );
	void SendGameControllerTriggerEvent( GameController_t *pController, QControllerEvent::EventType eEvent );
	void QuitGameControllers();

	void SendControllerButtonEvent( QControllerEvent::EventType eEvent, bool bPressed );
	void SendControllerDPadEvent( bool bUp, bool bDown, bool bLeft, bool bRight );
	void SendControllerThumbstickEvent( QControllerEvent::EventType eEvent, float flAngle, float flDistance );
	void SendControllerTriggerEvent( QControllerEvent::EventType eEvent, float flDistance );
	void SendControllerEvent( QEvent *pEvent );

private:
	bool m_bInitalizedSDL;
	int m_nUpdateTimerID;

	QVector<GameController_t*> m_vecGameControllers;

	enum EArrowKeys
	{
		k_EArrowKeyUp,
		k_EArrowKeyDown,
		k_EArrowKeyLeft,
		k_EArrowKeyRight,
		k_EArrowKeysMax,
	};
	bool m_arrArrowKeyState[ k_EArrowKeysMax ];
};

