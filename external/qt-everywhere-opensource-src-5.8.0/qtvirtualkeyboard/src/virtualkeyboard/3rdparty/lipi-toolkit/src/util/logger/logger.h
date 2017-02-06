/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate$
 * $Revision$
 * $Author$
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definitions for Logger dll exporting functions.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       01-April-2008
 *
 * CHANGE HISTORY:
 *
 * Author		Date			Description
 ************************************************************************/
#ifndef __LOGGERDLL_H__
#define __LOGGERDLL_H__

#ifndef _WIN32
#include  <dlfcn.h>
#else
#include <windows.h>
#endif


#ifdef _WIN32
#ifdef LOGGER_EXPORTS
#define LOGGER_API __declspec(dllexport)
#else
#define LOGGER_API __declspec(dllimport)
#endif //#ifdef LOGGER_EXPORTS
#else
#define LOGGER_API
#endif	//#ifdef _WIN32

class LTKLoggerInterface;
#include "LTKLogger.h"
#include "LTKTypes.h"


extern "C" LOGGER_API LTKLoggerInterface* getLoggerInstance();

extern "C" LOGGER_API void destroyLogger();

extern "C" LOGGER_API void setLoggerFileName(const string& logFileName);

extern "C" LOGGER_API int setLoggerLevel(LTKLogger::EDebugLevel logLevel);

extern "C" LOGGER_API const string& getLoggerFileName();

extern "C" LOGGER_API LTKLogger::EDebugLevel getLoggerLevel();

extern "C" LOGGER_API void startLogger();

extern "C" LOGGER_API void stopLogger();

extern "C" LOGGER_API ostream& logMessage(LTKLogger::EDebugLevel, 
                                             const string& fileName,
                                             int lineNumber);




#endif //#ifndef __LOGGERDLL_H__
