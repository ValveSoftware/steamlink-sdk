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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of boxfld dll exported functions.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       Aug 23, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of 
 ************************************************************************/
#include "boxfld.h"
#include "LTKMacros.h"
#include "LTKException.h"


#ifdef _WIN32

//DLL MAIN for Windows
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
* AUTHOR		: Deepu V
* DATE			: 23-Aug-2005
* NAME			: createWordRecognizer
* DESCRIPTION	: Crates instance of type BoxField Recongnizer and retuns of type 
				  LTKWordRecongizer. (Acts as a Factory Method).
* ARGUMENTS		: 
* RETURNS		: returns an instace of type LTKWordRecoginzer.
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description of 
*************************************************************************************/
int createWordRecognizer(const LTKControlInfo& controlInfo,LTKWordRecognizer** boxFldRecoPtr)
{
 
    try
    {
        *boxFldRecoPtr = (LTKWordRecognizer*) new BoxedFieldRecognizer(controlInfo);
    }
    catch(LTKException e)
    {
        *boxFldRecoPtr = NULL;
		LTKReturnError(e.getErrorCode());
    }

	return SUCCESS;
    
}

/**********************************************************************************
* AUTHOR		: Deepu V
* DATE			: 23-Aug-2005
* NAME			: deleteWordRecognizer
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: returns an instace of type LTKWordRecoginzer.
* NOTES			: 
* CHANGE HISTORY
* Author			Date				Description of 
*************************************************************************************/
//Destroys the instance by taking its addess as its argument.
int deleteWordRecognizer(LTKWordRecognizer* obj)
{
	/*delete obj;

	obj = NULL;

	return SUCCESS;*/

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
