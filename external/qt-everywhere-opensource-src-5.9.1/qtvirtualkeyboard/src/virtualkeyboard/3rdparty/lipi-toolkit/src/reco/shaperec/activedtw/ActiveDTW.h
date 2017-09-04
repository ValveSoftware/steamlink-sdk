/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies 
* or substantial portions of the Software.
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
* $LastChangedDate: 2011-01-18 15:41:43 +0530 (Tue, 18 Jan 2011) $
* $Revision: 829 $
* $Author: mnab $
*
************************************************************************/
/************************************************************************
* FILE DESCR: Declarations for ActiveDTW dll exporting functions.
*
* CONTENTS:
*
* AUTHOR: S Anand 
*
w
* DATE:  3-MAR-2009  
* CHANGE HISTORY:
* Author       Date            Description of change
* Balaji MNA   18th Jan 2010   Receiving LTKShapeRecognizer as single pointer 
*                              instead of double pointer in deleteShapeRecognizer
************************************************************************/

#ifndef __ACTIVEDTW_H__
#define __ACTIVEDTW_H__

#include "LTKInc.h"
#include "LTKTypes.h"

#ifdef _WIN32
#ifdef ACTIVEDTW_EXPORTS
#define ACTIVEDTW_API __declspec(dllexport)
#else
#define ACTIVEDTW_API __declspec(dllimport)
#endif //#ifdef ACTIVEDTW_EXPORTS
#else
#define ACTIVEDTW_API
#endif	//#ifdef _WIN32

class LTKTraceGroup;
class LTKShapeRecognizer;


/** @defgroup ActiveDTWShapeRecognizer ActiveDTWShapeRecognizer
*@brief The ActiveDTWShapeRecognizer
*/

/**
* @ingroup ActiveDTWShapeRecognizer
* @file ActiveDTW.cpp
*/

/**
*  Crates instance of type ActiveDTWShapeRecognizer and returns of type 
*  LTKShpeRecognizer. (Acts as a Factory Method).
*
* @param  none
*
* @return  LTKShapeRecognizer - an instance of type LTKShapeRecognizer.
*/
extern "C" ACTIVEDTW_API int createShapeRecognizer(const LTKControlInfo& controlInfo, 
												   LTKShapeRecognizer** pReco );
												   /**
												   * Destroy the instance by taking the address as its argument.
												   *
												   * @param  obj - Address of LTKShapeRecognizer instance.
												   *
												   * @return  0 on Success
*/
extern "C" ACTIVEDTW_API int deleteShapeRecognizer(LTKShapeRecognizer *obj);

extern "C" ACTIVEDTW_API int getTraceGroups(LTKShapeRecognizer *obj, int shapeID, int numberOfTraceGroups,vector<LTKTraceGroup> &outTraceGroups);

void unloadDLLs();

#endif //#ifndef __NN_H__
