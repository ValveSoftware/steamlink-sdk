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
 * FILE DESCR: Function prototype for the Factory class LTKShapeFeatureExtractorFactory
 *
 * CONTENTS:   
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       07-FEB-2007
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKFEATUREEXTRACTORFACTORY_H
#define __LTKFEATUREEXTRACTORFACTORY_H
#include "LTKTypes.h"

#ifndef _WIN32
#include  <dlfcn.h>
#else
#include <windows.h>
#endif

class LTKShapeFeatureExtractor;

/**
* @ingroup feature_extractor
* @brief A factory class which return a pointer to the feature extractor.
* @class LTKShapeFeatureExtractorFactory
* 
*
* <p>
* A factory class which return a pointer to the feature extractor depending
* on the featureExtractorType passed to it
* </p>
*/
typedef int (*FN_PTR_CREATE_SHAPE_FEATURE_EXTRACTOR)
            (const LTKControlInfo& controlInfo,
            LTKShapeFeatureExtractor** outFeatureExtractor);

class LTKShapeFeatureExtractorFactory
{

public:

	LTKShapeFeatureExtractorFactory();										 
    
	int createFeatureExtractor(const string& featureExtractorType,
                              const string& lipiRootPath,
                              const string& lipiLibPath,
                              void** m_libHandlerFE,
                              const LTKControlInfo& controlInfo,
                              LTKShapeFeatureExtractor** outFeatureExtractor);

	int getFeatureExtractorInst(const string& lipiRootPath,
                                 const string& lipiLibPath,
                                 const string& feName,
                                 void** m_libHandlerFE,
                                 const LTKControlInfo& controlInfo,
                                 LTKShapeFeatureExtractor** outFeatureExtractor);

private:
	int mapFeatureExtractor(const string& featureExtractorName,
							    string& outFECode);
};


#endif
