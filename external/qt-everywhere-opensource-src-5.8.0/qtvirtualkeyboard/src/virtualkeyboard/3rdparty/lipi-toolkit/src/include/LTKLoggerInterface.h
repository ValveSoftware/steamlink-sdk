/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
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
/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2007-09-17 19:00:29 +0530 (Mon, 17 Sep 2007) $
 * $Revision: 209 $
 * $Author: madant $
 *
 ************************************************************************/

#ifndef __LTKLoggerINTERFACE_H_
#define __LTKLoggerINTERFACE_H_

#include <ostream>

using namespace std;

class LTKLoggerInterface
{
public:
     // create dummy stream which just dumps all output
    // this is what we use if the DebugLevel is higher than the message importance
    // this stream sets a failbit and hence won't attempt to process it's input

    enum ELogStatus
    {
    	INACTIVE,
        ACTIVE
    };

    /*
        description of the debug levels
        	ALL		writes all messages
        	DEBUG	writes DEBUG, INFO and ERR messages
        	INFO	writes INFO and ERR messages
        	ERR		writes ERROR messages
        	OFF		does not log any messages
    */


    enum EDebugLevel
    {
		LTK_LOGLEVEL_ALL,
		LTK_LOGLEVEL_VERBOSE,
		LTK_LOGLEVEL_DEBUG,
        LTK_LOGLEVEL_INFO,
        LTK_LOGLEVEL_ERR,
        LTK_LOGLEVEL_OFF
    };

public:

	virtual ostream& operator()(const EDebugLevel& debugLevel, const string &fileName, int lineNumber = 0) = 0;

	virtual int setLogLevel(const EDebugLevel& debugLevel) = 0;

	/**
	 * This is a  method sets the name of the log file to be used
	 * @param logFileName Name of the log file to be used for logging
	 * @param traceGroup trace group into which the ink file has to be read into
	 * @return SUCCESS on successful set operation
	 */

	 virtual void setLogFileName(const string& logFileName) = 0;

	/**
	 * This is a  method which returns the log level of the messages to be considered for
	 * redirecting to the log file
	 * @return the log level of the messages to be considered for redirecting to the log file
	 */

	 virtual EDebugLevel getLogLevel() = 0;

	/**
	 * This is a  method which returns the name of the file logging is done into
	 * @return the name of the file logging is done into
	 */

	 virtual const string& getLogFileName() = 0 ;

	/**
	 * This is a  method which starts the logging operation
	 * @return SUCCESS on successful start of logging
	 */

	 virtual int startLog(bool timeStamp = true) = 0;

	 virtual int stopLog() = 0;

 	/**
	 * Pure Virtual Destructor
	 */
     virtual ~LTKLoggerInterface(){};

};

#endif	//#ifndef __LTKLoggerINTERFACE_H_


