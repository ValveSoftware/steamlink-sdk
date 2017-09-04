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
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
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

#include "LTKWinCEUtil.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"
#include "LTKStringUtil.h"

/************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: LTKWinCEUtil
 * DESCRIPTION	: Default constructor		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 *****************************************************************************/
LTKWinCEUtil::LTKWinCEUtil()
{
    
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: ~LTKWinCEUtil
 * DESCRIPTION	: Desstructor		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
LTKWinCEUtil::~LTKWinCEUtil()
{
    
}

/***************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
* NAME			: LoadLibraryUnicode
* DESCRIPTION	: Loads the library which takes as character string input
* ARGUMENTS		: 
* RETURNS		: Handle to the library loaded.
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
HANDLE LTKWinCEUtil::LoadLibraryUnicode(const char* pszDllName) 
{
	if (pszDllName == NULL)
		return NULL ;
	
	HANDLE hDll = NULL ;
	TCHAR *pszwDll = convertCharToTChar(pszDllName);
	if (pszwDll)
	{
		hDll = ::LoadLibrary(pszwDll) ;
		delete(pszwDll) ;
	}
	return hDll ;
}
/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: loadSharedLib
 * DESCRIPTION	: Loads dynamic library		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::loadSharedLib(const string & lipiRoot, 
                                 const string & sharedLibName, 
                                 void * * libHandle)
{    
    string sharedLibraryPath = "";
    
	//	construct the path for the recogniser DLL
    sharedLibraryPath = lipiRoot + "\\" + "lib" + "\\" + sharedLibName + ".dll";

    // Load the DLL
	*libHandle = (void*)LoadLibraryUnicode(sharedLibraryPath.c_str());

	if(*libHandle == NULL)
	{ 
		cout << GetLastError() << endl;
		return FAILURE; 
	}
    
    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: loadSharedLib
 * DESCRIPTION	: Loads dynamic library		
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::unloadSharedLib(void * libHandle)
{
    
    if (libHandle != NULL)
    {
        int returnVal = FreeLibrary((HINSTANCE)(libHandle));

        // For FreeLibrary, If the function fails, the return value is zero
        if (returnVal == 0 )
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
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getFunctionAddress
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::getFunctionAddress(void * libHandle,
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
	TCHAR *pszwName = convertCharToTChar(functionName.c_str());
    *functionHandle = GetProcAddress((HMODULE)libHandle, pszwName);
	delete pszwName;

	if ( *functionHandle == NULL )
    {
        return FAILURE;
    }

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getPlatformName
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::getPlatformName(string& outStr)
{
    outStr = "Windows";
    return SUCCESS;

}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getProcessorArchitechure
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::getProcessorArchitechure(string& outStr)
{
    SYSTEM_INFO architechure;

    GetSystemInfo(&architechure);

    int arc = architechure.wProcessorArchitecture;

    switch (arc)
    {
        case 0 : outStr = "INTEL";
                 break;

        case 1 : outStr = "MIPS";
                 break;

        case 2 : outStr = "ALPHA";
                 break;

        case 3 : outStr = "PPC";
                 break;

        case 4 : outStr = "SHX";
                 break;

        case 5 : outStr = "ARM";
                 break;

        case 6 : outStr = "IA64";
                 break;

        case 7 : outStr = "ALPHA64";
                 break;

        default : outStr = "UNKNOWN";
                 break;
    }
        
    return SUCCESS;

}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getOSInfo
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::getOSInfo(string& outStr)
{
   outStr = "";
   outStr += "Microsoft WinCE" ;
   
   return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: recordStartTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::recordStartTime()
{
	GetLocalTime(&m_startTime);
    
	return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: recordEndTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::recordEndTime()
{
 	GetLocalTime(&m_endTime);

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: diffTime
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::diffTime(string& outStr)
{
	//Convert the SYSTEMTIME to FILETIME
	FILETIME ftimeStart, ftimeEnd ;
	SystemTimeToFileTime(&m_startTime, &ftimeStart) ;
	SystemTimeToFileTime(&m_endTime, &ftimeEnd) ;
	
	//Convert the FILETIME to ULARGE_INTEGER 
	ULARGE_INTEGER uIntStart, uIntEnd ;
	uIntStart.HighPart = ftimeStart.dwHighDateTime ;
	uIntStart.LowPart = ftimeStart.dwLowDateTime;

	uIntEnd.HighPart = ftimeEnd.dwHighDateTime;
	uIntEnd.LowPart = ftimeEnd.dwLowDateTime; 

	ULONGLONG ulDiff = uIntEnd.QuadPart - uIntStart.QuadPart ;

	char temp[10];
	sprintf(temp, "%.1f", ulDiff/10000000);

	string tempStr(temp) ;
    outStr = tempStr;

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getSystemTimeString
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/
int LTKWinCEUtil::getSystemTimeString(string& outStr)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char szTime[100] ;
	sprintf(szTime, "%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
	outStr = szTime ;

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Sarbeswar Meher
 * DATE			: 11-08-2008
 * NAME			: getLibraryHandle
 * DESCRIPTION	: 
 * ARGUMENTS      : 
 *
 * RETURNS		: 
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ***************************************************************************/

void* LTKWinCEUtil::getLibraryHandle(const string& libName)
{
    string libNameWindows = libName + ".dll";
    
    void* libHandle =  NULL;

	TCHAR *pszwLib = convertCharToTChar(libNameWindows.c_str()) ;
    libHandle = GetModuleHandle(pszwLib);
	delete pszwLib ;

    return libHandle;
}
/***************************************************************************
* AUTHOR		: Sarbeswar Meher
* DATE			: 14-Apr-2008
* NAME			: ConvertCharToTChar
* DESCRIPTION	: Converts char to TCHAR
* ARGUMENTS		: 
* RETURNS		: TCHAR*
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
TCHAR* LTKWinCEUtil::convertCharToTChar(const char* pszData) 
{
	if (pszData == NULL)
		return NULL ;
	
	int nLen = strlen(pszData)+1 ;
	TCHAR *pszwData = new TCHAR[nLen] ;
	if (pszwData)
	{
		//Converts the char to TCHAR value
		MultiByteToWideChar(CP_ACP, 0, pszData, nLen, pszwData, nLen) ;
		return pszwData ;
	}
	return NULL ;
}

/***************************************************************************
* AUTHOR		: Sarbeswar Meher
* DATE			: 14-Apr-2008
* NAME			: getEnvVariable
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
string LTKWinCEUtil::getEnvVariable(const string& envVariableName)
{
	return NULL;
}