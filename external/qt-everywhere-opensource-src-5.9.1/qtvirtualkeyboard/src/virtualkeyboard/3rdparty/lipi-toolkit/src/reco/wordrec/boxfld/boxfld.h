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
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for boxfld dll exporting functions.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       Aug 23, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of 
 ************************************************************************/
#ifndef __BOXFLD_H__
#define __BOXFLD_H__

#ifdef _WIN32
#include <windows.h>
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DTW_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DTW_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef BOXFLD_EXPORTS
#define BOXFLD_API __declspec(dllexport)
#else
#define BOXFLD_API __declspec(dllimport)
#endif
#else
#define BOXFLD_API
#endif	//#ifdef _WIN32


#include "LTKWordRecognizer.h"
#include "BoxFieldRecognizer.h"
#include "LTKLoggerUtil.h"
#include "LTKErrors.h"

void *m_hAlgoDLLHandle;

/**
 *  Crates instance of type PCAShaperecongnizer and retuns of type 
 *  LTKShpeRecongizer. (Acts as a Factory Method).
 *
 * @param  void - No argument
 *
 * @return  LTKShapeRecognizer - an instace of type LTKShapeRecoginzer.
 */

//Creates Instance of the Box Field recognizer and returns base class (LTKWordRecognizer) pointer
extern "C" BOXFLD_API int createWordRecognizer(const LTKControlInfo& controlInfo,LTKWordRecognizer** boxFldRecoPtr);

/**
 * Destry the instance by taking the address as its argument.
 *
 * @param  obj - Address of LTKShapeRecognizer instnace.
 *
 * @return  0 on Success
 */

//Destroys the instance by taking its addess as its argument.
extern "C" BOXFLD_API int deleteWordRecognizer(LTKWordRecognizer* obj);

#endif //#ifndef __BOXFLD_H__
