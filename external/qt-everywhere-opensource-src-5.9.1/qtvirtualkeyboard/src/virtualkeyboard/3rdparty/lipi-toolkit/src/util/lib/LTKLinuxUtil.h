/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or 
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
******************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-08-12 11:34:07 +0530 (Tue, 12 Aug 2008) $
 * $Revision: 604 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: 
 *
 * CONTENTS: 
 *
 * AUTHOR:     
 *
 * DATE:       May 29, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKLINUXUTIL_H
#define __LTKLINUXUTIL_H

#include "LTKOSUtil.h"

class LTKLinuxUtil: public LTKOSUtil
{
private:
    time_t m_startTime;

    time_t m_endTime;
    
public:

   /**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKLinuxUtil();

   /**
	* Destructor
	*/

	~LTKLinuxUtil();

	// @}
    int loadSharedLib(const string& lipiLibPath,
                         const string& sharedLibName,
                         void** libHandle);

    int unloadSharedLib(void* libHandle);

    int getFunctionAddress(void * libHandle,
                                     const string& functionName,
                                     void** functionHandle);

    int getPlatformName(string& outStr);

    int getProcessorArchitechure(string& outStr);

    int getOSInfo(string& outStr);

    int recordStartTime();

    int recordEndTime();

    int diffTime(string& outStr);

    int getSystemTimeString(string& outStr);

    void* getLibraryHandle(const string& libName);
    
    string getEnvVariable(const string& envVariableName);
	
};

#endif	



