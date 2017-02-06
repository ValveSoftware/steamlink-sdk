/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Implementation of LTKException class. The error messages are 
 *			   thrown embedded  in objects of this class
 *
 * CONTENTS:
 *	getExceptionMessage
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKMacros.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKException
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKException::LTKException():
	m_errorCode(DEFAULT_ERROR_CODE)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKException::LTKException()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKException::LTKException()" << endl;*/
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKException
* DESCRIPTION	: Initialization Constructor
* ARGUMENTS		: exceptionMsg - error message
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* *****************************************************************************/

LTKException::LTKException(int errorCode) : 
	m_errorCode(errorCode) 
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKException::LTKException(int)" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKException::LTKException(int)" << endl;*/
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getExceptionMessage
* DESCRIPTION	: returns the error message
* ARGUMENTS		: 
* RETURNS		: error message string
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

string LTKException::getExceptionMessage() const
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKException::getExceptionMessage()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKException::getExceptionMessage()" << endl;*/

	return string(getErrorMessage(m_errorCode));
}

/*****************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 22-Feb-2007
* NAME			: getErrorCode  
* DESCRIPTION	: returns the error code
* ARGUMENTS		: 
* RETURNS		: error code
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKException::getErrorCode() const
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKException::getErrorCode()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKException::getErrorCode()" << endl;*/

	return m_errorCode;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKException
* DESCRIPTION	: Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKException::~LTKException()
{
}


