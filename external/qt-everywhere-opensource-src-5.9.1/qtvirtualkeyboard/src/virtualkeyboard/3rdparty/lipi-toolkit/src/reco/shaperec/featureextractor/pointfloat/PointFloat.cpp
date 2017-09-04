/*****************************************************************************
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
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
* THE SOFTWARE. 
******************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
// pointFloat.cpp : Defines the entry point for the DLL application.

#include "PointFloat.h"
#include "PointFloatShapeFeatureExtractor.h"
#include "LTKShapeFeatureExtractor.h"
#include "PointFloatShapeFeature.h"
#include "LTKErrors.h"


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

/***************************************************************************
* AUTHOR		: Saravanan R
* DATE			: 15-Mar-2007
* NAME			: createFeatureExtractor
* DESCRIPTION	: Creates instance of type PointFloatShapeFeatureExtractor and retuns of type 
				  LTKShapeFeatureExtractor. (Acts as a Factory Method).
* ARGUMENTS		: 
* RETURNS		: returns an instace of type LTKShapeFeatureExtractor.
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description
*******************************************************************************/
int createShapeFeatureExtractor(const LTKControlInfo& controlInfo,
                                 LTKShapeFeatureExtractor** outFeatureExtractor)
{
	try
	{
		*outFeatureExtractor = new PointFloatShapeFeatureExtractor(controlInfo);
	}
	catch(LTKException e)
	{
		*outFeatureExtractor = NULL;

		LTKReturnError(e.getErrorCode());
	}

    return SUCCESS;
}

/****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 15-Mar-2007
* NAME			: deleteShapeFeatureExtractor
* DESCRIPTION	: Destroy the instance by taking the address as its argument.
* ARGUMENTS		: Address of LTKShapeRecognizerFeatureExtractor instnace.
* RETURNS		: Returns 0 on Success
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description
*****************************************************************************/
int deleteShapeFeatureExtractor(LTKShapeFeatureExtractor *obj)
{
	if ( obj != NULL )
	{
		delete obj;
		obj = NULL;
	}

    return 0;
}
