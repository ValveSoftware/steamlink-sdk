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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKShapeRecoResult which holds the recognition results of the 
 *			   shape recognition engine
 *
 * CONTENTS:   
 *	getShapeID
 *	getConfidence   
 *	setShapeID
 *	setConfidence
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKInc.h"

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#include "LTKShapeRecoResult.h"

#include "LTKLoggerUtil.h"


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecoResult
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecoResult::LTKShapeRecoResult()    
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::LTKShapeRecoResult()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::LTKShapeRecoResult()"  <<endl;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecoResult
* DESCRIPTION	: Constructor initializing the member variables
* ARGUMENTS		: shapeId - shape id of the result
*				  confidence - its corresponding confidence value
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecoResult::LTKShapeRecoResult(int shapeId, float confidence) : m_shapeId(shapeId), 
									   m_confidence(confidence)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::LTKShapeRecoResult(int, float)"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::LTKShapeRecoResult(int, float)"  <<endl;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getShapeId
* DESCRIPTION	: returns the shape id of the result
* ARGUMENTS		: 
* RETURNS		: shape id of the result
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKShapeRecoResult::getShapeId() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::getShapeId()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::getShapeId()"  <<endl;
    return m_shapeId;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getConfidence
* DESCRIPTION	: returns the confidence value of the result
* ARGUMENTS		: 
* RETURNS		: confidence value of the result
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

float LTKShapeRecoResult::getConfidence() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::getConfidence()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::getConfidence()"  <<endl;
    return m_confidence;
}

/**********************************************************************************
 * AUTHOR		: Balaji R.
 * DATE			: 23-DEC-2004
 * NAME			: setShapeId
 * DESCRIPTION	: sets the shape id of the result
 * ARGUMENTS		: shapeId - shape id of the result
 * RETURNS		: SUCCESS on successful set function
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 *************************************************************************************/

int LTKShapeRecoResult::setShapeId(int shapeId)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::setShapeId()"  <<endl;

    if(shapeId < 0)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
            getErrorMessage(EINVALID_CLASS_ID)<<
            " LTKShapeRecoResult::setShapeId()"  <<endl;

        LTKReturnError(EINVALID_CLASS_ID);
    }
    this->m_shapeId = shapeId;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::setShapeId()"  <<endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Balaji R.
 * DATE			: 23-DEC-2004
 * NAME			: setConfidence
 * DESCRIPTION	: sets the confidence of the result
 * ARGUMENTS		: confidenceVal - confidence of the result
 * RETURNS		: SUCCESS on successful set function
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 *************************************************************************************/

int LTKShapeRecoResult::setConfidence(float confidenceVal)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Entering LTKShapeRecoResult::setConfidence()"  <<endl;

    if( confidenceVal < 0 || confidenceVal > 1)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
            getErrorMessage(EINVALID_CONFIDENCE_VALUE)<<
            " LTKShapeRecoResult::setShapeId()"  <<endl;

        LTKReturnError(EINVALID_CONFIDENCE_VALUE);
    }

    this->m_confidence = confidenceVal;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<  
        "Exiting LTKShapeRecoResult::setConfidence()"  <<endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Balaji R.
 * DATE			: 23-DEC-2004
 * NAME			: ~LTKShapeRecoResult
 * DESCRIPTION	: destructor
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 *************************************************************************************/

LTKShapeRecoResult::~LTKShapeRecoResult()
{
}
