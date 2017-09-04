/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
* THE SOFTWARE. 
******************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for DP word recognizer 
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       August 17, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of 
 ************************************************************************/

#ifndef __BOXFIELDRECOGNIZER_H
#define __BOXFIELDRECOGNIZER_H

#include "LTKWordRecognizer.h"
#include "LTKLoggerUtil.h"
#include "LTKWordRecoResult.h"
#include "LTKShapeRecoResult.h"

class LTKShapeRecognizer;
class LTKWordRecoConfig;
class LTKOSUtil;

typedef int (*FN_PTR_CREATESHAPERECOGNIZER)(const LTKControlInfo&,  
											LTKShapeRecognizer**);

typedef int (*FN_PTR_DELETESHAPERECOGNIZER)(LTKShapeRecognizer*);


/**
* @class BoxedFieldWordRecognizer
* <p> This class contains the implementation of the Boxed Field
* word recognizer</p>
*/

class BoxedFieldRecognizer : public LTKWordRecognizer  
{
private:

	/** Boxed Field Recognizer config file name */
	string m_boxedConfigFile;

	/** Lipi root */
	string m_lipiRoot;

	/** Lipi libraries */
	string m_lipiLib;

	/** Isolated shape recognizer project used for recognizing each box */
	string m_boxedShapeProject;

	/** Profile for the shape recognition project */
	string m_boxedShapeProfile;

	/**
	 * @name shape recognizer related
	 */

	// @{
	LTKShapeRecognizer *m_shapeRecognizer;	// shape recognizer

	/** Number of results from shape recognizer */
	int m_numShapeRecoResults;

	/** The confidence parameter input to shape recognizer */
	float m_shapeRecoMinConfidence; 

	/** Temporary trace group that holds the strokes for recognition */
	LTKTraceGroup m_boxedChar;

	string m_logFile;

	LTKLogger::EDebugLevel m_logLevel;

	string m_toolkitVersion;

    LTKOSUtil* m_OSUtilPtr;
	// @}

	/**
	 * @name Shape recognizer loading
	 */

	/** Factory method to create the shape recognizer */
	FN_PTR_CREATESHAPERECOGNIZER m_module_createShapeRecognizer;

	/** Factory method to delete the shape recognizer */
	FN_PTR_DELETESHAPERECOGNIZER m_module_deleteShapeRecognizer;
	
	//void * m_hAlgoDLLHandle;  //handle to the wordrecognition algorithm

	/**
	 * @name state of recognizer
	 */
	// @{

	/** Number of characters processed so far  */
	int m_numCharsProcessed;	                 

	/** Number of traces processed so far */
	int m_numTracesProcessed;

	/** maintains decoded recognition results after each trace, upto max style length */
	vector <LTKWordRecoResult> m_decodedResults;  
	
	// @}

public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

    
	/**
	 * Parameterized Constructor
	 */

	 BoxedFieldRecognizer(const LTKControlInfo& controlInfo);
	/**
	 *  Destructor 
	 */

   virtual ~BoxedFieldRecognizer();
	 // @}

  
   /**
	* This method is called from recognition context whenever new traces
	* are added to it.  The Recognizer need to process the new traces 
	* in this methods and updates the internal state.
	* @param rc The recognition context for the current recognition
	*/
     int processInk (LTKRecognitionContext& rc);

   /**
	* This function notifies the recognizer that end of current ink is 
	* the end of a logic segment.  This information could be used in 
	* constraining the recognizer choices
	*/
     void endRecoUnit () ;
     
   /**
	* This is the recognize call. In case of trace by trace recognition
	* The results of the recognition is set on the Recognition context 
	* object.  
	* @param rc The recognition context for the current recognition
	*/
     int recognize (LTKRecognitionContext& rc) ;

   /**
	* This method unloads all the training data
	* To re-initialize the recognizer call the
	* API initialize again
	*/
	int unloadModelData(); 
	
 
   /**
	* This method reset the recognizer.  
	* @param resetParam This parameter could specify what to reset
	*/
     int reset (int resetParam = 0) ;
	// @}
private:

	/**
     *  performs the recognition of the new strokes added to recognition context
     * @param rc The recognitino context
     */

	int recognizeTraces(LTKRecognitionContext& rc);

	/**
     * Erase the state information of the recognizer
     */
    void clearRecognizerState();

	/**
     * This function tries to update the 
     * shape recognition choices with new shape recognition results
     * @param results New results for updating the results
     */
	int updateRecognitionResults(const vector<LTKShapeRecoResult>& results, LTKRecognitionContext& rc);

   /**
    * This function loads and create a shape recognizer
	* from DLL/shared object
	**/
	int createShapeRecognizer(const string& strProjectName, const string& strProfileName,LTKShapeRecognizer** outShapeRecPtr);
    
   /**
    * This function loads functions pointers
	* from the shape recognizer DLL/SO
    **/
	int mapShapeAlgoModuleFunctions();


	int readClassifierConfig();

};
#endif //#ifndef __BOXFIELDRECOGNIZER_H
