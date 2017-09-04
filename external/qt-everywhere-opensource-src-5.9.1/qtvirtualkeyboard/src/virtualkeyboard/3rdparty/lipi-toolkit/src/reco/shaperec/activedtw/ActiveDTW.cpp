/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies 
* or substantial portions of the Software.
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
* $LastChangedDate: 2011-01-18 15:41:43 +0530 (Tue, 18 Jan 2011) $
* $Revision: 829 $
* $Author: mnab $
*
************************************************************************/
/************************************************************************
* FILE DESCR: Defines the entry for ActiveDTW dll application
*
* CONTENTS:
*
* AUTHOR: S Anand 
*
w
* DATE:  3-MAR-2009  
* CHANGE HISTORY:
* Author       Date            Description of change
* Balaji MNA   18th Jan 2010   Receiving LTKShapeRecognizer as single pointer 
*                              instead of double pointer in deleteShapeRecognizer
************************************************************************/


#include "ActiveDTW.h"
#include "LTKShapeRecognizer.h"
#include "ActiveDTWShapeRecognizer.h"
#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKOSUtilFactory.h"
#include "LTKOSUtil.h"

#ifdef _WIN32
#include <windows.h>

BOOL APIENTRY DllMain( HANDLE hModule, 
					  DWORD  ul_reason_for_call, 
					  LPVOID lpReserved
					  )
{
    switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
    }
    return TRUE;
}
#endif

/** createShapeRecognizer **/

int createShapeRecognizer(const LTKControlInfo& controlInfo,
						  LTKShapeRecognizer** ptrObj )
{
	try
	{
		*ptrObj = new ActiveDTWShapeRecognizer(controlInfo);
		return SUCCESS;
	}
	catch(LTKException e)
	{
		LTKReturnError(e.getErrorCode());
	}
}


/**deleteShapeRecognizer **/
int deleteShapeRecognizer(LTKShapeRecognizer *obj)
{
	try
	{
		if (obj != NULL )
		{
			delete obj;
			obj = NULL;
			
			//unloadDLLs();
		}
	}
	catch(LTKException e)
	{
		LTKReturnError(e.getErrorCode());
	}
	
	
    return SUCCESS;
}

/** unloadDLLs **/
/*
void unloadDLLs()
{
// Unload feature extractor DLL
if(m_libHandlerFE != NULL)
{
//Unload the DLL
LTKUnloadDLL(m_libHandlerFE);
m_libHandlerFE = NULL;
}
}*/

/** getTraceGroups **/
int getTraceGroups(LTKShapeRecognizer *obj, int shapeId, 
				   int numberOfTraceGroups, 
				   vector<LTKTraceGroup> &outTraceGroups)
{
	int errorCode = ((ActiveDTWShapeRecognizer*)obj)->getTraceGroups(shapeId,		
		numberOfTraceGroups,  outTraceGroups);
	
	if ( errorCode != SUCCESS )
	{
		LTKReturnError(errorCode);
	}
	
	return SUCCESS;
}
