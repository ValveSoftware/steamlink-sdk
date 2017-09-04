/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate$
 * $Revision$
 * $Author$
 *
 ************************************************************************/
/***************************************************************************
 * FILE DESCR: DLL Implementation file for logger module
 *
 * CONTENTS:
 *  DllMain
 *  
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       April 01, 2008
 *
 * CHANGE HISTORY:
 *
 * Author       Date            Description of change
 ************************************************************************/
#include "logger.h"
#include "LTKLoggerUtil.h"
#include "LTKLoggerInterface.h"
#include "LTKLogger.h"
#include "LTKMacros.h"
#include "LTKErrorsList.h"



LTKLoggerInterface* ptrLog = LTKLogger::getInstance();

/************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : DllMain
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
************************************************************************/
#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call,
					  LPVOID lpReserved)
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}
#endif // #ifdef _WIN32

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : getLoggerInstance
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
LTKLoggerInterface* getLoggerInstance()
{
	return ptrLog;
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : destroyLogger
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
void destroyLogger()
{
	ptrLog = NULL;
	LTKLogger::destroyLoggerInstance();
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : setLogFileName
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
void setLoggerFileName(const string& logFileName)
{
	if (!ptrLog) return;
	ptrLog->setLogFileName(logFileName);
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : setLogLevel
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
int setLoggerLevel(LTKLogger::EDebugLevel logLevel)
{
	if (!ptrLog) return FAILURE;
	return ptrLog->setLogLevel(logLevel);
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 01-APR-2008
* NAME                : getLogFileName
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
const string& getLoggerFileName()
{
    static string emptyStr;
    if (!ptrLog) return emptyStr;
    return ptrLog->getLogFileName();
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 03-APR-2008
* NAME                : getLogLevel
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
LTKLogger::EDebugLevel getLoggerLevel()
{
    if (!ptrLog) return LTKLogger::LTK_LOGLEVEL_OFF;
    return ptrLog->getLogLevel();
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 02-APR-2008
* NAME                : startLog
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
void startLogger()
{
    if (!ptrLog) return;
    ptrLog->startLog();
}

/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 02-APR-2008
* NAME                : startLog
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
void stopLogger()
{
    if (!ptrLog) return;
    ptrLog->stopLog();
}


/***************************************************************************
* AUTHOR            : Nidhi Sharma 
* DATE                : 08-APR-2008
* NAME                : logMessage
* DESCRIPTION   : 
* ARGUMENTS      : 
* RETURNS          : 
* NOTES              :
* CHANGE HISTROY
* Author            Date                Description of change
***************************************************************************/
ostream& logMessage(LTKLogger::EDebugLevel logLevel, const string& fileName,
                       int lineNumber)
{
    if (!ptrLog) return LTKLoggerUtil::m_emptyStream;
    return (*ptrLog)(logLevel, fileName, lineNumber);
}