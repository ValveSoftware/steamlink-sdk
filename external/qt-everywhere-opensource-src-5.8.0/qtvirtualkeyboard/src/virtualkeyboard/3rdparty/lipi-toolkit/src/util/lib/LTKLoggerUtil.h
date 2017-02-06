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
 * $LastChangedDate:$
 * $Revision:  $
 * $Author:  $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for the String Splitter Module
 *
 * CONTENTS: 
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKLOGGERUTIL_H
#define __LTKLOGGERUTIL_H

#include "LTKInc.h"
#include "LTKLogger.h"
class LTKOSUtil;

typedef LTKLoggerInterface* (*FN_PTR_GETINSTANCE)();
typedef void (*FN_PTR_DESTROYINSTANCE)();
typedef void (*FN_PTR_STARTLOG)();
typedef void (*FN_PTR_SETLOGFILENAME)(const string&);
typedef void (*FN_PTR_SETLOGLEVEL)(LTKLogger::EDebugLevel);
typedef ostream& (*FN_PTR_LOGMESSAGE)(int, const string& , int );  

// Set to 1 to disable the logging functionality
#define DISABLE_LOG 1

#define LOG(EDebugLevel) if(!DISABLE_LOG) LTKLoggerUtil::logMessage(EDebugLevel, __FILE__, __LINE__)

/**
* @ingroup util
*/

/** @brief Utility class to tokenize a string on any given delimiter
@class LTKStringUtil
*/

class LTKLoggerUtil
{

public:

   /**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKLoggerUtil();


	// @}

	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This is a static method which splits a string at the specified delimiters
	 * @param str String to be split
	 * @param tokens The split sub-strings
	 * @param delimiters The symbols at which the string is to be split at
	 * @return SUCCESS on successful split operation
	 */

	static int createLogger(const string& lipiLibPath);

    static int destroyLogger();

    static int configureLogger(const string& logFile, LTKLogger::EDebugLevel logLevel);

    static int getAddressLoggerFunctions();

    static ostream& logMessage(LTKLogger::EDebugLevel logLevel, string, int);

    static ofstream m_emptyStream;


	// @}

    private:
    static void* m_libHandleLogger;

    static FN_PTR_LOGMESSAGE module_logMessage;

    static FN_PTR_STARTLOG module_startLogger;

    static FN_PTR_GETINSTANCE module_getInstanceLogger;

    static FN_PTR_DESTROYINSTANCE module_destroyLogger;

    

    
};

#endif	//#ifndef __LTKSTRINGTOKENIZER_H

