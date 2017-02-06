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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
 
/************************************************************************
 * FILE DESCR: getError function declaration.
 *
 * CONTENTS:   
 *
 * AUTHOR: Vijayakumara M.
 *
 * DATE: Sept 01, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#ifndef _LTK_ERRORS_H__
#define _LTK_ERRORS_H__

#include "LTKInc.h"

//extern int errorCode; // Error code, which will set for the error occurence.


/**
 * This method returns error description corresponding to the error code.
 *
 * @param error code, the integer value for an error.
 *
 * @return the error description of the error code.
 */

string getErrorMessage(int errorCode);

#endif // #ifndef _LTKERRORS_H__


