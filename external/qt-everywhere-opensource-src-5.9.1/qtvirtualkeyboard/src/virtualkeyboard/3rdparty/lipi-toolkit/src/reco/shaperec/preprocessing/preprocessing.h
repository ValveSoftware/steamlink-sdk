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
 * FILE DESCR: This file includes exporting function definitons.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Vijayakumara M.
 *
 * DATE:       28-July-2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#ifndef __PREPROCESSING_H_
#define __PREPROCESSING_H_

#ifdef _WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the PREPROCESSING_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// PREPROCESSING_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef PREPROCESSING_EXPORTS
#define PREPROCESSING_API __declspec(dllexport)
#else
#define PREPROCESSING_API __declspec(dllimport)
#endif
#else
#define PREPROCESSING_API
#endif	//	#ifdef _WIN32


#include "LTKPreprocessor.h"
#include "LTKLoggerUtil.h"

/**
 * This function creates the object of type LTKPreprocessor and returns the adderss
 * of type LTKPreprocessorInterface ( base class of LTKPreprocessor).
 *
 * @return the address of the new object create of type LTKPreprocessorInterface.
 */

extern "C" PREPROCESSING_API int createPreprocInst(const LTKControlInfo& controlInfo,
                        LTKPreprocessorInterface** preprocPtr);


/**
 * This function destroys the object of type LTKPreprocessorInterface by taking 
 * address of the instance.
 *
 * @param Address of the destroying object of type LTKPreprocessorInterface.
 */

extern "C" PREPROCESSING_API void destroyPreprocInst(LTKPreprocessorInterface* p);

#endif //#ifndef __PREPROCESSING_H_
