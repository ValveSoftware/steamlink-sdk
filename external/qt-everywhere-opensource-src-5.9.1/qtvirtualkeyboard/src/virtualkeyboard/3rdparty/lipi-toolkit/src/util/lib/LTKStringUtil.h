/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for the String Splitter Module
 *
 * CONTENTS: 
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKSTRINGTOKENIZER_H
#define __LTKSTRINGTOKENIZER_H

#include "LTKInc.h"

/**
* @ingroup util
*/

/** @brief Utility class to tokenize a string on any given delimiter
@class LTKStringUtil
*/

class LTKStringUtil
{

public:

   /**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */

	LTKStringUtil();

   /**
	* Destructor
	*/

	~LTKStringUtil();

	// @}

	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This is a static method which splits a string at the specified delimiters
	 * @param str String to be split
	 * @param tokens The split sub-strings
	 * @param delimiters The symbols at which the string is to be split at
	 * @return SUCCESS on successful split operation
	 */

	static int tokenizeString(const string& str, const string& delimiters, vector<string>& outTokens);

    static void convertIntegerToString(int intVariable, string& outStr);

    static void convertFloatToString(float floatVariable, string& outStr);

    static float convertStringToFloat(const string& str);

	static void trimString(string& str);

    static bool isFloat(const string& inputStr);

	static bool isInteger(const string& inputStr);

	// @}

};

#endif	//#ifndef __LTKSTRINGTOKENIZER_H
