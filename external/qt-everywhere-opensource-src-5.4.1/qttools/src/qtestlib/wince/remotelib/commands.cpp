/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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
#include "commands.h"
#include <Pm.h>
#include <Pmpolicy.h>


#define CLEAN_FAIL(a) {delete appName; \
    delete arguments; \
    return (a); }

int qRemoteLaunch(DWORD, BYTE*, DWORD*, BYTE**, IRAPIStream* stream)
{
    if (!stream)
        return -1;

    DWORD bytesRead;
    int appLength;
    wchar_t* appName = 0;
    int argumentsLength;
    wchar_t* arguments = 0;
    int timeout = -1;
    int returnValue = -2;
    DWORD error = 0;

    if (S_OK != stream->Read(&appLength, sizeof(appLength), &bytesRead))
        CLEAN_FAIL(-2);
    appName = (wchar_t*) malloc(sizeof(wchar_t)*(appLength + 1));
    if (S_OK != stream->Read(appName, sizeof(wchar_t)*appLength, &bytesRead))
        CLEAN_FAIL(-2);
    appName[appLength] = '\0';

    if (S_OK != stream->Read(&argumentsLength, sizeof(argumentsLength), &bytesRead))
        CLEAN_FAIL(-2);
    arguments = (wchar_t*) malloc(sizeof(wchar_t)*(argumentsLength + 1));
    if (S_OK != stream->Read(arguments, sizeof(wchar_t)*argumentsLength, &bytesRead))
        CLEAN_FAIL(-2);
    arguments[argumentsLength] = '\0';

    if (S_OK != stream->Read(&timeout, sizeof(timeout), &bytesRead))
        CLEAN_FAIL(-2);

    bool result = qRemoteExecute(appName, arguments, &returnValue, &error, timeout);

    if (timeout != 0) {
        if (S_OK != stream->Write(&returnValue, sizeof(returnValue), &bytesRead))
            CLEAN_FAIL(-4);
        if (S_OK != stream->Write(&error, sizeof(error), &bytesRead))
            CLEAN_FAIL(-5);
    }
    delete appName;
    delete arguments;
    // We need to fail here for the execute, otherwise the calling application will wait
    // forever for the returnValue.
    if (!result)
        return -3;
    return S_OK;
}


bool qRemoteExecute(const wchar_t* program, const wchar_t* arguments, int *returnValue, DWORD* error, int timeout)
{
    *error = 0;

    if (!program)
        return false;

    PROCESS_INFORMATION pid;
    if (!CreateProcess(program, arguments, NULL, NULL, false, 0, NULL, NULL, NULL, &pid)) {
        *error = GetLastError();
        wprintf(L"Could not launch: %s\n", program);
        return false;
    }

    // Timeout is in seconds
    DWORD waitingTime = (timeout == -1) ? INFINITE : timeout * 1000;

    if (waitingTime != 0) {
        DWORD waitStatus = WaitForSingleObject(pid.hProcess, waitingTime);
        if (waitStatus == WAIT_TIMEOUT) {
            TerminateProcess(pid.hProcess, 2);
            return false;
        } else if (waitStatus == WAIT_OBJECT_0 && returnValue) {
            *returnValue = 0;
            DWORD exitCode;
            if (GetExitCodeProcess(pid.hProcess, &exitCode))
                *returnValue = exitCode;
        }
    }
    return true;
}
/**
\brief Reset the device.
*/
int qRemoteSoftReset(DWORD, BYTE*, DWORD*, BYTE**, IRAPIStream* stream)
{
    //POWER_STATE_ON        On state
    //POWER_STATE_OFF       Off state
    //POWER_STATE_CRITICAL  Critical state
    //POWER_STATE_BOOT      Boot state
    //POWER_STATE_IDLE      Idle state
    //POWER_STATE_SUSPEND   Suspend state
    //POWER_STATE_RESET     Reset state

    DWORD returnValue  = SetSystemPowerState(0, POWER_STATE_RESET, POWER_FORCE);
    return returnValue;
}

/**
\brief Toggle the unattended powermode of the device
*/
int qRemoteToggleUnattendedPowerMode(DWORD, BYTE*, DWORD*, BYTE**, IRAPIStream* stream)
{
    if (!stream)
        return -1;

    DWORD bytesRead;
    int toggleVal   = 0;
    int returnValue = S_OK;

    if (S_OK != stream->Read(&toggleVal, sizeof(toggleVal), &bytesRead))
        return -2;

    //PPN_REEVALUATESTATE 0x0001 Reserved. Set dwData to zero (0).
    //PPN_POWERCHANGE 0x0002 Reserved. Set dwData to zero (0).
    //PPN_UNATTENDEDMODE 0x0003 Set dwData to TRUE or FALSE.
    //PPN_SUSPENDKEYPRESSED or
    //PPN_POWERBUTTONPRESSED 0x0004 Reserved. Set dwData to zero (0).
    //PPN_SUSPENDKEYRELEASED 0x0005 Reserved. Set dwData to zero (0).
    //PPN_APPBUTTONPRESSED 0x0006 Reserved. Set dwData to zero (0).
    //PPN_OEMBASE Greater than or equal to 0x10000
    //You can define higher values, such as 0x10001, 0x10002, and so on.
    // Reserved. Set dwData to zero (0).
    returnValue = PowerPolicyNotify(PPN_UNATTENDEDMODE, toggleVal);

    if (S_OK != stream->Write(&returnValue, sizeof(returnValue), &bytesRead))
        return -3;
    else
        return S_OK;
}

/**
\brief Virtually press the power button of the device
*/
int qRemotePowerButton(DWORD, BYTE*, DWORD*, BYTE**, IRAPIStream* stream)
{
    if (!stream)
        return -1;

    DWORD bytesRead;
    int toggleVal   = 0;
    int returnValue = S_OK;

    if (S_OK != stream->Read(&toggleVal, sizeof(toggleVal), &bytesRead))
        return -2;

    //PPN_REEVALUATESTATE 0x0001 Reserved. Set dwData to zero (0).
    //PPN_POWERCHANGE 0x0002 Reserved. Set dwData to zero (0).
    //PPN_UNATTENDEDMODE 0x0003 Set dwData to TRUE or FALSE.
    //PPN_SUSPENDKEYPRESSED or
    //PPN_POWERBUTTONPRESSED 0x0004 Reserved. Set dwData to zero (0).
    //PPN_SUSPENDKEYRELEASED 0x0005 Reserved. Set dwData to zero (0).
    //PPN_APPBUTTONPRESSED 0x0006 Reserved. Set dwData to zero (0).
    //PPN_OEMBASE Greater than or equal to 0x10000
    //You can define higher values, such as 0x10001, 0x10002, and so on.
    // Reserved. Set dwData to zero (0).
    returnValue = PowerPolicyNotify(PPN_SUSPENDKEYPRESSED, 0);

    if (S_OK != stream->Write(&returnValue, sizeof(returnValue), &bytesRead))
        return -3;
    else
        return S_OK;
}

