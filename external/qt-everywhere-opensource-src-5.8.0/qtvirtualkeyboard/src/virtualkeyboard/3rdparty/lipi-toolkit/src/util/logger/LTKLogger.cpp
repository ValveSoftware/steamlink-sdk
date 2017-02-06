/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
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
 * $LastChangedDate: 2008-01-10 14:36:54 +0530 (Thu, 10 Jan 2008) $
 * $Revision: 353 $
 * $Author: charakan $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of the Debug Logging Module
 *
 * CONTENTS:
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKLogger.h"
#include "LTKMacros.h"
#include "LTKErrorsList.h"
#include "LTKOSUtil.h"
#include "LTKOSUtilFactory.h"

LTKLoggerInterface* LTKLogger::loggerInstance = NULL;

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKLogger
* DESCRIPTION	: Default Constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKLogger::LTKLogger():
m_debugLevel(LTKLogger::LTK_LOGLEVEL_ERR),
m_logFileName(DEFAULT_LOG_FILE),
m_logStatus(INACTIVE),
m_isTimeStamped(true)
{
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKLogger
* DESCRIPTION	: destructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKLogger::~LTKLogger()
{
	stopLog();
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator()
* DESCRIPTION	: overloaded function call operator
* ARGUMENTS		: msgDebugLevel - log level of the messages to be logged
* RETURNS		: reference to an output stream object
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

ostream& LTKLogger::operator()(const EDebugLevel& msgDebugLevel, 
                               const string& fileName, 
                               int lineNumber)
{
	if ( m_debugLevel <= msgDebugLevel )
	{
		writeAuxInfo(fileName, lineNumber);
        
		switch ( msgDebugLevel )
		{

			case LTKLogger::LTK_LOGLEVEL_ALL:
											m_logFile << "[All] ";
											break;
			
			case LTKLogger::LTK_LOGLEVEL_VERBOSE:
											m_logFile << "[Verbose] ";
											break;
			case LTKLogger::LTK_LOGLEVEL_DEBUG:
											m_logFile << "[Debug] ";
											break;

			case LTKLogger::LTK_LOGLEVEL_INFO:
											m_logFile << "[Info] ";
											break;

			case LTKLogger::LTK_LOGLEVEL_ERR:
											m_logFile << "[Error] ";
											break;
		}
        
		m_logFile.flush();

        return m_logFile;
	}

    // if m_debugLevel is higher than the message importance,
	// then don't attempt to process it's input
	return m_ofstream;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: writeAuxInfo
* DESCRIPTION	: writes the data, time, file, line no. information into the log file
* ARGUMENTS		: fileName - name of file containing the log message
*				  lineNumber - line number of the log message
* RETURNS		: SUCCESS on successful writing of the auxiliary information
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKLogger::writeAuxInfo(const string& fileName, int lineNumber)
{

	#ifdef LOGGEROFF
	return FAILURE;
	#endif

	if(m_isTimeStamped)
	{
		LTKOSUtil *pOSUtil = LTKOSUtilFactory::getInstance() ;
		
		string formattedTimeStr ;
		pOSUtil->getSystemTimeString(formattedTimeStr) ;
		m_logFile << formattedTimeStr << ' ';
		delete pOSUtil ;

		/*
		time_t rawtime;

		time(&rawtime);

		string timeStr = ctime(&rawtime);

		string formattedTimeStr = timeStr.substr(0, 24);

		m_logFile << formattedTimeStr << ' ';
		*/
	}

	m_logFile << fileName.substr(fileName.find_last_of(SEPARATOR) + 1, fileName.size());

	if(lineNumber != 0)
	{
		m_logFile << '(' << lineNumber << "): ";
	}

	return SUCCESS;
}


/***************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setLogLevel
* DESCRIPTION	: sets the level of the messages to be logged
* ARGUMENTS		: debugLevel - level of the messages to be logged
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
****************************************************************************/

int LTKLogger::setLogLevel(const EDebugLevel& debugLevel)
{
	#ifdef LOGGEROFF
	return FAILURE;
	#endif

	m_debugLevel = debugLevel;

	return SUCCESS;
}

/****************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setLogFileName
* DESCRIPTION	: sets the name of the file to be used for logging
* ARGUMENTS		: logFileName - name of the file to be used for logging
* RETURNS		: Nothing
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Balaji MNA        07-Apr-2009         To fix bug:	LIPTKT-600
*****************************************************************************/

void LTKLogger::setLogFileName(const string& logFileName)
{

	#ifdef LOGGEROFF
	    return;
	#endif

	m_logStatus = INACTIVE;
	m_logFileName = logFileName;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getLogLevel
* DESCRIPTION	: returns the log of messages to be logged
* ARGUMENTS		:
* RETURNS		: log of messages to be logged
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKLogger::EDebugLevel LTKLogger::getLogLevel()
{
	#ifdef LOGGEROFF
	return string("Logger is off");
	#endif
    return m_debugLevel;
}

/*****************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getLogFileName
* DESCRIPTION	: returns the name of the log file
* ARGUMENTS		:
* RETURNS		: name of the log file
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

const string& LTKLogger::getLogFileName()
{
	return m_logFileName;
}

/****************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: startLog
* DESCRIPTION	: starts the logging operation
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful start of logging
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
****************************************************************************/

int LTKLogger::startLog(bool isTimeStamped)
{
    
	if (m_debugLevel == LTK_LOGLEVEL_OFF )
        return FAILURE;

	if(m_logStatus == INACTIVE)
	{
		m_isTimeStamped = isTimeStamped;

		if(m_logFileName != "")
		{
			m_logFile.close();

			m_logFile.clear();

			m_logFile.open(m_logFileName.c_str(), ios::app);
		}

		if(m_logFileName == "" || !m_logFile)
		{
			//	log the non-existance of m_logFileName file path is unable to open m_logFileName
		
			return (ELOG_FILE_NOT_EXIST);			

			m_logFile.close();

			m_logFile.clear();

			m_logFile.open(m_logFileName.c_str(), ios::app);
		}

		m_logStatus = ACTIVE;
	}

	return SUCCESS;
}

/****************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 12-SEPT-2005
* NAME			: getInstance
* DESCRIPTION	: This file will return the address of the Logger instance.
* ARGUMENTS		:
* RETURNS		: Address of the Logger instance.
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Thanigai			28-NOV-05			returning the address of global Logger object
*										is giving runtime error, because the vfptr is not
*										getting initialized(vtable). So implementing the
*										workaround by allocating a new Logger object runtime.
*****************************************************************************/

LTKLoggerInterface* LTKLogger::getInstance()
{
	if(loggerInstance == NULL)
	{
		loggerInstance = new LTKLogger();
	}

	return loggerInstance;

}

/****************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 12-SEPT-2005
* NAME			: destroyLoggerInstance
* DESCRIPTION	: 
* ARGUMENTS		:
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/
void LTKLogger::destroyLoggerInstance()
{
    if (loggerInstance != NULL)
    {
        delete loggerInstance;
        loggerInstance = NULL;
    }
}

/****************************************************************************
* AUTHOR		: Thanigai
* DATE			: 12-SEPT-2005
* NAME			: stopLog
* DESCRIPTION	: stops the logging operation
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful stop of logging
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/

int LTKLogger::stopLog()
{
    if (m_debugLevel == LTK_LOGLEVEL_OFF )
        return FAILURE;

	if(m_logFileName != "")
	{
		m_logFile.close();
		m_logStatus = INACTIVE;
		return 0;
	}
	return FAILURE;
}

