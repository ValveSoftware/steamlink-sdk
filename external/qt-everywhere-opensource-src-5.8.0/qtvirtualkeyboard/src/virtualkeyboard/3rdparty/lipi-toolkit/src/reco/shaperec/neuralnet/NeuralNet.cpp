// NEURALNET.cpp : Defines the entry point for the DLL application.
//

#include "NeuralNet.h"
#include "LTKShapeRecognizer.h"
#include "NeuralNetShapeRecognizer.h"
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
		*ptrObj = new NeuralNetShapeRecognizer(controlInfo);
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
	int errorCode = ((NeuralNetShapeRecognizer*)obj)->getTraceGroups(shapeId,		
			numberOfTraceGroups,  outTraceGroups);

	if ( errorCode != SUCCESS )
	{
		LTKReturnError(errorCode);
	}

	return SUCCESS;
}