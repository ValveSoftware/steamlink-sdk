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
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       May 29, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKWindowsUtil.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"

#include <windows.h>

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
LTKWindowsUtil::LTKWindowsUtil()
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
LTKWindowsUtil::~LTKWindowsUtil()
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
int LTKWindowsUtil::loadSharedLib(const string & lipiLibPath,
                                 const string & sharedLibName, 
                                 void * * libHandle)
{    
    string sharedLibraryPath = "";
    
	//	construct the path for the recogniser DLL
    sharedLibraryPath = lipiLibPath + "\\" + sharedLibName +
#ifndef NDEBUG
        "d"
#endif
        ".dll";
    // Load the DLL
    *libHandle = (void*)LoadLibrary(sharedLibraryPath.c_str());

	if(*libHandle == NULL)
	{ 
		cout << GetLastError() << endl;
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
int LTKWindowsUtil::unloadSharedLib(void * libHandle)
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
int LTKWindowsUtil::getFunctionAddress(void * libHandle,
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
    
    *functionHandle = GetProcAddress((HMODULE)libHandle, functionName.c_str());

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
int LTKWindowsUtil::getPlatformName(string& outStr)
{
    outStr = "Windows";
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
int LTKWindowsUtil::getProcessorArchitechure(string& outStr)
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
int LTKWindowsUtil::getOSInfo(string& outStr)
{
   outStr = "";

   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
   {
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (! GetVersionEx ((OSVERSIONINFO *) &osvi)) 
         return SUCCESS;
   }

   switch (osvi.dwPlatformId)
   {
      // Test for the Windows NT product family.
      case VER_PLATFORM_WIN32_NT:

         // Test for the specific product family.
         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
         {
            outStr = "Microsoft Windows Server&nbsp;2003 family, ";
         }

         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
         {
            outStr = "Microsoft Windows XP ";
         }

         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
         {
            outStr = "Microsoft Windows 2000 ";
         }

         if ( osvi.dwMajorVersion <= 4 )
         {
            outStr = "Microsoft Windows NT ";
         }

         // Test for specific product on Windows NT 4.0 SP6 and later.
         if( bOsVersionInfoEx )
         {
            // Test for the workstation type.
               if( osvi.dwMajorVersion == 4 )
               {
                  outStr += "Workstation 4.0 ";
                }
         }
         else  // Test for specific product on Windows NT 4.0 SP5 and earlier
         {
            HKEY hKey;
			
            char szProductType[BUFSIZE];
            DWORD dwBufLen=BUFSIZE;
            LONG lRet;

            lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
               "SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
               0, KEY_QUERY_VALUE, &hKey );
            if( lRet != ERROR_SUCCESS )
               return SUCCESS;

            lRet = RegQueryValueEx( hKey, "ProductType", NULL, NULL,
               (LPBYTE) szProductType, &dwBufLen);
            if( (lRet != ERROR_SUCCESS) )
               return SUCCESS;

            RegCloseKey( hKey );

            if ( lstrcmpi( "WINNT", szProductType) == 0 )
            {
                outStr += "Workstation ";
            }

            if ( lstrcmpi( "LANMANNT", szProductType) == 0 )
            {
                outStr += "Server ";
            }

            if ( lstrcmpi( "SERVERNT", szProductType) == 0 )
            {
                outStr += "Advanced Server ";
            }

            printf( "%d.%d ", osvi.dwMajorVersion, osvi.dwMinorVersion );
         }

      // Display service pack (if any) and build number.

         if( osvi.dwMajorVersion == 4 && 
             lstrcmpi( osvi.szCSDVersion, "Service Pack 6" ) == 0 )
         {
            HKEY hKey;
            LONG lRet;

            // Test for SP6 versus SP6a.
            lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
               "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009",
               0, KEY_QUERY_VALUE, &hKey );
            
            if( lRet == ERROR_SUCCESS )
            {
				char tempStr[50];
				sprintf(tempStr,"%d", osvi.dwBuildNumber & 0xFFFF);
				string str1 = string(tempStr);
				string temp = "Service Pack 6a (Build " + str1 + ")";

                outStr += temp;
            }
            else // Windows NT 4.0 prior to SP6a
            {
				char tempStr[100];

                sprintf (tempStr, "%s (Build %d)",
                                  osvi.szCSDVersion,
                                  osvi.dwBuildNumber & 0xFFFF);
				string str1 = string(tempStr);

                outStr +=  str1;
            }

            RegCloseKey( hKey );
         }
         else // Windows NT 3.51 and earlier or Windows 2000 and later
         {
			char tempStr[100];
			
            sprintf (tempStr, "%s (Build %d)",
                  osvi.szCSDVersion,
                  osvi.dwBuildNumber & 0xFFFF);
            
				string str1 = string(tempStr);

            outStr += str1;
            
         }


         break;

      // Test for the Windows 95 product family.
      case VER_PLATFORM_WIN32_WINDOWS:

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
         {
            outStr += "Microsoft Windows 95 ";
            
            if ( osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B' )
            {
                outStr +=  "OSR2 ";
            }
         } 

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
         {
            outStr += "Microsoft Windows 98 ";
             
            if ( osvi.szCSDVersion[1] == 'A' )
            {
                outStr +=  "SE " ;
            }
         } 

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
         {
            outStr += "Microsoft Windows Millennium Edition";
         } 
         break;

      case VER_PLATFORM_WIN32s:
            outStr += "Microsoft Win32s";

         break;
   }
   
   return SUCCESS;
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
int LTKWindowsUtil::recordStartTime()
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
int LTKWindowsUtil::recordEndTime()
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
int LTKWindowsUtil::diffTime(string& outStr)
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
int LTKWindowsUtil::getSystemTimeString(string& outStr)
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

void* LTKWindowsUtil::getLibraryHandle(const string& libName)
{
    string libNameWindows = libName +
#ifndef NDEBUG
        "d"
#endif
        ".dll";
    
    void* libHandle =  NULL;

    libHandle = GetModuleHandle(libNameWindows.c_str());

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

string LTKWindowsUtil::getEnvVariable(const string& envVariableName)
{
	return getenv(envVariableName.c_str());
}
