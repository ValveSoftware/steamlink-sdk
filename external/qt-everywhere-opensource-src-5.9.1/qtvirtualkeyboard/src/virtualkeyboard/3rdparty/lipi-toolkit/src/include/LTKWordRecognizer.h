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
 * FILE DESCR: The abstract class for a word recognizer
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 10, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKWORDRECOGNIZER_H
#define __LTKWORDRECOGNIZER_H

#include "LTKTypes.h"

#include "LTKInc.h"

#include "LTKTraceGroup.h"

class LTKRecognitionContext;
/**
* @class LTKWordFeatureExtractor
* <p> This is an abstract class.  This contains the interface definitions of a wordrecognizer</p>
*/

class LTKWordRecognizer  
{
protected:
		const string m_wordRecognizerName;	//	name of the word recognizer class deriving from the LTKShapeRecognizer class

public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */
     LTKWordRecognizer()
	 {

	 }

    /**
	 * Initialization Constructor. Initialzes the member m_shapeRecognizerName
	 */

	 LTKWordRecognizer(const string& wordRecognizerName):
	 m_wordRecognizerName(wordRecognizerName)
	 {

	 }
     
	/**
	 * Virtual Destructor 
	 */
     virtual ~LTKWordRecognizer()
	 {

	 }

	 // @}

   /**
	* This is a pure virtual method to be implemented by the derived class.
	* This method is called from recognition context whenever new traces
	* are added to it.  The Recognizer need to process the new traces 
	* in this methods and updates the internal state.
	* @param rc The recognition context for the current recognition
	*/

     //virtual int processInk (const LTKRecognitionContext& rc) = 0;
     virtual int processInk (LTKRecognitionContext& rc) = 0;

   /**
	* This is a pure virtual method to be implemented by the derived class.
	* This function notifies the recognizer that end of current ink is 
	* the end of a logic segment.  This information could be used in 
	* constraining the recognizer choices
	*/
     virtual void endRecoUnit () = 0;
     
   /**
	* This is a pure virtual method to be implemented by the derived class.
	* This is the recognize call. In case of trace by trace recognition
	* The results of the recognition is set on the Recognition context 
	* object.  
	* @param rc The recognition context for the current recognition
	*/

    //   virtual int recognize (const LTKRecognitionContext& rc) = 0;
     virtual int recognize (LTKRecognitionContext& rc) = 0;
 
   /**
	* This is a pure virtual method to be implemented by the derived class.
	* This method reset the recognizer.  
	* @param resetParam This parameter could specify what to reset
	*/
     virtual int reset (int resetParam = 0) = 0;

   /**
	* This method unloads all the training data
	* To re-initialize the recognizer call the
	* API initialize again
	*/
	virtual int unloadModelData() =0; 

}; 

#endif // #ifndef __LTKWORDRECOGNIZER_H


