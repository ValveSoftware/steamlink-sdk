/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
*******************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKScreenContext which holds the co-ordinates 
               of the writing area provided for the set of traces being sent 
			   for recognition 
 *
 * CONTENTS: 
 *	getBboxLeft
 *	getBboxBottom
 *	getBboxRight
 *	getBboxTop
 *	setBboxLeft
 *	setBboxBottom
 *	setBboxRight
 *	setBboxTop
 *
 * AUTHOR:     Mudit Agrawal.
 *
 * DATE:       February 23, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKScreenContext.h"
#include "LTKMacros.h"


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKScreenContext
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKScreenContext::LTKScreenContext() :
	m_bboxBottom(0.0),
	m_bboxLeft(0.0),
	m_bboxRight(0.0),
	m_bboxTop(0.0)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::LTKScreenContext()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::LTKScreenContext()" << endl;*/
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKScreenContext
* DESCRIPTION	: Initialization Constructor
* ARGUMENTS		: bboxLeft - left x co-ordinate of the writing area
*				  bboxBottom - bottom y co-ordinate of the writing area
*				  bboxRight - right x co-ordinate of the writing area
*                 bboxTop - top y co-ordinate of the writing area
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKScreenContext::LTKScreenContext(float bboxLeft, float bboxBottom, 
								   float bboxRight,float bboxTop) : 
	m_bboxLeft(bboxLeft), 
	m_bboxBottom(bboxBottom),
	m_bboxRight(bboxRight), 
	m_bboxTop(bboxTop)
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::LTKScreenContext(float,float,float,float)" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::LTKScreenContext(float,float,float,float)" << endl;*/
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getBboxLeft
* DESCRIPTION	: gets the left x co-ordinate of the writing area
* ARGUMENTS		: 
* RETURNS		: left x co-ordinate of the writing area
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

float LTKScreenContext::getBboxLeft() const 
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getBboxLeft()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getBboxLeft()" << endl;*/

	return m_bboxLeft;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getBboxBottom
* DESCRIPTION	: gets the bottom y co-ordinate of the writing area
* ARGUMENTS		: 
* RETURNS		: bottom y co-ordinate of the writing area
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

float LTKScreenContext::getBboxBottom() const 
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getBboxBottom()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getBboxBottom()" << endl;*/

	return m_bboxBottom;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getBboxRight
* DESCRIPTION	: gets the right x co-ordinate of the writing area
* ARGUMENTS		: 
* RETURNS		: right x co-ordinate of the writing area
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

float LTKScreenContext::getBboxRight() const 
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getBboxRight()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getBboxRight()" << endl;*/

	return m_bboxRight;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getBboxTop
* DESCRIPTION	: gets the top y co-ordinate of the writing area
* ARGUMENTS		: 
* RETURNS		: top y co-ordinate of the writing area
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

float LTKScreenContext::getBboxTop() const 
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getBboxTop()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getBboxTop()" << endl;*/

	return m_bboxTop;
}

/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 01-MAR-2005
* NAME			: getAllHLines
* DESCRIPTION	: gets the horizontal lines in the screen context
* ARGUMENTS		: none
* RETURNS		: const reference to vector of ordinates of horizontal lines
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

const floatVector& LTKScreenContext::getAllHLines() const 
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getAllHLines()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getAllHLines()" << endl;*/

	return m_hLines;
}

/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 01-MAR-2005
* NAME			: getAllVLines
* DESCRIPTION	: gets the horizontal lines in the screen context
* ARGUMENTS		: none
* RETURNS		: const reference to vector of ordinates of horizontal lines
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

const floatVector& LTKScreenContext::getAllVLines() const 
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::getAllVLines()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::getAllVLines()" << endl;*/

	return m_vLines;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setBboxLeft
* DESCRIPTION	: sets the left x co-ordinate of the writing area
* ARGUMENTS		: bboxLeft - left x co-ordinate of the writing area
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKScreenContext::setBboxLeft(float bboxLeft)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::setBboxLeft()" << endl;*/

	if(bboxLeft <0)
	{
		return FAILURE;
	}

	m_bboxLeft = bboxLeft;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::setBboxLeft()" << endl;*/

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setBboxBottom
* DESCRIPTION	: sets the bottom y co-ordinate of the writing area
* ARGUMENTS		: bboxBottom - bottom y co-ordinate of the writing area
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKScreenContext::setBboxBottom(float bboxBottom)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::setBboxBottom()" << endl;*/

	if(bboxBottom<0)
	{
		return FAILURE;
	}

	m_bboxBottom = bboxBottom;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::setBboxBottom()" << endl;*/

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setBboxRight
* DESCRIPTION	: sets the right x co-ordinate of the writing area
* ARGUMENTS		: bboxRight - right x co-ordinate of the writing area
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKScreenContext::setBboxRight(float bboxRight)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::setBboxRight()" << endl;*/

	if(bboxRight<0)
	{
		return FAILURE;
	}

	m_bboxRight = bboxRight;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::setBboxRight()" << endl;*/

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setBboxTop
* DESCRIPTION	: sets the top y co-ordinate of the writing area
* ARGUMENTS		: bboxTop - top y co-ordinate of the writing area
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKScreenContext::setBboxTop(float bboxTop)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::setBboxTop()" << endl;*/

	if(bboxTop<0)
	{
		return FAILURE;
	}

	m_bboxTop = bboxTop;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::setBboxTop()" << endl;*/

	return SUCCESS;
}
/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 01-MAR-2005
* NAME			: addHLine
* DESCRIPTION	: This function adds a horizontal line in the screen context
* ARGUMENTS		: ordinate - position of the horizontal line 
* RETURNS		: SUCCESS on successful add operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
int LTKScreenContext::addHLine(float ordinate)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::addHLine()" << endl;*/

	if(ordinate<0)
	{
		return FAILURE;
	}

	m_hLines.push_back(ordinate);

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::addHLine()" << endl;*/

	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 01-MAR-2005
* NAME			: addVLine
* DESCRIPTION	: This function adds a vertical line in the screen context
* ARGUMENTS		: ordinate - position of the vertical line 
* RETURNS		: SUCCESS on successful add operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKScreenContext::addVLine(float abscissa)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKScreenContext::addVLine()" << endl;*/

	if(abscissa <0)
	{
		return FAILURE;
	}

	m_vLines.push_back(abscissa);

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKScreenContext::addVLine()" << endl;*/

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKScreenContext
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKScreenContext::~LTKScreenContext ()
{
}


