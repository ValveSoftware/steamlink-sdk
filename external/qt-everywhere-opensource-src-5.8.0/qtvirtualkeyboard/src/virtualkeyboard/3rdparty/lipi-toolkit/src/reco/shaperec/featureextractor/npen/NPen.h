/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
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
 * $LastChangedDate: 2008-02-20 10:03:51 +0530 (Wed, 20 Feb 2008) $
 * $Revision: 423 $
 * $Author: sharmnid $
 *
 ************************************************************************/
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NPen_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NPen_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef _WIN32
#ifdef NPen_EXPORTS
#define NPen_API __declspec(dllexport)
#else
#define NPen_API __declspec(dllimport)
#endif  //ifdef NPen_EXPORTS
#else
#define NPen_API
#endif	//#ifdef _WIN32

#include "LTKTypes.h"

class LTKShapeFeatureExtractor;
class LTKShapeFeature;

/** @defgroup NPenShapeFeatureExtractor
*@brief The NPenShapeFeatureExtractor
*/

/**
* @ingroup NPenShapeFeatureExtractor
* @file NPen.cpp
* <p>
* The functions exported by the DLL
*   - ::createShapeFeatureExtractor
*   - ::deleteShapeFeatureExtractor
*   - ::getCurrentVersion
*   - ::deleteShapeFeaturePtr
*/

/**
*  @brief Returns a pointer to the instance of NPenShapeFeatureExtractor
*
* <p>
* This function is based on run-time polymorphism. It creates an instance of 
* NPenShapeFeatureExtractor at run-time and returns it as a pointer to the base class <i>LTKShapeFeatureExtractor</i>
* </p>
*
* @param  none
*
* @return  Pointer to LTKShapeFeatureExtractor
*/
//extern "C" NPen_API int createShapeFeatureExtractor(const LTKControlInfo& controlInfo, LTKShapeFeatureExtractor**);
extern "C" NPen_API int createShapeFeatureExtractor(
                                            const LTKControlInfo& controlInfo,
                                LTKShapeFeatureExtractor** outFeatureExtractor);

/**
* @brief Deletes the feature extractor instance.
*
* <p> 
* This function cleans up all the memory allocations done by the feature extractor by calling it's destructor.
* </p>
*
* @param  featureExtractorPtr - Pointer to LTKShapeFeatureExtractor.
*
* @return  0 on Success
*/

//smart pointer for LTKShapeFeatureExtractor??
extern "C" NPen_API int deleteShapeFeatureExtractor(LTKShapeFeatureExtractor *featureExtractorPtr);
