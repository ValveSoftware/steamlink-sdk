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
 * $LastChangedDate: 2009-02-25 13:55:23 +0530 (Wed, 25 Feb 2009) $
 * $Revision: 741 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: 
 *
 * CONTENTS: 
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       May 29, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#include "LTKLinuxUtil.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"


#include <dlfcn.h>
#include <stdio.h>
#include <sys/utsname.h>


/************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-05-2008
 * NAME			: LTKWindowsUtil
 * DESCRIPTION	: Default constructor		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 *****************************************************************************/
LTKLinuxUtil::LTKLinuxUtil()
{
    
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-05-2008
 * NAME			: ~LTKWindowsUtil
 * DESCRIPTION	: Desstructor		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
LTKLinuxUtil::~LTKLinuxUtil()
{
    
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-05-2008
 * NAME			: loadSharedLib
 * DESCRIPTION	: Loads dynamic library		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::loadSharedLib(const string & lipiLibPath,
                                 const string & sharedLibName, 
                                 void * * libHandle)
{
   
    
    string sharedLibraryPath = "";
    
	//	construct the path for the recogniser DLL
    sharedLibraryPath = lipiLibPath + "/" + "lib" + sharedLibName + ".so";
    
    // Load the DLL
	*libHandle = dlopen(sharedLibraryPath.c_str(), RTLD_LAZY);

	if(*libHandle == NULL)
	{
		return FAILURE; 
	}

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-05-2008
 * NAME			: loadSharedLib
 * DESCRIPTION	: Loads dynamic library		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::unloadSharedLib(void * libHandle)
{
    if (libHandle != NULL)
    {
        int returnVal = dlclose(libHandle);

        if (returnVal != 0 )
        {
            return FAILURE;
        }
    }
    else
    {
        return FAILURE;
    }
    
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-05-2008
 * NAME			: getFunctionAddress
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::getFunctionAddress(void * libHandle,
                                             const string& functionName,
                                             void** functionHandle)
{
    // validate parameters
    if (libHandle == NULL )
    {
        return FAILURE;
    }

    if (functionName.empty())
    {
        return FAILURE;
    }
    
    *functionHandle = dlsym(libHandle, functionName.c_str());

    if ( *functionHandle == NULL )
    {
        return FAILURE;
    }

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: getPlatformName
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::getPlatformName(string& outStr)
{
    outStr = "Linux";
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: getProcessorArchitechure
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::getProcessorArchitechure(string& outStr)
{
    
    struct utsname name;

    uname(&name);

    outStr = name.machine;
        
    return SUCCESS;

}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: getOSInfo
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::getOSInfo(string& outStr)
{
   
    struct utsname name;
    uname(&name);
	string sysName(name.sysname);
	string sysRelease(name.release);

    outStr = sysName + " " + sysRelease;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: recordStartTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::recordStartTime()
{
    time(&m_startTime);
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: recordEndTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::recordEndTime()
{
    time(&m_endTime);
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: diffTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::diffTime(string& outStr)
{
    char temp[10];
	sprintf(temp, "%.1f", difftime(m_endTime, m_startTime));
	string tempStr(temp) ;
    outStr = tempStr;
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 26-06-2008
 * NAME			: getSystemTimeString
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKLinuxUtil::getSystemTimeString(string& outStr)
{
    time_t rawtime;

	time(&rawtime);

	string timeStr = ctime(&rawtime);

	outStr = timeStr.substr(0, 24);

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 17-07-2008
 * NAME			: getLibraryHandle
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/

void* LTKLinuxUtil::getLibraryHandle(const string& libName)
{
    string lipiRoot= getEnvVariable(LIPIROOT_ENV_STRING) ;
    string libNameLinux = lipiRoot + "/" + "lib" + "/" + "lib" + libName + ".so";

    void* libHandle = NULL;
    libHandle = dlopen(libNameLinux.c_str(), RTLD_LAZY);
    
	if(libHandle == NULL)
        cout << "Error opening " << libNameLinux.c_str() << " : " << dlerror() << endl;

    return libHandle;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 17-07-2008
 * NAME			: getEnvVariable
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/

string LTKLinuxUtil::getEnvVariable(const string& envVariableName)
{
	return getenv(envVariableName.c_str());
}
