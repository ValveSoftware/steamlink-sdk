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
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for the Configuration File Reader Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description
 ************************************************************************/

#ifndef __LTKCONFIGFILEREADER_H
#define __LTKCONFIGFILEREADER_H

#include "LTKTypes.h"

/**
* @ingroup util
*/

/** @brief A utility class for reading the key value pairs defined in Lipi Config Files
* @class LTKConfigFileReader
*/
class LTKConfigFileReader
{

private :
	/** @brief A string string map that holds the key-value pairs defined in the config file
	*/
	stringStringMap m_cfgFileMap;
	string m_configFilePath;
	
public:

	/**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Parameterized Constructor
	 *
	 * <p>
	 * The constructor takes in the path of the confg file to be read as paramater and
	 * reads the key-value pairs defined in that class to the data member LTKConfigFileReader::m_cfgFileMap
	 * </p>
	 */

	LTKConfigFileReader(const string& configFilePath);

   /**
	* Destructor
	*/

	~LTKConfigFileReader();

	// @}

	/** @name public methods
	*/

	// @{

	/** @brief Returns the value for the key defined in the config file
	* <p>
	* Returns the value for the key if the key is defined in the config file and empty string otherwise.
	* </p>
	* @param key 	Type : string (constant)
	* @returns 		Type : string
	*/
	int getConfigValue(const string& key, string& outValue);


	/** @brief Returns true if no key-value pairs were defined in the config file
	* 
	* @returns 		Type : bool, true : if config file has no valid key-value pairs and false otherwise.
	*/
	 bool isConfigMapEmpty(); 
	//bool isConfigFileEmpty();
	// @}
	/** @brief Returns the stringStringMap of m_cfgFileMap
	* 
	*/
	const stringStringMap& getCfgFileMap();
	// @}

private:
	
	/**
	 * @name Methods
	 */

	// @{

	/**
	 * Reads key-value pairs from a file into a map
	 */

	int getMap();

	//@}
};

#endif

