/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
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
 * $LastChangedDate:  $
 * $Revision:  $
 * $Author:  $
 *
 ************************************************************************/
// SubStroke.cpp : Defines the entry point for the DLL application.

#include "SubStroke.h"
#include "SubStrokeShapeFeatureExtractor.h"
#include "LTKShapeFeatureExtractor.h"
#include "SubStrokeShapeFeature.h"
#include "LTKErrors.h"


#ifdef _WIN32
#include <windows.h>
BOOL APIENTRY DllMain(HANDLE hModule,
					  DWORD ul_reason_for_call,
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

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: createShapeFeatureExtractor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int: 
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int createShapeFeatureExtractor(const LTKControlInfo& controlInfo,
                                 LTKShapeFeatureExtractor** outFeatureExtractor)
{
	try
	{
		*outFeatureExtractor = new SubStrokeShapeFeatureExtractor(controlInfo);
	}
	catch(LTKException e)
	{
		*outFeatureExtractor = NULL;

		LTKReturnError(e.getErrorCode());
	}

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: deleteShapeFeatureExtractor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int: 
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int deleteShapeFeatureExtractor(LTKShapeFeatureExtractor *obj)
{
	if ( obj != NULL )
	{
		delete obj;
		obj = NULL;
	}

    return 0;
}
