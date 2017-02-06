/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*****************************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-04 11:43:39 +0530 (Fri, 04 Jul 2008) $
 * $Revision: 544 $
 * $Author: sharmnid $
 *
 ************************************************************************/
 
/************************************************************************
 * FILE DESCR: Implementation of the String Splitter Module
 *
 * CONTENTS: 
 *	tokenizeString
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKLoggerUtil.h"
#include "LTKOSUtil.h"
#include "LTKOSUtilFactory.h"
#include "LTKLogger.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <memory>

void* LTKLoggerUtil::m_libHandleLogger = NULL;
FN_PTR_LOGMESSAGE LTKLoggerUtil::module_logMessage = NULL;
FN_PTR_STARTLOG LTKLoggerUtil::module_startLogger = NULL;
FN_PTR_GETINSTANCE LTKLoggerUtil::module_getInstanceLogger = NULL;
FN_PTR_DESTROYINSTANCE LTKLoggerUtil::module_destroyLogger = NULL;
#ifdef _WIN32
ofstream LTKLoggerUtil::m_emptyStream;
#else
ofstream LTKLoggerUtil::m_emptyStream("/dev/null");
#endif

/****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 09-Jul-2007
* NAME			: LTKLoggerUtil
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
****************************************************************************/

LTKLoggerUtil::LTKLoggerUtil(){}



/****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 09-Jul-2007
* NAME			: createLogger
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
****************************************************************************/

int LTKLoggerUtil::createLogger(const string& lipiLibPath)
{
    void* functionHandle = NULL; 
    auto_ptr<LTKOSUtil> a_ptrOSUtil(LTKOSUtilFactory::getInstance());

    int iErrorCode = a_ptrOSUtil->loadSharedLib(lipiLibPath,
                                                LOGGER_MODULE_STR, 
                                                &m_libHandleLogger);

	
    if(iErrorCode != SUCCESS)
    {
        return iErrorCode;
    }

    // Create logger instance
    if (module_getInstanceLogger == NULL)
    {
        iErrorCode = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                     "getLoggerInstance", 
                                                     &functionHandle);
        if(iErrorCode != SUCCESS)
    	{
    	    return iErrorCode;
    	}

        module_getInstanceLogger = (FN_PTR_GETINSTANCE)functionHandle;

    	functionHandle = NULL;
    }

    module_getInstanceLogger();

    // map destoylogger function
    if (module_destroyLogger == NULL)
    {
        iErrorCode = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                     "destroyLogger", 
                                                     &functionHandle);
        if(iErrorCode != SUCCESS)
    	{
    	    return iErrorCode;
    	}

        module_destroyLogger = (FN_PTR_DESTROYINSTANCE)functionHandle;

    	functionHandle = NULL;
    }
    
    return iErrorCode;
    
}

/*****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 
* NAME			: destroyLogger
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/
int LTKLoggerUtil::destroyLogger()
{
    auto_ptr<LTKOSUtil> a_ptrOSUtil(LTKOSUtilFactory::getInstance());

    if (module_destroyLogger != NULL )
    {
        module_destroyLogger();
    }

    int returnVal = a_ptrOSUtil->unloadSharedLib(m_libHandleLogger);

    return returnVal;
}


/*****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 
* NAME			: getAddressLoggerFunctions
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/
int LTKLoggerUtil::configureLogger(const string& logFile, LTKLogger::EDebugLevel logLevel)
{
     void* functionHandle = NULL; 
     int returnVal = SUCCESS;

     FN_PTR_SETLOGFILENAME module_setLogFileName = NULL;
     FN_PTR_SETLOGLEVEL module_setLogLevel = NULL;

    if (m_libHandleLogger == NULL )
    {
        LTKReturnError(ELOGGER_LIBRARY_NOT_LOADED);
    }
    
    auto_ptr<LTKOSUtil> a_ptrOSUtil(LTKOSUtilFactory::getInstance());

    if ( logFile.length() != 0 )
    {
        returnVal = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                    "setLoggerFileName", 
                                                    &functionHandle);

        if(returnVal != SUCCESS)
    	{
    	    return returnVal;
    	}

        module_setLogFileName = (FN_PTR_SETLOGFILENAME)functionHandle;

    	functionHandle = NULL;

        module_setLogFileName(logFile);
    	
    }
    else
    {
		LTKReturnError(EINVALID_LOG_FILENAME); 
    }
    
    returnVal = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                "setLoggerLevel", 
                                                &functionHandle);

    if(returnVal != SUCCESS)
	{
	    LTKReturnError(returnVal);
	}

    module_setLogLevel = (FN_PTR_SETLOGLEVEL)functionHandle;

	functionHandle = NULL;

    module_setLogLevel(logLevel);

    return SUCCESS;
    
}


/*****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 
* NAME			: getAddressLoggerFunctions
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/
int LTKLoggerUtil::getAddressLoggerFunctions()
{
    void* functionHandle = NULL; 
    int returnVal = SUCCESS;

    auto_ptr<LTKOSUtil> a_ptrOSUtil;

    //start log
    
    if (module_startLogger == NULL )
    {
        if(!a_ptrOSUtil.get())
            a_ptrOSUtil.reset(LTKOSUtilFactory::getInstance());
        returnVal = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                    "startLogger", 
                                                    &functionHandle);

        if(returnVal != SUCCESS)
    	{
    	    LTKReturnError(returnVal);
    	}

        module_startLogger = (FN_PTR_STARTLOG)functionHandle;

    	functionHandle = NULL;
    }

    module_startLogger();

    // map Log message
    if (module_logMessage == NULL)
    {
        if(!a_ptrOSUtil.get())
            a_ptrOSUtil.reset(LTKOSUtilFactory::getInstance());
        returnVal = a_ptrOSUtil->getFunctionAddress(m_libHandleLogger,
                                                    "logMessage", 
                                                    &functionHandle);

        if(returnVal != SUCCESS)
    	{
    	    LTKReturnError(returnVal);
    	}

        module_logMessage = (FN_PTR_LOGMESSAGE)functionHandle;

    	functionHandle = NULL;    
    
    }
    
	
	return SUCCESS;
	
}

/*****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 15-Jul-2008
* NAME			: nidhi
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/


ostream& LTKLoggerUtil::logMessage(LTKLogger::EDebugLevel logLevel, string inStr, int lineNumber)
{
	if (m_libHandleLogger == NULL)
	{
        auto_ptr<LTKOSUtil> a_ptrOSUtil(LTKOSUtilFactory::getInstance());
        m_libHandleLogger = a_ptrOSUtil->getLibraryHandle(LOGGER_MODULE_STR);

		if (m_libHandleLogger == NULL)
		{
			return m_emptyStream;
		}
	}


	// get function addresses
    if ( module_startLogger == NULL ||
        module_logMessage == NULL )
    {
        int returnVal = getAddressLoggerFunctions();

        if(returnVal != SUCCESS)
    	{
    	    return m_emptyStream;
    	}
    }

    return module_logMessage(logLevel, inStr, lineNumber);
}
