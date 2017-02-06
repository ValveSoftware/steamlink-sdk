/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
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
 * FILE DESCR: Definitions for NN dll exporting functions.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Vijayakumara M.
 *
 * DATE:       28-July-2005
 * CHANGE HISTORY:
 * Author		Date			Description
 ************************************************************************/
#ifndef __NN_H__
#define __NN_H__


// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NN_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NN_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef _WIN32
#ifdef NN_EXPORTS
#define NN_API __declspec(dllexport)
#else
#define NN_API __declspec(dllimport)
#endif //#ifdef NN_EXPORTS
#else
#define NN_API
#endif	//#ifdef _WIN32

class LTKTraceGroup;
class LTKShapeRecognizer;

#include "LTKInc.h"
#include "LTKTypes.h"

/** @defgroup NNShapeRecognizer NNShapeRecognizer
*@brief The NNShapeRecognizer
*/

/**
* @ingroup NNShapeRecognizer
* @file NN.cpp
*/

/**
 *  Crates instance of type NNShapeRecognizer and returns of type 
 *  LTKShpeRecognizer. (Acts as a Factory Method).
 *
 * @param  none
 *
 * @return  LTKShapeRecognizer - an instance of type LTKShapeRecognizer.
 */
extern "C" NN_API int createShapeRecognizer(const LTKControlInfo& controlInfo, 
												   LTKShapeRecognizer** pReco );

/**
 * Destroy the instance by taking the address as its argument.
 *
 * @param  obj - Address of LTKShapeRecognizer instance.
 *
 * @return  0 on Success
 */
extern "C" NN_API int deleteShapeRecognizer(LTKShapeRecognizer *obj);

extern "C" NN_API int getTraceGroups(LTKShapeRecognizer *obj, int shapeID, int numberOfTraceGroups,
									 vector<LTKTraceGroup> &outTraceGroups);

void unloadDLLs();

#endif //#ifndef __NN_H__
