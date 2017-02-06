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
 * $LastChangedDate: 2008-07-30 15:57:38 +0530 (Wed, 30 Jul 2008) $
 * $Revision: 576 $
 * $Author: kuanish $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKShapeRecognizer which would be used as a place holder in LTKShapeRecognizer 
 *			   for anyone of the implemented shape recognizers like PCAShapeRecognizer which is derived from this class
 *
 * CONTENTS: 
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKShapeRecognizer.h"
#include "LTKLoggerUtil.h"
#include "LTKException.h"
#include "LTKErrorsList.h"
#include "LTKErrors.h"
#include "LTKInc.h"

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecognizer
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecognizer::LTKShapeRecognizer() : m_cancelRecognition(false)
{

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Entering LTKShapeRecognizer::LTKShapeRecognizer()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Exiting LTKShapeRecognizer::LTKShapeRecognizer()"  <<endl;

}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecognizer
* DESCRIPTION	: Initializes the member(s) of the class
* ARGUMENTS		: shapeRecognzierName - name of the derived shape recognizer which is being held
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecognizer::LTKShapeRecognizer(const string& shapeRecognzierName) : m_shapeRecognizerName(shapeRecognzierName), m_cancelRecognition(false)
{

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Entering LTKShapeRecognizer::LTKShapeRecognizer(const string&)"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Exiting LTKShapeRecognizer::LTKShapeRecognizer(const string&)"  <<endl;
}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 07-JUN-2007
* NAME			: AddCLass
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKShapeRecognizer::addClass(const LTKTraceGroup& sampleTraceGroup,int& shapeID)
{

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Entering LTKShapeRecognizer::addClass()"  <<endl;
    
    LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
        getErrorMessage(ESHAPE_RECOCLASS_NOIMPLEMENTATION) <<
            " LTKShapeRecognizer::addClass()"  <<endl;

    LTKReturnError(ESHAPE_RECOCLASS_NOIMPLEMENTATION);
    
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Exiting LTKShapeRecognizer::addClass()"  <<endl;

    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: 
* DATE			: 
* NAME			: AddSample
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKShapeRecognizer::addSample(const LTKTraceGroup& sampleTraceGroup,int shapeID)
{

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering LTKShapeRecognizer::AddSample()"  <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
		getErrorMessage(ESHAPE_RECOCLASS_NOIMPLEMENTATION) <<
		" LTKShapeRecognizer::AddSample()"  <<endl;

	LTKReturnError(ESHAPE_RECOCLASS_NOIMPLEMENTATION);

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting LTKShapeRecognizer::AddSample()"  <<endl;

	return SUCCESS;
}
/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 07-JUN-2007
* NAME			: Delete Class
* DESCRIPTION	:  
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKShapeRecognizer::deleteClass(int shapeID)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeRecognizer::deleteClass()"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
        getErrorMessage(ESHAPE_RECOCLASS_NOIMPLEMENTATION) <<
        " LTKShapeRecognizer::deleteClass()"  <<endl;

    LTKReturnError(ESHAPE_RECOCLASS_NOIMPLEMENTATION);

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeRecognizer::deleteClass()"  <<endl;

    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-AUG-2007
* NAME			: Adapt
* DESCRIPTION	:  
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKShapeRecognizer::adapt(int shapeID )
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeRecognizer::adapt()"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
        getErrorMessage(ESHAPE_RECOCLASS_NOIMPLEMENTATION) <<
        " LTKShapeRecognizer::adapt()"  <<endl;

    LTKReturnError(ESHAPE_RECOCLASS_NOIMPLEMENTATION);

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeRecognizer::adapt()"  <<endl;

    return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-AUG-2007
* NAME			: Adapt
* DESCRIPTION	:  
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKShapeRecognizer::adapt(const LTKTraceGroup& sampleTraceGroup, int shapeID)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeRecognizer::adapt(const LTKTraceGroup&, int)"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " <<
        getErrorMessage(ESHAPE_RECOCLASS_NOIMPLEMENTATION) <<
        " LTKShapeRecognizer::adapt()"  <<endl;

    LTKReturnError(ESHAPE_RECOCLASS_NOIMPLEMENTATION);

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeRecognizer::adapt(const LTKTraceGroup&, int)"  <<endl;

    return SUCCESS;

}


/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKShapeRecognizer
* DESCRIPTION	: destructor 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecognizer::~LTKShapeRecognizer()
{
}

