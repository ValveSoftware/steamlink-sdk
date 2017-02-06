/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for the Ink File Writer Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Bharath A.
 *
 * DATE:       March 22, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#ifndef __LTKINKFILEWRITER_H
#define __LTKINKFILEWRITER_H

#include "LTKInc.h"

class LTKTraceGroup;

/**
* @ingroup util
*/

/** @brief Utility class for writing ink files
* @class LTKInkFileWriter
*/
class LTKInkFileWriter
{

public:
	
	/**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKInkFileWriter();

	/**
	* Destructor
	*/

	~LTKInkFileWriter();

	// @}

	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This is a static method which writes the trace group supplied to the file name specified with X and Y DPI info
	 * @param traceGroup trace group to be written onto the file
	 * @param fileName name of the file
	 * @param xDPI x-coordinate dots per inch
	 * @param yDPI y-coordinate dots per inch
	 * @return SUCCESS on successful write operation
	 */
	
	static int writeRawInkFile(const LTKTraceGroup& traceGroup,const string& fileName,int xDPI,int yDPI);

	/**
	 * This is a static method which writes the trace group supplied to the file name specified with X and Y DPI info, in unipen format
	 * @param traceGroup trace group to be written onto the file
	 * @param fileName name of the file
	 * @param xDPI x-coordinate dots per inch
	 * @param yDPI y-coordinate dots per inch
	 * @return SUCCESS on successful write operation
	 */
	
	static int writeUnipenInkFile(const LTKTraceGroup& traceGroup,const string& fileName,int xDPI,int yDPI);

	// @}
};


#endif

