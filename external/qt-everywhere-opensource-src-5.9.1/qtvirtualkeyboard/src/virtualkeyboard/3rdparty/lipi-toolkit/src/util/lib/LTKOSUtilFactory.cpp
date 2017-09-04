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
********************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-08-12 13:24:43 +0530 (Tue, 12 Aug 2008) $
 * $Revision: 610 $
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

#include "LTKOSUtil.h"
#include "LTKOSUtilFactory.h"
#include "LTKMacros.h"

#ifdef WINCE
	#include "LTKWinCEUtil.h"
#elif defined WIN32
	#include "LTKWindowsUtil.h"
#else
	#include "LTKLinuxUtil.h"
#endif


/*************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 29-May-2008
* NAME			: getInstance
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
**************************************************************************/
LTKOSUtil* LTKOSUtilFactory::getInstance()
{
	#ifdef WINCE
        return new LTKWinCEUtil();
	#elif defined WIN32
        return new LTKWindowsUtil();
	#else	
        return new LTKLinuxUtil();
	#endif
}
