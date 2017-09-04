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
 * FILE DESCR: Definitions for the Ink File Reader Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu V.     14 Sept 2005    Added unipen file reading function
 *                              that reads annotation.
 ************************************************************************/

#ifndef __LTKINKFILEREADER_H
#define __LTKINKFILEREADER_H

#include "LTKTypes.h"


class LTKTraceGroup;
class LTKCaptureDevice;
class LTKScreenContext;

/**
* @ingroup util
*/

/** @brief Exposes the APIs for reading ink files
@class LTKInkFileReader
*/
class LTKInkFileReader
{

public:

	/**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKInkFileReader();

	/**
	* Destructor
	*/

	~LTKInkFileReader();

	// @}

	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This is a static method which reads a raw ink file and loads it into a trace group object
	 * Also reads the device information from the unipen ink file into a capture device object
	 * @param inkFile Name of the ink file to be read
	 * @param traceGroup trace group into which the ink file has to be read into
	 * @param captureDevice object into which the device specific information is to be read into
	 * @return SUCCESS on successful read operation
	 */
	
	static int readRawInkFile(const string& inkFile, LTKTraceGroup& traceGroup, LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext);

	/**
	 * This is a static method which reads a unipen ink file and loads it into a trace group object
	 * Also reads the device information from the unipen ink file into a capture device object
	 * @param inkFile Name of the ink file to be read
	 * @param traceGroup trace group into which the ink file has to be read into
	 * @param captureDevice object into which the device specific information is to be read into
	 * @return SUCCESS on successful read operation
	 */
	
	static int readUnipenInkFile(const string& inkFile, LTKTraceGroup& traceGroup, LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext);

	/**
	 * This is a static method which reads the contents of a unipen file containing ink stored in a specified format into
	 * a trace group object. Also the device information stored in the ink file is read
	 * into a capture device object.The writing area information is captured in screen context object.
	 * @param inkFile name of the file containing the ink
	 * @param hierarchyLevel level at which the ink is required, ex. WORD or CHARACTER that follows .SEGMENT
	 * @param quality quality of the ink that is required. Can be GOOD,BAD,OK or ALL. Example, if ink of quality
	 *		  GOOD and BAD are required, then quality="GOOD,BAD" (NOTE:comma(,) is the delimiter) else if all ink 
	 *		  are required, quality="ALL"
	 * @param traceGroup trace group into which the ink has to be read into
	 * @param traceIndicesCommentsMap Map containing list of strokes separated by commas as key and the comments
	 *        to that trace group unit as value (ex. key-"2,4,5" value-"delayed stroke"
     * @param captureDevice capture device object into which device info is to be read into
     * @param screenContext writing area information
	 * @return SUCCESS on successful read operation
	 */

	static int readUnipenInkFileWithAnnotation(const string& inkFile,const string& hierarchyLevel,const string& quality, LTKTraceGroup& traceGroup,map<string,string>& traceIndicesCommentsMap,LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext);


	// @}
};

#endif	//#ifndef __LTKINKFILEREADER_H
