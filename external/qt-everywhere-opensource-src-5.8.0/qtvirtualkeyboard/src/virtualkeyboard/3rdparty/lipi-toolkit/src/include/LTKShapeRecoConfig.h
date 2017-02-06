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
 * FILE DESCR: Definition of LTKShapeRecoConfig which holds the configuration information read 
 *			   from the configuration files
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKSHAPERECOCONFIG_H
#define __LTKSHAPERECOCONFIG_H

#include "LTKTypes.h"

/**
 * @class LTKShapeRecoConfig
 * <p> This class contains data about the location of the configuration files which are to be
 * loaded during the instanatiation of various shape recognizers. It is passed as an argument during
 * the instantiation of a shape recognizer</p>
 */

class LTKShapeRecoConfig
{

private:

	string m_lipiRoot;				//	root path of the lipi tool kit

	string m_shapeSet;				//	shapeset of the recognition problem

	string m_profile;				//	profile of the recognition problem

	string m_shapeRecognizerName;	//	logical name of the shape recognizer

	int m_numShapes;				//	number of shapes in the recognition problem

public:

	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

	LTKShapeRecoConfig();

	/**
	 * This constructor takes various arguments which are used to intialize the members of
	 * this class during its instantiation.
	 * @param lipiRoot Root path of Lipi Toolkit
	 * @param shapeSet Shape set
	 * @param profile Profile name
	 * @param shapeRecongizerName Name of the shape recognizer
	 * @param numShapes number of shapes in the recognition problem
	 */

	LTKShapeRecoConfig(const string& lipiRoot, const string& shapeSet, const string& profile, 
		      const string& shapeRecognizerName, int numShapes);

	/**
	 * Destructor
	 */

	~LTKShapeRecoConfig();

	// @}

	/**
	 * @name Methods
	 */

	// @{

	/**
	 * This function reads the configuration information from the config files
	 * @param configFile - full path of the main configuration file
	 *
	 * @return SUCCESS on successful read operation
	 */

	int readConfigInfo(const string& configFile);

	// @}

	/**
	 * @name Getter Functions
	 */
	// @{

	/**
	 * This function returns the Lipi Root
	 * @param void
	 *
	 * @return Lipi Root.
	 */

	const string& getLipiRoot();

	/**
	 * This function returns the Shape Set Directory name
	 * @param void
	 *
	 * @return Shape Set Directory name.
	 */

	const string& getShapeSet();

	/**
	 * This function returns the Profile Directory name
	 * @param void
	 *
	 * @return Profile Directory name.
	 */

	const string& getProfile();

	/**
	 * This function returns the shape recognizer name
	 * @param void
	 *
	 * @return shape recognizer name.
	 */

	const string& getShapeRecognizerName();

	/**
	 * This function returns the number of shapes in the recognition problem
	 * @param void
	 *
	 * @return number of shapes in the recognition problem.
	 */

	int getNumShapes();

	// @}

	/**
	 * @name Setter Functions
	 */

	// @{

	/**
	 * This function sets the Lipi Root Path
	 * @param lipiRootStr Lipi Root Path
	 */

	int setLipiRoot(const string& lipiRootStr);

	/**
	 * This function sets the Shape Set Directory name
	 * @param shapeSetStr Shape Set Directory name
	 */

	int setShapeSet(const string& shapeSetStr);

	/**
	 * This function sets the Profile Directory name
	 * @param profileStr Profile Directory name
	 */

	int setProfile(const string& profileStr);

	/**
	 * This function sets shape recognizer name
	 * @param shapeRecognizerName shape recognizer name
	 */

	int setShapeRecognizerName(const string& shapeRecognizerName);

	/**
	 * This function sets number of shapes in the recognition problem
	 * @param numShapes number of shapes in the recognition problem
	 */

	int setNumShapes(int numShapes);

	// @}

};

#endif

//#ifndef __LTKSHAPERECOCONFIG_H
//#define __LTKSHAPERECOCONFIG_H


