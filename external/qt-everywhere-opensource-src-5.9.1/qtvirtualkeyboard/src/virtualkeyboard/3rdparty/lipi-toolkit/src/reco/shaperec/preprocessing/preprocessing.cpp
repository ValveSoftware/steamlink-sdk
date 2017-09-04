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
 * $LastChangedDate: 2008-08-01 09:48:58 +0530 (Fri, 01 Aug 2008) $
 * $Revision: 583 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Defines the entry point for the DLL application.
 *
 * CONTENTS:
 *	DllMain
 *	createPreprocInst
 *	destroyPreprocInst
 *
 * AUTHOR: Vijayakumara M.
 *
 * DATE:       July 28, 2005.
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "preprocessing.h"
#include "LTKException.h"
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
/**********************************************************************************
* AUTHOR		: Vijayakumara M.
* DATE			: 13-JULY-2005
* NAME			: create
* DESCRIPTION	: This function creates the object of LTKPreprocessor class and returns 
				  the address of the object.
* ARGUMENTS		:  - N0 -
* RETURNS		: Address of the created object
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int createPreprocInst(const LTKControlInfo& controlInfo,
                        LTKPreprocessorInterface** preprocPtr)
{

	try
	{
		*preprocPtr = new LTKPreprocessor(controlInfo);
	}
	catch(LTKException e)
	{
	    *preprocPtr = NULL;
		LTKReturnError(e.getErrorCode());
	}

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Vijayakumara M.
* DATE			: 13-JULY-2005
* NAME			: destroy
* DESCRIPTION	: Deletes the object,
* ARGUMENTS		: Address of the object of type LTKPreprocessorInterface
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
void destroyPreprocInst(LTKPreprocessorInterface* p)
{    
	delete p;
}
