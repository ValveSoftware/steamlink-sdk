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
 * $LastChangedDate: 2011-01-18 15:41:43 +0530 (Tue, 18 Jan 2011) $
 * $Revision: 829 $
 * $Author: mnab $
 *
 ************************************************************************/
// NN.cpp : Defines the entry point for the DLL application.
//

#include "NN.h"
#include "LTKShapeRecognizer.h"
#include "NNShapeRecognizer.h"
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


/**********************************************************************************
* AUTHOR		: Saravanan R
* DATE			: 23-Jan-2007
* NAME			: createShapeRecoginizer
* DESCRIPTION	: Creates instance of type NNShaperecongnizer and retuns of type 
				  LTKShapeRecognizer. (Acts as a Factory Method).
* ARGUMENTS		: 
* RETURNS		: returns an instace of type LTKShapeRecoginzer.
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description
*************************************************************************************/
int createShapeRecognizer(const LTKControlInfo& controlInfo,
								 LTKShapeRecognizer** ptrObj )
{
	try
	{
		*ptrObj = new NNShapeRecognizer(controlInfo);
		return SUCCESS;
	}
	catch(LTKException e)
	{
		LTKReturnError(e.getErrorCode());
	}
}

/**********************************************************************************
* AUTHOR		: Saravanan R
* DATE			: 23-Jan-2007
* NAME			: deleteShapeRecoginzer
* DESCRIPTION	: Destroy the instance by taking the address as its argument.
* ARGUMENTS		: Address of LTKShapeRecognizer instance.
* RETURNS		: Returns 0 on Success
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description
* Balaji MNA        18th Jan 2010       Receiving LTKShapeRecognizer as single pointer 
*                                       instead of double pointer in deleteShapeRecognizer
*************************************************************************************/
int deleteShapeRecognizer(LTKShapeRecognizer *obj)
{
	try
	{
		if (obj != NULL )
		{
			delete obj;
			obj = NULL;
		}
	}
	catch(LTKException e)
	{
		LTKReturnError(e.getErrorCode());
	}
	
	
    return SUCCESS;
}


/**********************************************************************************
* AUTHOR        :Tarun Madan
* DATE          : 
* NAME          : 
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int getTraceGroups(LTKShapeRecognizer *obj, int shapeId, 
		int numberOfTraceGroups, 
		vector<LTKTraceGroup> &outTraceGroups)
{
	int errorCode = ((NNShapeRecognizer*)obj)->getTraceGroups(shapeId,		
			numberOfTraceGroups,  outTraceGroups);

	if ( errorCode != SUCCESS )
	{
		LTKReturnError(errorCode);
	}

	return SUCCESS;
}
