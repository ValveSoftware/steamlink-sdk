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
 * FILE DESCR: Definitions for the Shape ID to Unicode mapping functions
 *
 * CONTENTS: 
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       September 08 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKSTRENCODING_H
#define __LTKSTRENCODING_H

#include <string>

#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKErrorsList.h"
#include "LTKErrors.h"

//mapping for Tamil basic characters 
static unsigned short tamilIsoCharMap[] = {
//class ID      Unicode value  
/*000*/        0x0b85,  //a
/*001*/        0x0b86,  //aa
/*002*/        0x0b87,  //i
/*003*/        0x0b88,  //ii
/*004*/        0x0b89,  //u
/*005*/        0x0b8a,  //uu
/*006*/        0x0b8e,  //e
/*007*/        0x0b8f,  //E
/*008*/        0x0b90,  //ai
/*009*/        0x0b92,  //o
/*010*/        0x0b93,  //O
/*011*/        0x0b83,  //aytham
/*012*/        0x0b95,  //ka
/*013*/        0x0b99,  //nga
/*014*/        0x0b9a,  //cha
/*015*/        0x0b9e,  //nja
/*016*/        0x0b9f,  //Ta
/*017*/        0x0ba3,  //Na
/*018*/        0x0ba4,  //ta
/*019*/        0x0ba8,  //na
/*020*/        0x0baa,  //pa
/*021*/        0x0bae,  //ma
/*022*/        0x0baf,  //ya
/*023*/        0x0bb0,  //ra
/*024*/        0x0bb2,  //la
/*025*/        0x0bb5,  //va
/*026*/        0x0bb4,  //zha
/*027*/        0x0bb3,  //La
/*028*/        0x0bb1,  //Ra
/*029*/        0x0ba9,  //n2a
/*030*/        0x0bb8,  //sa
/*031*/        0x0bb7,  //sha
/*032*/        0x0b9c,  //ja
/*033*/        0x0bb9   //ha
	};

/**
* @class LTKStrEncoding
* <p> This class contains the methods 
* for mapping shape ID to Unicode </p>
*/

class LTKStrEncoding
{

public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */
	LTKStrEncoding();

	/**
	 * Destructor 
	 */
	virtual ~LTKStrEncoding();

	// @}

	/**
	 * This function maps the vector of shape recognizer strings to the 
	 * 
	 * @param  shapeRecProjectName    The shape recognition project name
     * @param  shapeIDs               the vector of shape ids from the shape recognizer
     * @param  unicodeString          the unicode string output
	 */
	static int shapeStrToUnicode(const string shapeRecProjectName, const vector<unsigned short>& shapeIDs,  vector<unsigned short>& unicodeString);

private:

    static int numShapeStrToUnicode(const vector<unsigned short>& shapeIDs, vector<unsigned short>& unicodeString);
	static int tamilShapeStrToUnicode(const vector<unsigned short>& shapeIDs,  vector<unsigned short>& unicodeString);
	static int tamilCharToUnicode(const unsigned short& shapeID,  vector<unsigned short>& unicodeString);


};
#endif //#ifndef __LTKSTRENCODING_H