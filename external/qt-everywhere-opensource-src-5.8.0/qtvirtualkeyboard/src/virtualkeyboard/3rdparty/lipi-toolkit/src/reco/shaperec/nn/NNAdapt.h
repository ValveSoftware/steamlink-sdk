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
 * $LastChangedDate: 2007-06-01 11:16:10 +0530 (Fri, 01 Jun 2007) $
 * $Revision: 105 $
 * $Author: sharmnid $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Implements NNShapeRecognizer::Adapt
 *
 * CONTENTS:   
 *
 * AUTHOR:     Tarun Madan
 *
 * DATE:       30-Aug-2007
 * CHANGE HISTORY:
 * Author		Date			Description 
 ************************************************************************/

class LTKAdapt
{
private:
	LTKAdapt(NNShapeRecognizer* ptrNNShapeReco);
	static LTKAdapt* adaptInstance;
	static int m_count;
	
public:	
	static LTKAdapt* getInstance(NNShapeRecognizer* ptrNNShapeReco);
	int adapt(int shapeId);
	~LTKAdapt();
	void deleteInstance();

private:
		/**< @brief Pointer to NNShapeRecognizer
	*	<p>
	*
	*	</p>
	*/
	NNShapeRecognizer* m_nnShapeRecognizer;

	/**< @brief Adapt Scheme name
	*	<p>
	*
	*	</p>
	*/
	string m_adaptSchemeName;

        /**< @brief Minimum number of samples required per class 
	*	<p>
	*
	*	</p>
	*/
      
        int m_minNumberSamplesPerClass;


	/**
	* This method reads Config variables related to Adapt from CFG
	*
	* Semantics
	*
	*
	* @param none
	*
	* @return SUCCESS:  
	*         FAILURE:  return ErrorCode
	* @exception none
	*/
	int readAdaptConfig();
	
	/**
	* This method implements AddLVQ scheme of Adaptation
	*
	* Semantics
	*	- if wrong recognition or new style, 
	*     then 
	*			ADD
	*     else
	*			Morph 
	*	- Update Prototypeset and Update MDT File
	*
	* @param shapeId			: int 			: Holds shapeId of adaptSampleTrace
	*
	* @return SUCCESS:  return 0
	*         FAILURE:  return ErrorCode
	* @exception none
	*/
	int adaptAddLVQ(int shapeId);
};
