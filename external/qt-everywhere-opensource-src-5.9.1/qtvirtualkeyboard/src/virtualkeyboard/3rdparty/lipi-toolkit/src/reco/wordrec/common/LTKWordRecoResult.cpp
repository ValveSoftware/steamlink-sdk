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
 * $LastChangedDate$
 * $Revision$
 * $Author$
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of the word recognition result 
 * CONTENTS: setWordRecoResult
 *           getResultWord
 *           getResultConfidence
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 11, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu V.     23-Aug-05       Added update word recognition result method
 ************************************************************************/

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#include "LTKException.h"

#include "LTKWordRecoResult.h"

#include "LTKLoggerUtil.h"



/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: LTKWordRecoResult
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKWordRecoResult::LTKWordRecoResult()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::LTKWordRecoResult()" << endl;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::LTKWordRecoResult()" << endl;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: LTKWordRecoResult
* DESCRIPTION	: Constructor that takes two arguements
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKWordRecoResult::LTKWordRecoResult(const vector< unsigned short >& word, float confidence = 0)
:m_word(word)
{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::LTKWordRecoResult(const vector< unsigned short >&, float)" << endl;
	
	if(confidence < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
            <<" LTKWordRecoResult::LTKWordRecoResult"
            <<"(vector< unsigned short >&, float)" <<endl;

		throw LTKException(ENEGATIVE_NUM);
	}
	
	m_confidence = confidence;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::LTKWordRecoResult(const vector< unsigned short >&, float)" << endl;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: setWordRecoResult
* DESCRIPTION	: assign values to the word recognition result 
* ARGUMENTS		: word       - unicode result string
*                 confidence - confidence of the current word (default 0)
* RETURNS		: SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKWordRecoResult::setWordRecoResult(const vector< unsigned short >& word, float confidence = 0)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::setWordRecoResult()" << endl;
	
	if(confidence < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
            <<" LTKWordRecoResult::setWordRecoResult()"<<endl;

		LTKReturnError(ENEGATIVE_NUM);

	}

	if(word.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< EEMPTY_VECTOR <<":"<< getErrorMessage(EEMPTY_VECTOR)
            <<" LTKWordRecoResult::setWordRecoResult()"<<endl;

		LTKReturnError(EEMPTY_VECTOR);
	}
	
	//assigning the values
	m_word = word;
	
	m_confidence = confidence;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::setWordRecoResult()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: getResultWord
* DESCRIPTION	: returns the result word
* ARGUMENTS		: 
* RETURNS		: Returns unicode result string
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
const vector<unsigned short>& LTKWordRecoResult::getResultWord() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::getResultWord()" << endl;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::getResultWord()" << endl;

	return m_word;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: getResultConfidence
* DESCRIPTION	: returns the confidence of result
* ARGUMENTS		: 
* RETURNS		: Returns float confidence value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
float LTKWordRecoResult::getResultConfidence() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::getResultConfidence()" << endl;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::getResultConfidence()" << endl;

	return m_confidence;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: setResultConfidence
* DESCRIPTION	: sets the confidence of result
* ARGUMENTS		: 
* RETURNS		: Returns SUCCESS if completed successfully
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKWordRecoResult::setResultConfidence(float confidence) 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::setResultConfidence()" << endl;
	
	if(confidence < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
            <<" LTKWordRecoResult::setResultConfidence()"<<endl;

		LTKReturnError(ENEGATIVE_NUM);

	}
	
	m_confidence = confidence;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::setResultConfidence()" << endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 19-MAR-2005
* NAME			: updateWordRecoResult
* DESCRIPTION	: This method adds to the existing word recognition result
*               : with a new symbol
* ARGUMENTS		: newSymbol  - This will be appended to the existing word
*               : confidence - confidence of the new symbol, will be added 
*               :              to existing confidence
* RETURNS		: Returns SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKWordRecoResult::updateWordRecoResult( unsigned short newSymbol, float confidence)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoResult::updateWordRecoResult()" << endl;
	
	if(confidence < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
        	<<"Error : "<< ENEGATIVE_NUM <<":"<< getErrorMessage(ENEGATIVE_NUM)
            <<" LTKWordRecoResult::updateWordRecoResult()"<<endl;

		LTKReturnError(ENEGATIVE_NUM);

	}

	m_word.push_back(newSymbol);
	m_confidence += confidence;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoResult::updateWordRecoResult()" << endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 11-MAR-2005
* NAME			: ~LTKWordRecoResult
* DESCRIPTION	: Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKWordRecoResult::~LTKWordRecoResult()
{
}

