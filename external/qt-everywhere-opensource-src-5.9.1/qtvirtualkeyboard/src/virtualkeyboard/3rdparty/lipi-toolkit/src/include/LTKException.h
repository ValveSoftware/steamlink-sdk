/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of LTKException which holds error message of thrown error
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKEXCEPTION_H
#define __LTKEXCEPTION_H

#include "LTKInc.h"

/** @defgroup Exceptions
*/

/** @ingroup Exceptions
*/

/**
 * @class LTKException
 * <p>This class is used to contain error message thrown during an exception. </p>
 */

class LTKException
{

protected:

	int m_errorCode;		//	Error Code of the exception defined in LTKMacros.h

public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

    LTKException ();

	/**
	 * Initialization Constructor
	 * 
	 * @param exceptionMsg - error message
	 *
	 */

	LTKException (int errorCode);

	/** 
	 * Destructor
	 */
	
	~LTKException ();

	//@}

	/**
	 * @name Getter Functions
	 */

	// @{

	/**
	 * This method returns the error message embedded in the class object
	 *
	 * @return error message
	 *
	 */

    string getExceptionMessage() const;

	/**
	 * This method returns the error code embedded in the class object
	 *
	 * @return error code
	 *
	 */

    int getErrorCode() const;


	// @}

};

#endif

//#ifndef __LTKEXCEPTION_H
//#define __LTKEXCEPTION_H




