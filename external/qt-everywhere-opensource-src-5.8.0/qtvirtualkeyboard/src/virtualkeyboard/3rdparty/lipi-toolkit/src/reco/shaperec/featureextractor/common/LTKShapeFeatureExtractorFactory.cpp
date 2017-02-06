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
 * $LastChangedDate: 2009-06-25 16:40:26 +0530 (Thu, 25 Jun 2009) $
 * $Revision: 778 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
* FILE DESCR: Implementation for LTKFeatureExtractor factory class
*
* CONTENTS:
*	createFeatureExtractor
*
* AUTHOR:     Nidhi Sharma
*
* DATE:       December 23, 2004
* CHANGE HISTORY:
* Author		Date			Description of LTKShapeFeatureExtractorFactory
************************************************************************/
#include "LTKShapeFeatureExtractorFactory.h"
#include "LTKMacros.h"
#include "LTKErrorsList.h"
#include "LTKException.h"
#include "LTKLoggerUtil.h"
#include "LTKErrors.h"
#include "LTKOSUtil.h"
#include "LTKOSUtilFactory.h"

/*************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 07-FEB-2007
* NAME			: createFeatureExtractor
* DESCRIPTION	: create method of a factory class
* ARGUMENTS		:
* RETURNS		: Pointer to the instance of LTKShapeFeatureExtractor
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of LTKFeatureExtractor
*****************************************************************************/

LTKShapeFeatureExtractorFactory::LTKShapeFeatureExtractorFactory()
{
}

/***********************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 07-FEB-2007
* NAME			: createFeatureExtractor
* DESCRIPTION	: create method of a factory class
* ARGUMENTS		:
* RETURNS		: Pointer to the instance of LTKShapeFeatureExtractor
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of LTKFeatureExtractor
******************************************************************************/
int LTKShapeFeatureExtractorFactory::createFeatureExtractor(
                                const string& featureExtractorName,
                                const string& lipiRootPath,
                                const string& lipiLibPath,
                                void** m_libHandlerFE,
                                const LTKControlInfo& controlInfo,
                                LTKShapeFeatureExtractor** outFeatureExtractor)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " << 
        "LTKShapeFeatureExtractorFactory::createFeatureExtractor()" << endl;
    
    string feName = "";
    
	int errorCode = mapFeatureExtractor(featureExtractorName, feName);

    if (errorCode != SUCCESS)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)
           << getErrorMessage(errorCode)  
           << "LTKShapeFeatureExtractorFactory::createFeatureExtractor:"
           << endl;
        
        LTKReturnError(errorCode);
    }
        
    errorCode = getFeatureExtractorInst(lipiRootPath, lipiLibPath, feName, m_libHandlerFE,
			                            controlInfo, outFeatureExtractor);

    if ( errorCode != SUCCESS)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)
           << getErrorMessage(errorCode) << ":" << feName 
           << "LTKShapeFeatureExtractorFactory::createFeatureExtractor:"
           << endl;
        
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " << 
        "LTKShapeFeatureExtractorFactory::createFeatureExtractor()" << endl;
    
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 11-Dec-2007
* NAME			: getFeatureExtractorInst
* DESCRIPTION	: 
* ARGUMENTS		:
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of LTKFeatureExtractor
*************************************************************************************/
int LTKShapeFeatureExtractorFactory::getFeatureExtractorInst(
                                 const string& lipiRootPath,
                                 const string& lipiLibPath,
                                 const string& feName,
                                 void** m_libHandlerFE,
                                 const LTKControlInfo& controlInfo,
                                 LTKShapeFeatureExtractor** outFeatureExtractor)
{
	FN_PTR_CREATE_SHAPE_FEATURE_EXTRACTOR createFeatureExtractorPtr;
    void *functionHandle = NULL;

    LTKOSUtil* utilPtr = LTKOSUtilFactory::getInstance();

    int returnVal = utilPtr->loadSharedLib(lipiLibPath, feName, m_libHandlerFE);

    
	if(returnVal != SUCCESS)
	{
	    LTKReturnError(ELOAD_FEATEXT_DLL);
	}

    returnVal = utilPtr->getFunctionAddress(*m_libHandlerFE, 
                                            CREATE_SHAPE_FEATURE_EXTRACTOR, 
                                            &functionHandle);

	if(returnVal != SUCCESS)
	{
	    utilPtr->unloadSharedLib(m_libHandlerFE);
        *m_libHandlerFE = NULL;

		LTKReturnError(EDLL_FUNC_ADDRESS_CREATE_FEATEXT);
	}

    createFeatureExtractorPtr = (FN_PTR_CREATE_SHAPE_FEATURE_EXTRACTOR)functionHandle;

    int errorCode = createFeatureExtractorPtr(controlInfo, outFeatureExtractor);

    if (errorCode != SUCCESS)
    {
		LTKReturnError(errorCode);
    }

    delete utilPtr;
	return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Saravanan
 * DATE			: 24-03-2007
 * NAME			: mapFeatureExtractor
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int LTKShapeFeatureExtractorFactory::mapFeatureExtractor(const string& featureExtractorName,
														 string& outFEName)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " << 
        "LTKShapeFeatureExtractorFactory::mapFeatureExtractor()" << endl;

    int returnCode = SUCCESS;

    if(LTKSTRCMP(featureExtractorName.c_str(), NAME_POINT_FLOAT_SHAPE_FEATURE_EXTRACTOR) == 0)
    {
        outFEName = POINT_FLOAT;
    }
    else if(LTKSTRCMP(featureExtractorName.c_str(), NAME_L7_SHAPE_FEATURE_EXTRACTOR) == 0)
    {
        outFEName = L7;
    }
	else if(LTKSTRCMP(featureExtractorName.c_str(),NAME_NPEN_SHAPE_FEATURE_EXTRACTOR) == 0)
	{
		outFEName = NPEN;
	}
	else if(LTKSTRCMP(featureExtractorName.c_str(),NAME_SUBSTROKE_SHAPE_FEATURE_EXTRACTOR) == 0)
	{
		outFEName = SUBSTROKE;
	}
	else
	{
		returnCode = EFTR_EXTR_NOT_EXIST;
	}

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " << 
        "LTKShapeFeatureExtractorFactory::mapFeatureExtractor()" << endl;

    return returnCode;
}
