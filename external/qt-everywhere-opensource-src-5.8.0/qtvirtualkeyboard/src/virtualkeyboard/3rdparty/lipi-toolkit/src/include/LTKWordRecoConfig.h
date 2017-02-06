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
 * FILE DESCR: Definition of LTKWordRecoConfig holds the config data for 
 *			   the recognizer at the time of loading.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Mudit Agrawal
 *
 * DATE:       Mar 2, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu        24-MAR-2005     Added getGrammarPath
 * 
 ************************************************************************/

#ifndef __LTKWORDRECOCONFIG_H
#define __LTKWORDRECOCONFIG_H

#include "LTKInc.h"
#include "LTKTypes.h"

/**
* @class LTKWordRecoConfig
* <p> This class contains the config data for the recognizer at the time of loading </p>
*/

class LTKWordRecoConfig  
{
private:
	string m_classifierName;				//name of the classifier to be loaded

	string m_dictionaryPath;				//path where dictionaries are present

	string m_grammarPath;					//path where grammar is present

	string m_lipiRoot;						//specifies the lipi root directory

	string m_problemName;					//specifies the logical name for the problem of recognition. e.g. devanagari-word-recognition

	string m_profile;						//refers to main.cfg. This in turn refers to word.cfg and profile.cfg

	string m_script;						//script that is to be recognized

public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

	LTKWordRecoConfig();

	/**
	 * This constrcutor takes one paramater about the recognizer
	 * @param lipiRoot
	 */

	LTKWordRecoConfig(const string& lipiRoot);

	
	/** Destructor */

	~LTKWordRecoConfig();
	
	// @}

	/**
	 * @name Getter Functions
	 */
	// @{

	/**
	 * This function returns the classifier name
	 * @param void
	 *
	 * @return classifier name
	 */

	string getClassifierName() const;

	/**
	 * This function returns the Dictionary Path
	 * @param void
	 *
	 * @return Dictionary Path
	 */

	string getDictionaryPath() const;

	/**
	 * This function returns the Grammar Path
	 * @param void
	 *
	 * @return Grammar Path
	 */

	string getGrammarPath() const;

	/**
	 * This function returns the lipi root
	 * @param void
	 *
	 * @return lipi root
	 */

	string getLipiRoot() const;

	/**
	 * This function returns the profile
	 * @param void
	 *
	 * @return profile
	 */

	string getProfile() const;

	/**
	 * This function returns the script
	 * @param void
	 *
	 * @return script name
	 */

	string getScript() const;

	/**
	 * This function returns the problem Name
	 * @param void
	 *
	 * @return problem Name
	 */

	string getProblemName() const;

	/**
	 * @name Setter Functions
	 */
	// @{

	/**
	 * This function reads the main config file and inturn other config files also (defined in main.cfg)
	 * 
	 * @param path of the main config file
	 *
	 * @return SUCCESS on successful read operation of all config files
	 */

	int readConfigFile(const string& configFileName);

	/**
	 * @name Setter Functions
	 */
	// @{

	/**
	 * This function sets the lipi root 
	 * 
	 * @param lipi root
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setLipiRoot(const string& lipiRoot);



};


#endif


