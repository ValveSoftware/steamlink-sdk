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
 * $LastChangedDate: 2007-09-12 15:47:40 +0530 (Wed, 12 Sep 2007) $
 * $Revision: 188 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of the Debug Logging Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef LTKLOGGER_H
#define LTKLOGGER_H

#include "LTKInc.h"
#include "LTKLoggerInterface.h"

/**
* @ingroup util
*/

/** @brief A concrete class that implements the logger interface
* @class LTKLogger 
*/
class LTKLogger : public LTKLoggerInterface 
{

private:

	 EDebugLevel m_debugLevel;	//	log level which determines which log messages are to be written into the file

	 string m_logFileName;		//	name of the log file

	 ofstream m_logFile;		//	file pointer to the log file

	 ofstream m_ofstream;		//  used to ignore messages with priority lower than defined by application

	 ELogStatus m_logStatus;	//	status of logging - active or inactive

	 bool m_isTimeStamped;		//	flag to indicate if time is to be printed on the log file along with log message

     static LTKLoggerInterface* loggerInstance;

	/**
	 * This is a  method which writes auxiliary information like date, time and line number
	 * into the log file
	 * @param lineNumber - line number of the log message
	 * @return SUCCESS on successful writing of auxiliary information into log file
	 */

	 int writeAuxInfo(const string& fileName, int lineNumber);

    /**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKLogger();

public:

	static void destroyLoggerInstance();

    static LTKLoggerInterface* getInstance();

	/**
	* Destructor
	*/

	virtual ~LTKLogger();

	// @}

	/**
	 * @name Function call operator
	 */
	//@{

	/**
	 * Function call operator
	 * @param debugLevel reference to the log level of the message
	 * @param lineNumber source code line number of the log message
	 *
	 * @return reference to an output stream object
	 */
	
	ostream& operator()(const EDebugLevel& debugLevel, const string& fileName, int lineNumber = 0);
	
	// @}
	
	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This is a  method sets the log level of the messages to be considered for 
	 * redirecting to the log file
	 * @param debugLevel log level of the messages to be considered
	 * @return SUCCESS on successful set operation
	 */

	 int setLogLevel(const EDebugLevel& debugLevel);

	/**
	 * This is a  method sets the name of the log file to be used
	 * @param logFileName Name of the log file to be used for logging
	 * @param traceGroup trace group into which the ink file has to be read into
	 * @return SUCCESS on successful set operation
	 */

	 void setLogFileName(const string& logFileName);

	/**
	 * This is a  method which returns the log level of the messages to be considered for 
	 * redirecting to the log file
	 * @return the log level of the messages to be considered for redirecting to the log file
	 */

	 EDebugLevel getLogLevel();

	/**
	 * This is a  method which returns the name of the file logging is done into
	 * @return the name of the file logging is done into
	 */

	 const string& getLogFileName() ;

	/**
	 * This is a  method which starts the logging operation
	 * @return SUCCESS on successful start of logging
	 */

	 int startLog(bool timeStamp = true);

	 /**
	  * This method will stop the logging operation.
	  * @return SUCCESS on successful start of logging
	  */

	 int stopLog();

	// @}

	
};

#endif

