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
 * FILE DESCR: The word recognition result class
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 10, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu V.     23-Aug-05       Added update word recognition result method
 ************************************************************************/
#ifndef __LTKWORDRECORESULT_H
#define __LTKWORDRECORESULT_H

#include "LTKTypes.h"

/**
* @class LTKWordFeatureExtractor
* <p> This is an abstract class.  This contains the interface definitions of a wordrecognizer</p>
*/

class LTKWordRecoResult  
{
private:
	
	vector<unsigned short> m_word;  //The result word as unicode string

	float m_confidence;             //confidence returned by the recognizer for this


public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */
	LTKWordRecoResult();

	/**
	 * Constructor that takes two arguements
	 */

	 //LTKWordRecoResult(const vector< unsigned short >& word, float confidence);
	LTKWordRecoResult(const vector< unsigned short >& word, float confidence);

    /**
	 * Destructor
	 */
	virtual ~LTKWordRecoResult();
	// @}

	/**
	 * This method adds to the existing word recognition result
	 * with a new symbol
	 * @param newSymbol - This will be appended to the existing word
	 * @param confidence - confidence of the new symbol, will be added to existing confidence
	 * @return SUCCESS/FAILURE
	 */
	int updateWordRecoResult( unsigned short newSymbol, float confidence);

	/**
	 * This method sets the word recognition result
	 * @param word The result
	 * @param confidence The result
	 * @return SUCCESS/FAILURE
	 */

    //int setWordRecoResult(const vector< unsigned short >& word, float confidence);
	int setWordRecoResult(const vector< unsigned short >& word, float confidence);

	/**
	 * This method gets the word recognition result
	 * @param void
	 * @return the unicode string
	 */
	const vector<unsigned short>& getResultWord() const;

	/**
	 * This method gets the confidence of the result
	 * @param void
	 * @return confidence value
	 */
	float getResultConfidence() const;

	/**
	 * This method sets the confidence of the result
	 * @param float value of confidence
	 * @return SUCCESS if completed successfully
	 */
	int setResultConfidence(float confidence);

};

#endif //#ifndef __LTKWORDRECORESULT_H


