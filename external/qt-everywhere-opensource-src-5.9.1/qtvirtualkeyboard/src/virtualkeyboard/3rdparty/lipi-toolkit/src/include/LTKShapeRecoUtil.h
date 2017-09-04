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
#ifndef __LTKSHAPERECOUTIL_H
#define __LTKSHAPERECOUTIL_H

#include "LTKInc.h"
#include "LTKTypes.h"


class LTKShapeFeature;
class LTKTraceGroup;
class LTKCaptureDevice;
class LTKScreenContext;
class LTKPreprocessorInterface;

#include "LTKShapeFeatureMacros.h"

/**
* @class LTKShapeRecoUtil 
* <p> A utility class that offer some common functionalities to all the classifiers <p>
*/
class LTKShapeRecoUtil
{

public:
//	static FN_PTR_DELETE_SHAPE_FTR_PTR m_deleteShapeFeatureFunc;

	/**
	* @name Constructors and Destructor
	*/
	//@{

	/**
	* Default Constructor
	*/
	LTKShapeRecoUtil();
	
	/**
	* Destructor
	*/
	~LTKShapeRecoUtil();

	// @}

	/**
	 * @name Utility Methods
	 */
	// @{

	/** 
	* 
	* This method is used to convert the relative path to the absolute path	
	*
	* Semantics
	*
	* 	- Split the line into tokens with delimiters \ and / using LTKStringUtil::tokenizeString
	*
	* 	- If first token is not relative path then return SUCCESS
	*
	* 	- Change the relative path to Absolute path ($LIPI_ROOT -> C:\Lipitk)
	*	
	* @param  pathName     : string : Holds the path of the training file
	* @param  lipiRootPath : string : Holds the lipiroot path
	* @return SUCCESS
	* @exception none
	*/
	int getAbsolutePath (const string& inputPath, 
	                            const string& lipiRootPath, 
	                            string& outPath);	

	/**
	* This method reads the project.cfg to find whether the project is dynamic or not
	*
	* Semantics
	*
	* 	- Read the key value pairs defined in project.cfg into a stringStringMap 
	*		projectCfgAttrs = new LTKConfigFileReader(configFilePath.c_str());
	*
	* 	- Read the value for the key PROJECT_CFG_ATTR_NUMSHAPES_STR
	*		numShapesCfgAttr = projectCfgAttrs[PROJECT_CFG_ATTR_NUMSHAPES_STR];
	*
	* 	- If  numShapesCfgAttr == "Dynamic"
	*		-	returnBool = true; and
	*		-	numShapes = 0
	*
	* 	- If numShapesCfgAttr != "Dynamic"
	*		-	Read the value and convert string to an integer
	*			returnBool = false;
	*			valueFromCFG = numShapesCfgAttr;
	*			tempNumShapes = atoi(valueFromCFG.c_str());
	*
	*		-	If tempNumShapes == 0, Project is treated as dynamic
	*
	* @param  configFilePath : string : Holds the path of the project.cfg
	* @param  numShapes : unsigned short : Holds the NumShapes value from config file
	* @param  returnStauts : int : Holds SUCCESS or ErrorValues
	* @param  strNumShapes : string : Holds the NumShapes value from config file
	*
	* @return True : If project is dynamic
	* @return        "  If the value for key PROJECT_CFG_ATTR_NUMSHAPES_STR is "dynamic"
	* @return        "  If value for key PROJECT_CFG_ATTR_NUMSHAPES_STR is 0
	* @return False: otherwise
	*
	* @exception	ECONFIG_FILE_OPEN		Could not open project.cfg
	* @exception	EINVALID_NUM_OF_SHAPES  Negative value for number of shapes
	*/
        int isProjectDynamic(const string& configFilePath, 
                unsigned short& numShapes, 
                string& strNumShapes,
                bool& outIsDynamic );


	/** This method reads the Ink file and check for empty traces
	*
	* <pre>
	* Semantics
	*
	* 	- Call getAbsolutePath to convert the relative path to absolute path
	*
	* 	- Call the readUnipenInkFile to read the ink file from the disk
	*
	* 	- Call the checkEmptyTraces to check for empty traces
	*
	* @param path		 : string : Holds the ink file path
	* @param lipiRootPath: string : Holds the path of Lipi Root
	* @param inTraceGroup: LTKTraceGroup :
	* @param captureDevice: LTKCaptureDevice :
	* @param screenContext: LTKScreenContext :
	* @return 0 if not have empty traces
	* @return 1 if it having empty traces
	* @exception none	
	*/
        int readInkFromFile(const string& path, const string& lipiRootPath,
                LTKTraceGroup& inTraceGroup, 
                LTKCaptureDevice& captureDevice,
                LTKScreenContext& screenContext);

	
	/** This method is used to check the empty traces
	*
	* <pre>
	* Semantics
	*
	* 	- Iterate all the traces in inTraceGroup
	*
	* 	- Check for number of points for each traces to 0
	*	
	* @param  inTraceGroup: LTKTraceGroup :
	* @return 1 if it contains empty trace group,
	* @return 0 otherwise
	* @exception none
	*/

    int convertHeaderToStringStringMap(const string& header, stringStringMap& headerSequence);

    int shapeFeatureVectorToFloatVector(const vector<LTKShapeFeaturePtr>& shapeFeature, 
                                                   floatVector& outFloatVector);

	
	// @}
};

#endif
