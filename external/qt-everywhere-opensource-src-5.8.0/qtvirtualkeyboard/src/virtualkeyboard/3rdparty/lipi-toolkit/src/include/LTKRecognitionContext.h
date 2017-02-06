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
 * FILE DESCR: Definitions for Recognition Context Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       February 21, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Thanigai		3-AUG-2005		Added default constructor and setWordRecoEngine 
 *								methods.
 *
 * Deepu       30-AUG-2005      Replaced LTKWordRecoEngine with LTKWordRecognizer
 *                              Changed the representation of m_recognitionFlags 
 *                              since there was a problem with dlls
 ************************************************************************/
#ifndef __LTKRECOGNITIONCONTEXT_H 
#define __LTKRECOGNITIONCONTEXT_H

#include "LTKTypes.h"
#include "LTKCaptureDevice.h"
#include "LTKScreenContext.h"

class LTKTrace;
class LTKTraceGroup;
class LTKWordPreprocessor;
class LTKWordRecoResult;
class LTKWordRecognizer;
typedef vector<LTKTraceGroup> LTKTraceGroupVector;

typedef vector<LTKWordRecoResult> LTKWordRecoResultVector;


/** 
 * @class LTKRecognitionContext This class holds the recognition context for a particular field.   
 * <p> 
 * LTKRecognitionContext specifies UI parameters (like Screen context, device context, segment info etc.)
 * , application specific parameters (lexicon to be used, other language models) and recognition 
 * related configurations (number of results, confidence threshold etc)</p>
 *
 */


class LTKRecognitionContext  
{
private:
	float m_confidThreshold;              //confidence threshold for the recognizer

	LTKCaptureDevice m_deviceContext;     //the device context in which the ink is collected

    LTKTraceVector m_fieldInk;                     //the input ink to be recognized

	int m_numResults;							   //number of results that the recognizer will return

	vector< pair<string,int> > m_recognitionFlags; //recognition flags.

	stringStringMap m_languageModels;     //the language models that contains dictionary name, grammar model etc.

	LTKScreenContext m_screenContext;     //the screen context object
	
	LTKWordRecognizer* m_wordRecPtr;        //the recognition engine    
    
	LTKWordRecoResultVector m_results;    //Results from the recognizer

	int m_nextBestResultIndex ;             //this points to the next best result to be returned


public:
   /**
	* @name Constructors and Destructor
	*/

	// @{

	LTKRecognitionContext();
   
   
	LTKRecognitionContext(LTKWordRecognizer *wordRecPtr );


	int setWordRecoEngine(LTKWordRecognizer *wordRecPtr);

	~LTKRecognitionContext();
    // @}
       
   /**
	* @name Functions to add traces to the recognition context
	*/
	// @{

   /**
    * This function adds a trace to the recognition context for recognition
    * @param trace is the trace to be added
    * @return SUCCESS/FAILURE
    */
    int addTrace (const LTKTrace& trace);

   /**
    * This function adds a vector of tracegroup for recognition in the recognition context
    * @param fieldInk is the ink to be added.
    * @return SUCCESS/FAILURE
    */

    int addTraceGroups (const LTKTraceGroupVector& fieldInk);

   /**
    * This function marks the beginning of a recognition unit of Ink.
    * The recognition unit is determined based on the flag set in recognition flags
    * @return SUCCESS/FAILURE
    */

   void beginRecoUnit ( );

   /**
    * This function clears all the recognition results
    * @param void
    * @return SUCCESS/FAILURE
    */

   //void clearRecognitionResult( );
	int clearRecognitionResult( );

   /**
    * This function marks the ending of a recognition unit of Ink.
    * The recognition unit is determined based on the flag set in recognition flags
    * @return SUCCESS/FAILURE
    */

   //void endRecoUnit ( );
    void endRecoUnit ( );

   // @}

   /**
	* @name get/set Functions
	*/
	// @{

   /**
    * This function returns a reference to internal Ink data.  
    * The recognition engine uses the reference to access the data
    * @return const reference to vector of LTKTrace 
    */
    const LTKTraceVector& getAllInk () const;
   
   /**
    * Returns the value of confidence threshold
    * @return float confidence threshold value
    */
    float getConfidThreshold ()  const;

   /**
    * This function returns current device const.
    * The recognition engine uses the reference to access the data
    * @return const reference to device context object
    */
    const LTKCaptureDevice& getDeviceContext ( )   const;

   /**
    * This function retrieves flag indexed by string key from the map and returns the value
    * @param key index of map
    * @return value(integer) of the queried flag
    */

   //int getFlag (const string& key,int& value)  const;
    int getFlag (const string& key,int& outValue)  const;

   /**
    * This function returns the current language model indexed by the key
    * @param key : the index
    * @return value (string) of the queried language model
    */
    //int getLanguageModel (string& key,string& value) const;
    int getLanguageModel (const string& key,string& outValue) const;

   /**
    * This function returns the next best results
    * @param numResults : number of results to be retrieved.
    * @param results[out] :Result vector returned
    * @return SUCCESS/FAILURE
    */
    int  getNextBestResults (int numResults, 
		            		 LTKWordRecoResultVector& outWordRecResults);


   /**
    * This function returns the number of results set in the recognition context.
    * @return number of results
    */
    int getNumResults ()  const;

   /**
    * This function returns the screen context
    * @return const reference to screen context
    */
    const LTKScreenContext& getScreenContext ( ) const;


   /**
    * This function gives top result of word recognition
    * @return const reference to screen context
    */
    int getTopResult (LTKWordRecoResult& outTopResult);
   
   /**
    * This function set the confidence threshold
	* @param thresh : the threshold value to be set
    * @return SUCCESS/FAILURE
    */

   //int setConfThreshold (float thresh);
    int setConfidThreshold (float thresh);

   /**
    * This function set the device context
	* @param dc: the reference to be LTKDeviceContext to be set
    * @return SUCCESS/FAILURE
    */
    void setDeviceContext (const LTKCaptureDevice& dc);

   /**
    * This function sets different flags 
	* @param key : name of the flag to be set
	* @param value : value to be set
    * @return SUCCESS/FAILURE
    */
    //void setFlag (const string& key, int value);
    int setFlag (const string& key, int value);

   /**
    * This function sets language models
	* @param property : name of the flag to be set
	* @param value : value to be set
    * @return SUCCESS/FAILURE
    */

   //void setLanguageModel (const string& property, const string& value);
    int setLanguageModel (const string& property, const string& value)  ;

   /**
    * This function sets number of results required from the recognizer
	* @param num : number of results
	* @param value : value to be set
    * @return SUCCESS/FAILURE
    */
    int setNumResults (int numResults);

   /**
    * This function sets the screen context object for the recognition
	* @param sc : the screen context object to be set
    * @return SUCCESS/FAILURE
    */

   //void setScreenContext (const LTKScreenContext& sc);
    void setScreenContext (const LTKScreenContext& sc);
   
    // @}
   
   /**
	* @name Recognition related functions
	*/

	// @{
   /**
    * This function is used by the recognizer to set the results back in the recognition context
	* @param result : the LTKRecognitionResult object to be added
    * @return SUCCESS/FAILURE
    */

    //void addRecognitionResult (const LTKWordRecoResult& result);
    void addRecognitionResult (const LTKWordRecoResult& result);

   /**
    * This function is the recogniz call from the application
	* This calls the recognize method of the recognizer internally
    * @return SUCCESS/FAILURE
    */
    int recognize ();
   // @}

   /**
	* @name Misc
	*/

   // @{

   /**
    * This function is used to reset the different components of recognition context
	* @param resetParam : parameter that identifies the component to be reset
    * @return SUCCESS/FAILURE
    */
    int reset (int resetParam);

   
   // @}

};

#endif // #ifndef __LTKRECOGNITIONCONTEXT_H


