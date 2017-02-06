/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies 
* or substantial portions of the Software.
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
* $LastChangedDate: 2011-02-08 16:57:52 +0530 (Tue, 08 Feb 2011) $
* $Revision: 834 $
* $Author: mnab $
*
************************************************************************/
/************************************************************************
* FILE DESCR: Implementation for ActiveDTW Shape Recognition module
*
* CONTENTS:
*
* AUTHOR: S Anand 
*
w
* DATE:  3-MAR-2009  
* CHANGE HISTORY:
* Author       Date            Description of change
************************************************************************/

#include "LTKConfigFileReader.h"

#include "ActiveDTWShapeRecognizer.h"

#include "LTKPreprocDefaults.h"

#include "LTKHierarchicalClustering.h"

#include "LTKPreprocessorInterface.h"

#include "LTKShapeFeatureExtractorFactory.h"

#include "LTKShapeFeatureExtractor.h"

#include "LTKShapeFeature.h"

#include "LTKVersionCompatibilityCheck.h"

#include "LTKInkFileWriter.h"
#include "LTKOSUtil.h"
#include "LTKOSUtilFactory.h"
#include "LTKClassifierDefaults.h"


/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ActiveDTWShapeRecognizer
* DESCRIPTION	: Default Constructor that initializes all data members
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/

void ActiveDTWShapeRecognizer::assignDefaultValues()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::assignDefaultValues()" << endl;
	
    m_numShapes = 0;
    m_activedtwCfgFilePath = "";
    m_activedtwMDTFilePath = "";
    m_ptrPreproc = NULL;
    m_projectTypeDynamic=false;
    m_prototypeSelection=NN_DEF_PROTOTYPESELECTION;
    m_prototypeReductionFactor=NN_DEF_PROTOTYPEREDUCTIONFACTOR;
    m_nearestNeighbors=NN_DEF_NEARESTNEIGHBORS;
    m_dtwBanding=NN_DEF_BANDING;
    m_dtwEuclideanFilter= ACTIVEDTW_DEF_DTWEUCLIDEANFILTER;
    m_preProcSeqn=NN_DEF_PREPROC_SEQ;
    m_ptrFeatureExtractor=NULL;
    m_featureExtractorName=NN_DEF_FEATURE_EXTRACTOR;
    m_numClusters=NN_NUM_CLUST_INITIAL; 
    m_MDTUpdateFreq=NN_DEF_MDT_UPDATE_FREQ;
    m_prototypeSetModifyCount=0;
    m_rejectThreshold=NN_DEF_REJECT_THRESHOLD;
    m_adaptivekNN=false;
    m_deleteLTKLipiPreProcessor=NULL;
    m_minClusterSize = ADAPT_DEF_MIN_NUMBER_SAMPLES_PER_CLASS;
    m_percentEigenEnergy = ACTIVEDTW_DEF_PERCENTEIGENENERGY;
    m_eigenSpreadValue = ACTIVEDTW_DEF_EIGENSPREADVALUE;
    m_useSingleton = ACTIVEDTW_DEF_USESINGLETON;
    m_MDTFileOpenMode = NN_MDT_OPEN_MODE_ASCII;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::assignDefaultValues()" << endl;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: initialize
* DESCRIPTION	: This method initializes the ActiveDTW shape recognizer
* ARGUMENTS		: string  Holds the Project Name
*				  string  Holds the Profile Name
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no erros
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer(const LTKControlInfo& controlInfo):
m_OSUtilPtr(LTKOSUtilFactory::getInstance()),
m_libHandler(NULL),
m_libHandlerFE(NULL)
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" << endl;
	
    try
    {
		LTKControlInfo tmpControlInfo=controlInfo;
		
		string strProjectName = "";
		string strProfileName = "";
		
		
		if( (tmpControlInfo.projectName).empty() )
		{
			throw LTKException(EINVALID_PROJECT_NAME);
		}
		if( (tmpControlInfo.lipiRoot).empty() )
		{
			throw LTKException(ELIPI_ROOT_PATH_NOT_SET);
		}
		
		if( (tmpControlInfo.profileName).empty() )
		{
			strProfileName = DEFAULT_PROFILE;
			tmpControlInfo.profileName = strProfileName;
		}
		
		if ( tmpControlInfo.toolkitVersion.empty() )
		{
			throw LTKException(ENO_TOOLKIT_VERSION);
		}
		
		assignDefaultValues();
		
		m_lipiRootPath = tmpControlInfo.lipiRoot;
		m_lipiLibPath = tmpControlInfo.lipiLib;
		m_currentVersion = tmpControlInfo.toolkitVersion;
		strProjectName = tmpControlInfo.projectName;
		strProfileName = tmpControlInfo.profileName;
		
		
		//Model Data file Header Information
		m_headerInfo[PROJNAME] = strProjectName;
		
		//Holds the value of Number of Shapes in string format
		string strNumShapes = "";
		
		string strProfileDirectory = m_lipiRootPath + PROJECTS_PATH_STRING +
			strProjectName + PROFILE_PATH_STRING;
		
		//Holds the path of the Preproc.dll
		
		//Holds the path of the Project.cfg
		string projectCFGPath = strProfileDirectory + PROJECT_CFG_STRING;
		
		// Config file
		
		m_activedtwCfgFilePath = m_lipiRootPath + PROJECTS_PATH_STRING +
			(tmpControlInfo.projectName) + PROFILE_PATH_STRING +
			(tmpControlInfo.profileName) + SEPARATOR +
			ACTIVEDTW + CONFIGFILEEXT;
		
		
		//Set the path for activedtw.mdt
		m_activedtwMDTFilePath = strProfileDirectory + strProfileName + SEPARATOR + ACTIVEDTW + DATFILEEXT;
		
		
		//To find whether the project was dynamic or not andto read read number of shapes from project.cfg
		int errorCode = m_shapeRecUtil.isProjectDynamic(projectCFGPath,
			m_numShapes, strNumShapes, m_projectTypeDynamic);
		
		if( errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
				"ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" <<endl;
			throw LTKException(errorCode);
		}
		
		//Set the NumShapes to the m_headerInfo
		m_headerInfo[NUMSHAPES] = strNumShapes;
		
		//Currently preproc cfg also present in ActiveDTW
		tmpControlInfo.cfgFileName = ACTIVEDTW;
		errorCode = initializePreprocessor(tmpControlInfo,&m_ptrPreproc);
		
		
		if( errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
				"ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" <<endl;
			throw LTKException(errorCode);
		}
		
		//Reading ActiveDTW configuration file
		errorCode = readClassifierConfig();
		
		
		if( errorCode != SUCCESS)
		{
			
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
				"ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" <<endl;
			throw LTKException(errorCode);
		}
		
		//Writing Feature extractor name and version into the header
		m_headerInfo[FE_NAME] = m_featureExtractorName;
		//FE version
		m_headerInfo[FE_VER] = SUPPORTED_MIN_VERSION; 
		
		//Writing mdt file open mode to the mdt header
		m_headerInfo[MDT_FOPEN_MODE] = m_MDTFileOpenMode;
		
		errorCode = initializeFeatureExtractorInstance(tmpControlInfo);
		
		
		if( errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
				"ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" <<endl;
			throw LTKException(errorCode);
		}
   }
   catch(LTKException e)
   {
	   deletePreprocessor();
	   m_prototypeShapes.clear();
	   
	   m_cachedShapeFeature.clear();
	   
	   //Unloading the feature Extractor instance
	   deleteFeatureExtractorInstance();
	   
	   delete m_OSUtilPtr;
	   throw e;
   }
   LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
	   "ActiveDTWShapeRecognizer::ActiveDTWShapeRecognizer()" << endl;
   
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: readClassifierConfig
* DESCRIPTION	: Reads the ActiveDTW.cfg and initializes the data members of the class
* ARGUMENTS		: none
* RETURNS		: SUCCESS   - If config file read successfully
*				  errorCode - If failure
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::readClassifierConfig()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::readClassifierConfig()" << endl;
    string tempStringVar = "";
    int tempIntegerVar = 0;
    float tempFloatVar = 0.0;
    LTKConfigFileReader *shapeRecognizerProperties = NULL;
    int errorCode = FAILURE;
	
    try
    {
        shapeRecognizerProperties = new LTKConfigFileReader(m_activedtwCfgFilePath);
    }
    catch(LTKException e)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)<< "Info: " <<
            "Config file not found, using default values of the parameters" <<
            "ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
		
        delete shapeRecognizerProperties;
		
		return FAILURE;
    }
	
    //Pre-processing sequence
    errorCode = shapeRecognizerProperties->getConfigValue(PREPROCSEQUENCE, m_preProcSeqn);
	
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO) << "Info: " <<
            "Using default value of prerocessing sequence: "<< m_preProcSeqn <<
            " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
		
        m_preProcSeqn = NN_DEF_PREPROC_SEQ;
    }
    else
    {
		m_headerInfo[PREPROC_SEQ] = m_preProcSeqn;
    }
	
	
    if((errorCode = mapPreprocFunctions()) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<" Error: " << errorCode << " " <<
            "ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
		
		delete shapeRecognizerProperties;
		
        LTKReturnError(errorCode);
    }
	
    //reading percent of eigen energy
    tempStringVar = "";
	
	errorCode = shapeRecognizerProperties->getConfigValue(RETAINPERCENTEIGENENERGY,
		tempStringVar);
    if(errorCode == SUCCESS )
    {
        if (LTKStringUtil::isFloat(tempStringVar))
        {
            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);
			
			if(tempFloatVar >= MIN_PERCENT_EIGEN_ENERGY && tempFloatVar <= MAX_PERCENT_EIGEN_ENERGY)
            {
                m_percentEigenEnergy = tempFloatVar;
				
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
					RETAINPERCENTEIGENENERGY " = " << m_percentEigenEnergy<< endl;
				
				
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                    "Error: " << ECONFIG_FILE_RANGE <<  " " << RETAINPERCENTEIGENENERGY
                    " is out of permitted range" <<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
				"Error: " << ECONFIG_FILE_RANGE <<  " " << RETAINPERCENTEIGENENERGY
				" is out of permitted range" <<
				" ActiveDTWShapeRecognizer::readClassifierConfig()" << endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            "Using default value for " << RETAINPERCENTEIGENENERGY << ": " <<
            m_percentEigenEnergy << " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
    }
	
    //reading method of prototype selection
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(PROTOTYPESELECTION,
		tempStringVar);
	
    if (errorCode == SUCCESS)
    {
        if( (LTKSTRCMP(tempStringVar.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0))
        {
            m_prototypeSelection = tempStringVar;
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
                PROTOTYPESELECTION << " = " << tempStringVar <<
                "ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                "Error: " << ECONFIG_FILE_RANGE << " " <<
                PROTOTYPESELECTION << " : " << tempStringVar
                <<  " method is not supported"  <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << PROTOTYPESELECTION << " : " << m_prototypeSelection <<
            " ActiveDTwShapeRecognizer::readClassifierConfig()"<<endl;
    }
	
    //reading prototype reduction factor 
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(PROTOREDFACTOR,
		tempStringVar);
	
    string tempStringVar1 = "";
    int errorCode1 = shapeRecognizerProperties->getConfigValue(NUMCLUSTERS,
		tempStringVar1);
	
    //prototype reduction factor
    if(errorCode1 == SUCCESS && errorCode == SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Error: " << ECONFIG_FILE_RANGE
            << " Cannot use both config parameters " <<
            PROTOREDFACTOR << " and " << NUMCLUSTERS << " at the same time "<<
            " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
		
        LTKReturnError(ECONFIG_FILE_RANGE);
    }
	
    if(tempStringVar != "")
    {
        if(LTKSTRCMP(tempStringVar.c_str(),  PROTO_RED_FACTOR_AUTOMATIC)==0)
        {
            //DEFINE MACRO DEF_PROTO_RED_FACTOR
			m_prototypeReductionFactor = -1;
        }
        else if(LTKSTRCMP(tempStringVar.c_str(), PROTO_RED_FACTOR_NONE)==0)
        {
            m_prototypeReductionFactor = 0;
        }
        else if(LTKSTRCMP(tempStringVar.c_str(), PROTO_RED_FACTOR_COMPLETE) == 0)
        {
            m_prototypeReductionFactor = 100;
        }
        else
        {
            if ( LTKStringUtil::isInteger(tempStringVar) )
            {
                tempIntegerVar = atoi((tempStringVar).c_str());
                if(tempIntegerVar >= 0 && tempIntegerVar <=100)
                {
                    m_prototypeReductionFactor = tempIntegerVar;
					
                    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
                        << PROTOREDFACTOR << " is  =" << tempStringVar<<endl;
                }
                else
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                        "Error: " << ECONFIG_FILE_RANGE <<
                        PROTOREDFACTOR << " is out of permitted range " <<
                        " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                    "Error: " << ECONFIG_FILE_RANGE <<
                    PROTOREDFACTOR << " is out of permitted range"<<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
			
        }
    }
    else if(tempStringVar1 != "")
    {
        if(LTKSTRCMP(tempStringVar1.c_str(),  PROTO_RED_FACTOR_AUTOMATIC) == 0)
        {
            m_prototypeReductionFactor = -1;
        }
        else
        {
            if ( LTKStringUtil::isInteger(tempStringVar1) )
            {
                tempIntegerVar = atoi((tempStringVar1).c_str());
                if(tempIntegerVar > 0)
                {
                    m_numClusters = tempIntegerVar;
					
                    // m_numClusters is used in this case
                    m_prototypeReductionFactor = NN_NUM_CLUST_INITIAL;
                    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                        NUMCLUSTERS << " is  = " << tempStringVar << endl;
                }
                else
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                        "Error: " << ECONFIG_FILE_RANGE <<
                        NUMCLUSTERS << " is out of permitted range "<<
                        " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    " Error: " << ECONFIG_FILE_RANGE <<
                    NUMCLUSTERS << " is out of permitted range"<<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Assuming default value of " NUMCLUSTERS << " : " << m_numClusters << endl;
    }
	
    //reading adaptive kNN
    tempStringVar = "";
    shapeRecognizerProperties->getConfigValue(ADAPTIVE_kNN, tempStringVar);
    if(LTKSTRCMP(tempStringVar.c_str(), "true") ==0)
    {
        m_adaptivekNN = true;
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
            <<"Confidence computation method: " << ADAPTIVE_kNN << endl;
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << ADAPTIVE_kNN << " : " <<
            m_adaptivekNN << endl;
    }
	
    //reading nearest neighbors
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEARESTNEIGHBORS,
		tempStringVar);
	
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
            tempIntegerVar = atoi((tempStringVar).c_str());
			
            //Valid values of nearest neighbors: 1 or greater than MIN_NEARESTNEIGHBORS
            if(tempIntegerVar > 0)
            {
                // If the value of NearestNeighbors = 1, ActiveDTW recognizer is used
                if(tempIntegerVar == 1)
                {
                    m_adaptivekNN = false;
                }
				
                // If AdaptivekNN is set to false, simply assign the value to m_nearestNeighbors
                // If AdaptivekNN is set, NearestNeighbors should be greater than than the
                // minimum no.of nearest neighbors allowed (MIN_NEARESTNEIGHBORS defined as macro)
                if(!m_adaptivekNN || (m_adaptivekNN && tempIntegerVar >= MIN_NEARESTNEIGHBORS))
                {
                    m_nearestNeighbors = tempIntegerVar;
                    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                        NEARESTNEIGHBORS << " = " <<m_nearestNeighbors<<endl;
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEARESTNEIGHBORS <<
                    " is out of permitted range" <<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEARESTNEIGHBORS <<
                " is out of permitted range" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Debug: " << "Using default value for " << NEARESTNEIGHBORS <<
            " : " << m_nearestNeighbors << endl;
    }
	
    //reading reject threshold
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(REJECT_THRESHOLD,
		tempStringVar);
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isFloat(tempStringVar) )
        {
            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);
			
            if(tempFloatVar  > 0 && tempFloatVar < 1)
            {
                m_rejectThreshold = tempFloatVar;
				
                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    REJECT_THRESHOLD << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << REJECT_THRESHOLD <<
                    " should be in the range (0-1)" <<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << REJECT_THRESHOLD <<
                " should be in the range (0-1)" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
			LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << REJECT_THRESHOLD <<
            " : " << m_rejectThreshold << endl;
    }
	
    //reading min cluster Size
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(MINCLUSTERSIZE,
		tempStringVar);
	
    if(errorCode == SUCCESS)
    {
		if(LTKStringUtil::isInteger(tempStringVar))
		{
			tempIntegerVar = atoi((tempStringVar).c_str());
			
			if(tempIntegerVar > 1)
			{
				m_minClusterSize = tempIntegerVar;
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    MINCLUSTERSIZE << " = " <<m_minClusterSize<<endl;
			}
			else
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
					"Error: "<< ECONFIG_FILE_RANGE << m_minClusterSize <<
					" is out of permitted range" <<
					" ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				LTKReturnError(ECONFIG_FILE_RANGE);
			}
		}
		else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: "<< ECONFIG_FILE_RANGE << MINCLUSTERSIZE <<
                " is out of permitted range" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << MINCLUSTERSIZE << " : " << m_minClusterSize << endl;
    }
	
    //reading eigen spread value
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(EIGENSPREADVALUE,
		tempStringVar);
	
    
	
    if(errorCode == SUCCESS)
    {
		if(LTKStringUtil::isInteger(tempStringVar))
		{
			tempIntegerVar = atoi((tempStringVar).c_str());
			
			if(tempIntegerVar > 0)
			{
				m_eigenSpreadValue = tempIntegerVar;
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    EIGENSPREADVALUE << " = " <<m_eigenSpreadValue<<endl;
			}
			else
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
					"Error: "<< ECONFIG_FILE_RANGE << EIGENSPREADVALUE <<
					" is out of permitted range" <<
					" ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				LTKReturnError(ECONFIG_FILE_RANGE);
			}
		}
		else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: "<< ECONFIG_FILE_RANGE << EIGENSPREADVALUE <<
                " is out of permitted range" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << EIGENSPREADVALUE << " : " << m_eigenSpreadValue << endl;
    }
	
    //reading use singleton
    tempStringVar = "";
    shapeRecognizerProperties->getConfigValue(USESINGLETON, tempStringVar);
    if(LTKSTRCMP(tempStringVar.c_str(), "false") ==0)
    {
        m_useSingleton = false;
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
            <<"Use Singleton: " << USESINGLETON << endl;
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << USESINGLETON << " : " <<
            m_useSingleton << endl;
    }
	
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(DTWBANDING,
		tempStringVar);
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isFloat(tempStringVar) )
        {
            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);
			
            if(tempFloatVar  > 0 && tempFloatVar <= 1)
            {
                m_dtwBanding = tempFloatVar;
                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    DTWBANDING << " = " <<m_dtwBanding<<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: "<< ECONFIG_FILE_RANGE << DTWBANDING <<
                    " is out of permitted range" <<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: "<< ECONFIG_FILE_RANGE <<
                " DTWBANDING is out of permitted range" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << DTWBANDING << " : " << m_dtwBanding << endl;
    }
	
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(DTWEUCLIDEANFILTER,
		tempStringVar);
	if(errorCode == SUCCESS)
	{
		if(LTKSTRCMP(tempStringVar.c_str(), DTW_EU_FILTER_ALL) == 0)
		{
			m_dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;
		}
		
		else
		{
			if ( LTKStringUtil::isInteger(tempStringVar) )
			{
				tempIntegerVar = atoi((tempStringVar).c_str());
				
				if(tempIntegerVar > 0 && tempIntegerVar <= 100)
				{
					if(tempIntegerVar == 100 )
						m_dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;
					else
						m_dtwEuclideanFilter = tempIntegerVar;	
					
					
					
					LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
						DTWEUCLIDEANFILTER << " is  = "<<
						m_dtwEuclideanFilter<<endl;
				}
				else
				{
					
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
						"Error: " << ECONFIG_FILE_RANGE <<
						DTWEUCLIDEANFILTER << " is out of permitted range " <<
						" ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
					
					delete shapeRecognizerProperties;
					
					LTKReturnError(ECONFIG_FILE_RANGE);
				}
				
			}
			else
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
					"Error: " << ECONFIG_FILE_RANGE << DTWEUCLIDEANFILTER <<
					" is out of permitted range"<<
					" ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				
				LTKReturnError(ECONFIG_FILE_RANGE);
			}
		}
		
	}
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << DTWEUCLIDEANFILTER <<
            " : " << m_dtwEuclideanFilter << endl;
    }
	
	
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(FEATUREEXTRACTOR,
		tempStringVar);
    if(errorCode == SUCCESS)
    {
        m_featureExtractorName = tempStringVar;
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            FEATUREEXTRACTOR << " = "<<tempStringVar<<endl;
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << FEATUREEXTRACTOR << " : " <<
            m_featureExtractorName << endl;
    }
	
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(MDTFILEUPDATEFREQ,
		tempStringVar);
	
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
            m_MDTUpdateFreq = atoi(tempStringVar.c_str());
            if(m_MDTUpdateFreq <= 0)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << MDTFILEUPDATEFREQ <<
                    " should be zero or a positive integer" <<
                    " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
				
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << MDTFILEUPDATEFREQ <<
                " should be zero or a positive integer" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << MDT_UPDATE_FREQUENCY <<
            " : " << m_MDTUpdateFreq << endl;
    }
	
    
	
    //reading mdt file open mode 
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(MDT_FILE_OPEN_MODE,
		tempStringVar);
	
    if(errorCode == SUCCESS)
    {
        if ( tempStringVar == NN_MDT_OPEN_MODE_ASCII ||
			tempStringVar == NN_MDT_OPEN_MODE_BINARY )
        {
            m_MDTFileOpenMode = tempStringVar;
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << MDT_FILE_OPEN_MODE <<
                " should be ascii or binary" <<
                " ActiveDTWShapeRecognizer::readClassifierConfig()"<<endl;
			
            delete shapeRecognizerProperties;
			
            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << MDT_FILE_OPEN_MODE <<
            " : " << m_MDTFileOpenMode << endl;
    }
	
    
	
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(SIZETHRESHOLD,
		tempStringVar);
	if(errorCode == SUCCESS)
	{
		m_headerInfo[DOT_SIZE_THRES] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(ASPECTRATIOTHRESHOLD,
		tempStringVar);
   	if(errorCode == SUCCESS)
	{
		m_headerInfo[ASP_RATIO_THRES] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(DOTTHRESHOLD,
		tempStringVar);
	if(errorCode == SUCCESS)
	{
		m_headerInfo[DOT_THRES] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(PRESERVERELATIVEYPOSITION,
		tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		m_headerInfo[PRESER_REL_Y_POS] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(PRESERVEASPECTRATIO,
		tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		m_headerInfo[PRESER_ASP_RATIO] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(SIZETHRESHOLD,
		tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		m_headerInfo[NORM_LN_WID_THRES] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(RESAMPLINGMETHOD,
		tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		m_headerInfo[RESAMP_POINT_ALLOC] = tempStringVar;
	}
	
	tempStringVar = "";
	errorCode = shapeRecognizerProperties->getConfigValue(SMOOTHFILTERLENGTH,
		tempStringVar);
	
	if(errorCode == SUCCESS)
	{
		m_headerInfo[SMOOTH_WIND_SIZE] = tempStringVar;
	}
	
	tempStringVar = "";
	LTKStringUtil::convertIntegerToString(m_ptrPreproc->getTraceDimension(), 
		tempStringVar);
	
	
	m_headerInfo[TRACE_DIM] = tempStringVar;
	
	delete shapeRecognizerProperties;
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
		"ActiveDTWShapeRecognizer::readClassifierConfig()" << endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME          		: mapPreprocFunctions
* DESCRIPTION   	: Maps the module name and its function names in the preprocessing
sequence.
* ARGUMENTS     	: none
* RETURNS       	: SUCCESS on successful,
*				  errorNumbers on Failure.
* NOTES         	:
* CHANGE HISTROY
* Author            Date                Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::mapPreprocFunctions()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::mapPreprocFunctions()" << endl;
	
    stringStringMap preProcSequence;
	
    stringStringPair tmpPair;
	
    stringVector moduleFuncNames;
    stringVector modFuncs;
    stringVector funcNameTokens;
	
    string module = "", funName = "", sequence = "";
    string::size_type indx;
	
    LTKTraceGroup local_inTraceGroup;
	
    LTKStringUtil::tokenizeString(m_preProcSeqn,  DELEMITER_SEQUENCE,  funcNameTokens);
	
    int numFunctions = funcNameTokens.size();
	
    if(numFunctions == 0)
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
            "Wrong preprocessor sequence in cfg file : " + m_preProcSeqn <<
            " ActiveDTWShapeRecognizer::mapPreprocFunctions()"<<endl;
		
        LTKReturnError(EINVALID_PREPROC_SEQUENCE);
    }
	
    for (indx = 0; indx < numFunctions ; indx++)
    {
        moduleFuncNames.push_back(funcNameTokens[indx]);
    }
	
    int numModuleFunctions = moduleFuncNames.size();
	
    for(indx=0; indx < numModuleFunctions ; indx++)
    {
        sequence = moduleFuncNames[indx];
		
        LTKStringUtil::tokenizeString(sequence,  DELEMITER_FUNC,  modFuncs);
		
        if(modFuncs.size() >= 2)
        {
            module = modFuncs.at(0);
			
            funName =  modFuncs.at(1);
			
            if(!module.compare("CommonPreProc"))
            {
                FN_PTR_PREPROCESSOR pPreprocFunc = NULL;
                pPreprocFunc = m_ptrPreproc->getPreprocptr(funName);
                if(pPreprocFunc!= NULL)
                {
                    tmpPair.first = module;
                    tmpPair.second = funName;
                    m_preprocSequence.push_back(tmpPair);
                }
                else
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                        "Wrong preprocessor sequence entry in cfg file : " <<funName<<
                        " ActiveDTWShapeRecognizer::mapPreprocFunctions()"<<endl;
                    LTKReturnError(EINVALID_PREPROC_SEQUENCE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                    "Wrong preprocessor sequence entry in cfg file  : " << module<<
                    " ActiveDTWShapeRecognizer::mapPreprocFunctions()"<<endl;
                LTKReturnError(EINVALID_PREPROC_SEQUENCE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                "Wrong preprocessor sequence entry in cfg file  : "<<module<<
                " ActiveDTWShapeRecognizer::mapPreprocFunctions()"<<endl;
            LTKReturnError(EINVALID_PREPROC_SEQUENCE);
        }
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::mapPreprocFunctions()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: ~ActiveDTWShapeRecognizer
* DESCRIPTION	: destructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()" << endl;
	
	int returnStatus = SUCCESS;
	
	if(LTKAdapt::getInstance(this))
		deleteAdaptInstance();
	
	if(m_prototypeSetModifyCount >0)
	{
		m_prototypeSetModifyCount = m_MDTUpdateFreq-1;
		
		returnStatus = writePrototypeShapesToMDTFile();
		if(returnStatus != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
				" ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()" << endl;
			throw LTKException(returnStatus);
		}
	}
	
	m_neighborInfoVec.clear();
    
    returnStatus = deletePreprocessor();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
            " ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }
	
    m_prototypeShapes.clear();
    m_cachedShapeFeature.clear();
    
    //Unloading the feature Extractor instance
    returnStatus = deleteFeatureExtractorInstance();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " <<  returnStatus << " " <<
            " ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }
	
    delete m_OSUtilPtr;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::~ActiveDTWShapeRecognizer()" << endl;
}


/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: train
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::train(const string& trainingInputFilePath,
									const string& mdtHeaderFilePath,
									const string &comment,const string &dataset,
									const string &trainFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::train()" << endl;
	
    
	
    int returnStatus = SUCCESS;
	
	
    if(comment.empty() != true)
    {
        m_headerInfo[COMMENT] = comment;
    }
	
    if(dataset.empty() != true)
    {
        m_headerInfo[DATASET] = dataset;
    }
	
    
    if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0)
    {
        returnStatus = trainClustering(trainingInputFilePath,
			mdtHeaderFilePath,
			trainFileType);
		
        if(returnStatus != SUCCESS)
        {
            LTKReturnError(returnStatus);
        }
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::train()" << endl;
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: trainClustering
* DESCRIPTION	: This function is the train method using Clustering prototype
*				  selection technique.
* ARGUMENTS		:
* RETURNS		: SUCCESS : if training done successfully
*				  errorCode : if traininhas some errors
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::trainClustering(const string& trainingInputFilePath,
											  const string &mdtHeaderFilePath,
											  const string& inFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::trainClustering()" << endl;
	
	
    //Time at the beginning of Train Clustering
    m_OSUtilPtr->recordStartTime();
    
    int returnStatus = SUCCESS;
	
    if(LTKSTRCMP(inFileType.c_str(), INK_FILE) == 0)
    {
        //If the Input file is UNIPEN Ink file
        returnStatus = trainFromListFile(trainingInputFilePath);
        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
                getErrorMessage(returnStatus) <<
                " ActiveDTWShapeRecognizer::trainClustering()" << endl;
            LTKReturnError(returnStatus);
        }
    }
    
	
    //Updating the Header Information
    updateHeaderWithAlgoInfo();
	
    //Adding header information	and checksum generation
    LTKCheckSumGenerate cheSumGen;
	
    returnStatus = cheSumGen.addHeaderInfo(mdtHeaderFilePath,
		m_activedtwMDTFilePath,
		m_headerInfo);
	
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
            getErrorMessage(returnStatus) <<
            " ActiveDTWShapeRecognizer::trainClustering()" << endl;
		
        LTKReturnError(returnStatus);
    }
	
    //Time at the end of Train Clustering
    m_OSUtilPtr->recordEndTime();
	
    string timeTaken = "";
    m_OSUtilPtr->diffTime(timeTaken);
	
    cout << "Time Taken  = " << timeTaken << endl;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::trainClustering()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: appendShapeModelToMDTFile
* DESCRIPTION	        : This method is called after performing clustering on each class 
It writes the class information to the activedtw.mdt file 
* ARGUMENTS		: INPUT
shapeModel struct ActiveDTWShapeModel (class training data)
mdtFileHandle ofstream (mdt File handle)

 * RETURNS		: integer Holds error value if occurs
 *						  Holds SUCCESS if no erros
*************************************************************************************/
int ActiveDTWShapeRecognizer::appendShapeModelToMDTFile(const ActiveDTWShapeModel& shapeModel,ofstream& mdtFileHandle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::appendShapeModelToMDTFile()" << endl;
	
	
	//used to temporarily store the size of a vector
	int vecSize;
	if(!mdtFileHandle)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_FILE_HANDLE << " " <<
			"Invalid file handle for MDT file"<<
			" ActiveDTWShapeRecognizer::appendShapeModelToMDTFile()" << endl;
		LTKReturnError(EINVALID_FILE_HANDLE);
	}
	
	string strFeature;
	
	vector<ActiveDTWClusterModel> clusterModelVector = shapeModel.getClusterModelVector();
	vector<ActiveDTWClusterModel>::iterator iStart = clusterModelVector.begin();
	vector<ActiveDTWClusterModel>::iterator iEnd = clusterModelVector.end();
	double2DVector eigenVectors;
	doubleVector eigenValues;
	doubleVector clusterMean;
	shapeMatrix singletonVector = shapeModel.getSingletonVector();
	ActiveDTWClusterModel clusterModel;
	
	/**APPENDING CLASS INFORMATION**/
	//APPENDING CLASSID NUMCLUSTERS NUMSINGLETONS
	if(m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII)
	{
		mdtFileHandle<<shapeModel.getShapeId()<<" "<<clusterModelVector.size()<<" "<<singletonVector.size()<<endl;
	}
	else
	{	
		int clusterSize = clusterModelVector.size();
		int singletonSize = singletonVector.size();	
		int shapeId = shapeModel.getShapeId();
		int numFeatures;
		int featureDimension;
		int clusterMeanDimension;
		mdtFileHandle.write((char*) &shapeId,sizeof(int));
		mdtFileHandle.write((char*) &clusterSize,sizeof(int));
		mdtFileHandle.write((char*) &singletonSize,sizeof(int));
		
		if(clusterSize != 0)
		{
			clusterMean = clusterModelVector[0].getClusterMean();
			clusterMeanDimension = clusterMean.size();
			mdtFileHandle.write((char*) &clusterMeanDimension,sizeof(int));
		}
		else
		{
			clusterMeanDimension = 0;
			mdtFileHandle.write((char*) &clusterMeanDimension,sizeof(int));
		}
		
		//writing number of features and feature dimension
		if(singletonSize != 0)
		{
			numFeatures = singletonVector[0].size();
			mdtFileHandle.write((char*) &numFeatures,sizeof(int));
			featureDimension = singletonVector[0][0]->getFeatureDimension();
			mdtFileHandle.write((char*) &featureDimension,sizeof(int));
		}
		else
		{
			numFeatures = 0;
			mdtFileHandle.write((char*) &numFeatures,sizeof(int));
			featureDimension = 0;
			mdtFileHandle.write((char*) &featureDimension,sizeof(int));
		}
		
	}
	
	/**APPENDING CLUSTER DATA**/
	//iterating through the cluster models
	
	for(;iStart != iEnd; ++iStart)
	{
		
		clusterModel = *iStart;
		
		
		//appending number of clusters in each sample 
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			mdtFileHandle<<clusterModel.getNumSamples()<<" ";
		}
		else
		{
			int numSamples = clusterModel.getNumSamples();
			mdtFileHandle.write((char*) &numSamples,sizeof(int));
		}
		
		
		eigenValues = clusterModel.getEigenValues();
		vecSize = eigenValues.size();
		
		/**WRITING EIGEN VALUES**/
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			for(int i = 0; i < vecSize; i++)
			{
				mdtFileHandle<<eigenValues[i];
				if(i != (vecSize - 1))
				{
					mdtFileHandle<<",";
				}
			}
			mdtFileHandle<<FEATURE_EXTRACTOR_DELIMITER;
		}
		else
		{
			//writing number of eigen values
			mdtFileHandle.write((char*) &vecSize,sizeof(int));
			
			//writing eigenValues
			for(int i = 0; i < vecSize; i++)
			{
				mdtFileHandle.write((char*) &(eigenValues[i]),sizeof(double));
			}
		}
		
		/**WRITING EIGEN VECTORS**/
		
		eigenVectors = clusterModel.getEigenVectors();
		vecSize = eigenVectors[0].size();
		int eigVecSize = eigenVectors.size();
		
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			for(int i = 0; i < eigVecSize; i++)
			{
				for(int j = 0; j < vecSize; j++)
				{
					mdtFileHandle<<eigenVectors[i][j];
					if(j != (vecSize - 1))
					{
						mdtFileHandle<<",";
					}
				}
				mdtFileHandle<<FEATURE_EXTRACTOR_DELIMITER;
			}
		}
		else
		{
			for(int i = 0; i < eigVecSize; i++)
			{
				for(int j = 0; j < vecSize; j++)
				{
					mdtFileHandle.write((char*) &(eigenVectors[i][j]),sizeof(double));
				}
			}
		}
		
		/**APPENDING CLUSTER MEAN**/
		
		clusterMean = clusterModel.getClusterMean();
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			for(int i = 0; i < vecSize; i++)
			{
				mdtFileHandle<<clusterMean[i];
				if(i != (vecSize - 1))
				{
					mdtFileHandle<<",";
				}
				
			}
			mdtFileHandle<<FEATURE_EXTRACTOR_DELIMITER<<endl;
		}
		else
		{
			for(int i = 0; i < vecSize; i++)
			{
				mdtFileHandle.write((char*) &(clusterMean[i]),sizeof(double));
			}
		}
		
		eigenVectors.clear();
		eigenValues.clear();
		clusterMean.clear();
		
	}
	clusterModelVector.clear();
	
	/**WRITING SINGLETON VECTORS**/
	shapeMatrix::iterator iterStart = singletonVector.begin();
	shapeMatrix::iterator iterEnd = singletonVector.end();
	shapeFeature singleton;
	
	for(; iterStart != iterEnd; ++iterStart )
	{
		
		singleton = *iterStart;
		
		vecSize = singleton.size();
		
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = singleton.begin();
			vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = singleton.end();
			
			for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
			{
				(*shapeFeatureIter)->toString(strFeature);
				mdtFileHandle << strFeature << FEATURE_EXTRACTOR_DELIMITER;
			}
			mdtFileHandle<<endl;
			
		}
		else
		{
			
			//converting the singleton vector to float and writing it
			floatVector floatFeatureVector;
			int errorCode = m_shapeRecUtil.shapeFeatureVectorToFloatVector(singleton,
				floatFeatureVector);
			
			if (errorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<errorCode<<
					" ActiveDTWShapeRecognizer::appendShapeModelToMDTFile" << endl;
				LTKReturnError(errorCode);
			}
			
			vecSize = floatFeatureVector.size();
			
			
			for (int i=0; i< vecSize; i++)
			{
				float floatValue = floatFeatureVector[i];
				mdtFileHandle.write((char *)(&floatValue), sizeof(float));
			}
		}
	}
	
	singletonVector.clear();
	
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::appendShapeModelToMDTFile()" << endl;
	
	return SUCCESS;
	
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: preprocess
* DESCRIPTION	: calls the required pre-processing functions from the LTKPreprocessor library
* ARGUMENTS		: inTraceGroup - reference to the input trace group
*				  outPreprocessedTraceGroup - pre-processed inTraceGroup
* RETURNS		: SUCCESS on successful pre-processing operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::preprocess (const LTKTraceGroup& inTraceGroup,
										  LTKTraceGroup& outPreprocessedTraceGroup)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::preprocess()" << endl;
	
    int indx = 0;
	int errorCode = -1;

    string module = "";
    string funName = "" ;
	
    LTKTraceGroup local_inTraceGroup;
	
    local_inTraceGroup = inTraceGroup;
	
    if(m_preprocSequence.size() != 0)
    {
        while(indx < m_preprocSequence.size())
        {
            module = m_preprocSequence.at(indx).first;
            funName =  m_preprocSequence.at(indx).second;
			
            FN_PTR_PREPROCESSOR pPreprocFunc = NULL;
            pPreprocFunc = m_ptrPreproc->getPreprocptr(funName);
			
            if(pPreprocFunc!= NULL)
            {
                outPreprocessedTraceGroup.emptyAllTraces();
				
				
                if((errorCode = (m_ptrPreproc->*(pPreprocFunc))
					(local_inTraceGroup,outPreprocessedTraceGroup)) != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Error: "<<  errorCode << " " <<
                        " ActiveDTWShapeRecognizer::preprocess()" << endl;
                    LTKReturnError(errorCode);
                }
                local_inTraceGroup = outPreprocessedTraceGroup;
            }
            indx++;
        }
    }
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting ActiveDTWShapeRecognizer::preprocess()"<<endl;
    return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: computerDTWDistanceClusteringWrapper
* DESCRIPTION	: Wrapper function to the function whichcomputes DTW distance between
two characters
* ARGUMENTS		: train character, test character
* RETURNS		: DTWDistance
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::computeDTWDistance(
												 const LTKShapeSample& inFirstShapeSampleFeatures,
												 const LTKShapeSample& inSecondShapeSampleFeatures,
												 float& outDTWDistance)
												 
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
	
    const vector<LTKShapeFeaturePtr>& firstFeatureVec = inFirstShapeSampleFeatures.getFeatureVector();
    const vector<LTKShapeFeaturePtr>& secondFeatureVec = inSecondShapeSampleFeatures.getFeatureVector();
	
    int errorCode = m_dtwObj.computeDTW(firstFeatureVec, secondFeatureVec, getDistance,outDTWDistance,
		m_dtwBanding, FLT_MAX, FLT_MAX);
	
    
    
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "DTWDistance: " <<
        outDTWDistance << endl;
	
    if (errorCode != SUCCESS )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Error: "<<
            getErrorMessage(errorCode) <<
            " ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
        LTKReturnError(errorCode);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: computeDTWDistance
* DESCRIPTION	        : This method computes the dtw distance between two shape features
* ARGUMENTS		: INPUT
inFirstFeatureVector  vector<LTKShapeFeaturePtr> 
inSecondFeatureVector vector<LTKShapeFeaturePtr> 
: OUTPUT
outDTWDistance float
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::computeDTWDistance(
												 const vector<LTKShapeFeaturePtr>& inFirstFeatureVector,
												 const vector<LTKShapeFeaturePtr>& inSecondFeatureVector, 
												 float& outDTWDistance)	
												 
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
	
    int errorCode = m_dtwObj.computeDTW(inFirstFeatureVector, inSecondFeatureVector, getDistance,outDTWDistance,
		m_dtwBanding, FLT_MAX, FLT_MAX);
	
    
    
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "DTWDistance: " <<
        outDTWDistance << endl;
	
    if (errorCode != SUCCESS )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Error: "<<
            getErrorMessage(errorCode) <<
            " ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
        LTKReturnError(errorCode);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::computeDTWDistance()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: computeEuclideanDistance
* DESCRIPTION	        : This computes the euclideanDistance between two shapeFeatures
* ARGUMENTS		: INPUT
inFirstFeature shapeFeature
inSecondFeature shapeFeature
:OUTPUT
outEuclideanDistance floats
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::computeEuclideanDistance(
													   const shapeFeature& inFirstFeature,
													   const shapeFeature& inSecondFeature,
													   float& outEuclideanDistance)
{
    int firstFeatureVectorSize = inFirstFeature.size();
    int secondFeatureVectorSize = inSecondFeature.size();
	
    if(firstFeatureVectorSize != secondFeatureVectorSize)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EUNEQUAL_LENGTH_VECTORS << " " <<
            getErrorMessage(EUNEQUAL_LENGTH_VECTORS) <<
            " ActiveDTWShapeRecognizer::computeEuclideanDistance()" << endl;
		
        LTKReturnError(EUNEQUAL_LENGTH_VECTORS);
    }
	
    for(int i = 0; i < firstFeatureVectorSize; ++i)
    {
        float tempDistance = 0.0;
        getDistance(inFirstFeature[i],
			inSecondFeature[i],
			tempDistance);
		
        outEuclideanDistance += tempDistance;
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::computeEuclideanDistance()" << endl;
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: loadModelData
* DESCRIPTION	: loads the reference model file into memory
* ARGUMENTS		:
* RETURNS		: SUCCESS on successful loading of the reference model file
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::loadModelData()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::loadModelData()" << endl;
	
    int errorCode = -1;
	
    int numofShapes = 0;
	
    // variable for shape Id
    int classId = -1;
    int i = 0;
	
    //Algorithm version
    string algoVersionReadFromMDT = "";
	
    stringStringMap headerSequence;
    LTKCheckSumGenerate cheSumGen;
	
    if(errorCode = cheSumGen.readMDTHeader(m_activedtwMDTFilePath,headerSequence))
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(errorCode);
    }
	
   	// printing the headerseqn
	stringStringMap::const_iterator iter = headerSequence.begin();
	stringStringMap::const_iterator endIter = headerSequence.end();
	
	for(; iter != endIter; iter++)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Debug: header seqn"<<
            iter->first << " : " <<
            iter->second << endl;
	}
	
    string featureExtractor = headerSequence[FE_NAME];
	
    if(LTKSTRCMP(m_featureExtractorName.c_str(), featureExtractor.c_str()) != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of FeatureExtractor parameter in config file ("<<
            m_featureExtractorName<<") does not match with the value in MDT file ("<<
            featureExtractor<<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    string feVersion = headerSequence[FE_VER];
	
    // comparing the mdt open mode read from cfg file with value in the mdt header
    string mdtOpenMode = headerSequence[MDT_FOPEN_MODE];
	
    if (LTKSTRCMP(m_MDTFileOpenMode.c_str(), mdtOpenMode.c_str()) != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of ActiveDTWMDTFileOpenMode parameter in config file ("<<
            m_MDTFileOpenMode <<") does not match with the value in MDT file ("<<
            mdtOpenMode<<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    // validating preproc parameters
    int iErrorCode = validatePreprocParameters(headerSequence);
    if (iErrorCode != SUCCESS )
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Values of ActiveMDTMDTFileOpenMode parameter in config file does not match with "
            <<"the values in MDT file " << "ActiveDTWShapeRecognizer::loadModelData()" << endl;
		
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    // Version comparison START
    algoVersionReadFromMDT = headerSequence[RECVERSION].c_str();
	
    LTKVersionCompatibilityCheck verTempObj;
    string supportedMinVersion(SUPPORTED_MIN_VERSION);
    string currentVersionStr(m_currentVersion);
	
    bool compatibilityResults = verTempObj.checkCompatibility(supportedMinVersion,
		currentVersionStr,
		algoVersionReadFromMDT);
	
    if(compatibilityResults == false)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINCOMPATIBLE_VERSION << " " <<
            " Incompatible version"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(EINCOMPATIBLE_VERSION);
    }
	
    // Version comparison END
	
    //Input Stream for Model Data file
    ifstream mdtFileHandle;
	
    if (m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_activedtwMDTFilePath.c_str(), ios::in);
    }
    else
    {
        mdtFileHandle.open(m_activedtwMDTFilePath.c_str(), ios::in | ios::binary);
    }
	
    //If error while opening, return an error
    if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            " Unable to open model data file : " <<m_activedtwMDTFilePath<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(EMODEL_DATA_FILE_OPEN);
    }
	
    mdtFileHandle.seekg(atoi(headerSequence[HEADERLEN].c_str()),ios::beg);
	
    //Read the number of shapes
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
		mdtFileHandle >> numofShapes;
    }
    else
    {
		mdtFileHandle.read((char*) &numofShapes,
			atoi(headerSequence[SIZEOFSHORTINT].c_str()));
    }
	
    if(!m_projectTypeDynamic && m_numShapes != numofShapes)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
            " Value of NumShapes parameter in config file ("<<m_numShapes<<
            ") does not match with the value in MDT file ("<<numofShapes<<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    if(m_projectTypeDynamic)
    {
        m_numShapes = numofShapes;
    }
	
    stringVector tokens;
	
    stringVector subTokens;
	
    string strFeatureVector = "";
	
    //number of samples in each cluster
    int numSamples;
	
    //keeps count of number of clusters
    // and singletons while reading from
    //mdt file
    int tempCount;
	
    //number of clusters in a class
    int numClusters;
	
    //number of singletons in a class
    int numSingletons;
	
    //dimension of cluster mean
    int clusterMeanDimension;
	
    //number of features in a feature vector
    int numFeatures;
	
    //dimension of the featureVector
    int featureDimension;
	
    
    shapeMatrix singletonVector;
    shapeFeature singleton;
    doubleVector eigenValues;
    double2DVector eigenVectors;
    doubleVector clusterMean;
    ActiveDTWClusterModel clusterModel;
    ActiveDTWShapeModel shapeModel;
    vector<ActiveDTWClusterModel> clusterModelVector;
    doubleVector tempVector;
	
    int floatSize = atoi(headerSequence[SIZEOFFLOAT].c_str());
	
    int intSize = atoi(headerSequence[SIZEOFINT].c_str());
	
    int doubleSize = sizeof(double);
	
    //Each pass over the loop reads data corresponding to one class
    //includes reading all the cluster data
    //all singleton vectors
    
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
		mdtFileHandle >> classId;
    }
    else
    {
		mdtFileHandle.read((char*) &classId, intSize);
		
    }
    
    while(!mdtFileHandle.eof())
    {
		
		/**READING CLASS INFORMATION**/
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			mdtFileHandle >> numClusters;
			
			mdtFileHandle >> numSingletons;
		}
		else
		{
			//reading number of clusters, singletons, clusterMeanDimension,
			//number of Features, and featureDimension
			mdtFileHandle.read((char*)  &numClusters,intSize);
			
			mdtFileHandle.read((char*) &numSingletons,intSize);
			
			mdtFileHandle.read((char*) &clusterMeanDimension,intSize);
			
			mdtFileHandle.read((char*) &numFeatures,intSize);
			
			
			mdtFileHandle.read((char*) &featureDimension,intSize);
			
			
			
		}
		
		tempCount = 0;
		
		/**READING CLUSTER DATA**/
		
		for(int clustersCount = 0 ; clustersCount < numClusters; clustersCount++)
		{
			//reading number of samples in a cluster
			if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
			{
				mdtFileHandle >> numSamples;
			}
			else
			{
				mdtFileHandle.read((char*) &numSamples,intSize);
			}
			
			iErrorCode = clusterModel.setNumSamples(numSamples);
			if(iErrorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< iErrorCode << " " <<
					" ActiveDTWShapeRecognizer::loadModelData()" << endl;
				LTKReturnError(iErrorCode);
			}
			
			
			if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
			{
				strFeatureVector = "";
				mdtFileHandle >> strFeatureVector;
				
				LTKStringUtil::tokenizeString(strFeatureVector,FEATURE_EXTRACTOR_DELIMITER,tokens);
				
				
				//first token contains eigen values
				LTKStringUtil::tokenizeString(tokens[0],",",subTokens);
				
				//extracting eigen values
				int i = 0;
				for(i = 0; i < subTokens.size(); i++)
				{
					
                    eigenValues.push_back(LTKStringUtil::convertStringToFloat(subTokens[i]));
					
				}
				
				clusterModel.setEigenValues(eigenValues);
				
				subTokens.clear();	
				
				//extracting eigen vectors
				
				for( i = 1; i < (eigenValues.size() + 1); i++)
				{
					LTKStringUtil::tokenizeString(tokens[i],",",subTokens);
					
					for(int j = 0; j < subTokens.size(); j++)
					{
                        tempVector.push_back(LTKStringUtil::convertStringToFloat(subTokens[j]));
					}
					
					
					eigenVectors.push_back(tempVector);
					tempVector.clear();
					subTokens.clear();
				}
				
				clusterModel.setEigenVectors(eigenVectors);
				
				
				//extracting cluster mean 
				
				LTKStringUtil::tokenizeString(tokens[(eigenValues.size() + 1)],",",subTokens);
				
				for( i = 0; i < subTokens.size(); i++)
				{
					
                    clusterMean.push_back(LTKStringUtil::convertStringToFloat(subTokens[i]));
				}
				
				clusterModel.setClusterMean(clusterMean);
				
				subTokens.clear();
				
				clusterModelVector.push_back(clusterModel);
			}
			else
			{
				//reading number of eigenValues
				int numEigenValues;
				mdtFileHandle.read((char*) &numEigenValues,intSize);
				
				//reading eigen values
				int i = 0;
				for(i = 0; i < numEigenValues; i++)
				{
					double eigenValue;
					mdtFileHandle.read((char*) &eigenValue,doubleSize );
					
					eigenValues.push_back(eigenValue);
					
					if ( mdtFileHandle.fail() )
					{
						break;
					}
				}
				
				clusterModel.setEigenValues(eigenValues);
				
				
				//reading eigenVectors
				for( i = 0; i < numEigenValues; i++)
				{
					for(int j = 0; j < clusterMeanDimension; j++)
					{
						double eigenVectorValue;
						mdtFileHandle.read((char*) &eigenVectorValue,doubleSize);
						tempVector.push_back(eigenVectorValue);
						
						if ( mdtFileHandle.fail() )
						{
							break;
						}
					}
					
					eigenVectors.push_back(tempVector);
					tempVector.clear();
				}
				
				clusterModel.setEigenVectors(eigenVectors);
				
				
				//reading cluster mean 
				for( i = 0; i < clusterMeanDimension; i++)
				{
					double clusterMeanValue;
					mdtFileHandle.read((char*) &clusterMeanValue,doubleSize);
					
					clusterMean.push_back(clusterMeanValue);
					
					if ( mdtFileHandle.fail() )
					{
						break;
					}
				}
				
				clusterModel.setClusterMean(clusterMean);
				
				
				clusterModelVector.push_back(clusterModel);
			}
			
			//clearing vectors
			
			eigenValues.clear();
			
			eigenVectors.clear();
			
			clusterMean.clear();
			
			tempVector.clear();
			tokens.clear();
			
	}
	
	/**READING SINGLETON VECTORS**/
	tempCount = 0;
	
	for(int singletonCount = 0; singletonCount < numSingletons; singletonCount++)
	{
		LTKShapeFeaturePtr feature;
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			strFeatureVector = "";
			mdtFileHandle >> strFeatureVector;
			
			//parsing the singleton vector
			LTKStringUtil::tokenizeString(strFeatureVector,  FEATURE_EXTRACTOR_DELIMITER,  tokens);
			
			for(int i = 0; i < tokens.size(); ++i)
			{
				feature = m_ptrFeatureExtractor->getShapeFeatureInstance();
				
				if (feature->initialize(tokens[i]) != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_INPUT_FORMAT << " " <<
						"Number of features extracted from a trace is not correct"<<
						" ActiveDTWShapeRecognizer::loadModelData()" << endl;
					
					LTKReturnError(EINVALID_INPUT_FORMAT);
				}
				
				singleton.push_back(feature);
			}
			
			singletonVector.push_back(singleton);
			
			singleton.clear();
			tokens.clear();
		}
		else
		{
			int featureIndex = 0;
			
			for ( ; featureIndex < numFeatures ; featureIndex++)
			{
				floatVector floatFeatureVector;
				int featureValueIndex = 0;
				
				feature = m_ptrFeatureExtractor->getShapeFeatureInstance();
				
				for(; featureValueIndex < featureDimension ; featureValueIndex++)
				{
					float featureValue = 0.0f;
					
					mdtFileHandle.read((char*) &featureValue, floatSize);
					
					floatFeatureVector.push_back(featureValue);
					
					if ( mdtFileHandle.fail() )
					{
						break;
					}
				}
				
				if (feature->initialize(floatFeatureVector) != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
						EINVALID_INPUT_FORMAT << " " <<
						"Number of features extracted from a trace is not correct"<<
						" ActiveDTWShapeRecognizer::loadModelData()" << endl;
					
					LTKReturnError(EINVALID_INPUT_FORMAT);
				}
				
				
				singleton.push_back(feature);
				
			}
			
			singletonVector.push_back(singleton);
			
			singleton.clear();
		}
	}
	
	/**CONSTRUCTING SHAPE MODEL**/
	
	
	iErrorCode = shapeModel.setShapeId(classId);
	if(iErrorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< iErrorCode << " "<< endl;
		LTKReturnError(iErrorCode);
	}
	
	shapeModel.setClusterModelVector(clusterModelVector);
	
	shapeModel.setSingletonVector(singletonVector);
	
	
	
	
	/**APPENDING THE SHAPE MODEL TO PROTOTYPE VECTOR**/ 
	
	m_prototypeShapes.push_back(shapeModel);
	
	
	m_shapeIDNumPrototypesMap[classId] = clusterModelVector.size();
	
	
	if(m_useSingleton == true || clusterModelVector.size() == 0)
		m_shapeIDNumPrototypesMap[classId] += singletonVector.size();
	
	
	
	
	
	clusterModelVector.clear();
	
	singletonVector.clear();
	
	if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
	{
		mdtFileHandle >> classId;
	}
	else
	{
		mdtFileHandle.read((char*) &classId, intSize);
		
		if ( mdtFileHandle.fail() )
		{
			break;
		}
	}
	
	
    }
	
	
    
    mdtFileHandle.close();
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::loadModelData()" << endl;
	
    return SUCCESS;
}



/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: recognize
* DESCRIsPTION	: recognizes the incoming tracegroup
* ARGUMENTS		: inTraceGroup - trace group to be recognized
*				  screenContext - screen context
*				  subSetOfClasses - subset of classes whose samples will be compared with traceGroup
*				  confThreshold - classes with confidence below this threshold are not returned, valid range of confThreshold: (0,1)
*				  numChoices - maximum number of choices to be returned
*				  outResultVector - result of recognition
* RETURNS		: SUCCESS on successful running of the code
* NOTES			:
* CHANGE HISTROY
* Author : 	Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::recognize(const LTKTraceGroup& traceGroup,
										const LTKScreenContext& screenContext,
										const vector<int>& subSetOfClasses,
										float confThreshold,
										int  numChoices,
										vector<LTKShapeRecoResult>& outResultVector)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::recognize()" << endl;
	
	
	//Check for empty traces in traceGroup
	
	if(traceGroup.containsAnyEmptyTrace())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<EEMPTY_TRACE << " " <<
			" Input trace is empty"<<
			" ActiveDTWShapeRecognizer::recognize()" << endl;
		LTKReturnError(EEMPTY_TRACE);
	}
	
	
	//Contains TraceGroup after Preprocessing is done
	LTKTraceGroup preprocessedTraceGroup;
	
	
	//Preprocess the traceGroup
	int errorCode = preprocess(traceGroup, preprocessedTraceGroup);
	if(  errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			getErrorMessage(errorCode)<<
			" ActiveDTWShapeRecognizer::recognize()" << endl;
		LTKReturnError(errorCode);
	}
	
	//Extract the shapeSample from preprocessedTraceGroup
	if(!m_ptrFeatureExtractor)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ENULL_POINTER << " " <<
			" m_ptrFeatureExtractor is NULL"<<
			" ActiveDTWShapeRecognizer::recognize()" << endl;
		LTKReturnError(ENULL_POINTER);
	}
	
	vector<LTKShapeFeaturePtr> shapeFeatureVec;
	errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
		shapeFeatureVec);
	
	if (errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			" ActiveDTWShapeRecognizer::recognize()" << endl;
		
		LTKReturnError(errorCode);
	}
	
	// call recognize with featureVector
	
	if(recognize( shapeFeatureVec, subSetOfClasses, confThreshold,
		numChoices, outResultVector) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            getErrorMessage(errorCode)<<
            " ActiveDTWShapeRecognizer::recognize()" << endl;
		LTKReturnError(errorCode);
		
	}
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::recognize()" << endl;
	
	return SUCCESS;
	
}
/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: recognize
* DESCRIsPTION	: recognizes the incoming tracegroup
* ARGUMENTS		: shapeFeatureVec - feature vector to be recognized
*				  screenContext - screen context
*				  subSetOfClasses - subset of classes whose samples will be compared with traceGroup
*				  confThreshold - classes with confidence below this threshold are not returned, valid range of confThreshold: (0,1)
*				  numChoices - maximum number of choices to be returned
*				  outResultVector - result of recognition
* RETURNS		: SUCCESS on successful running of the code
* NOTES			:
* CHANGE HISTROY
* Author : 	Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::recognize(const vector<LTKShapeFeaturePtr>& shapeFeatureVec,
										const vector<int>& inSubSetOfClasses,
										float confThreshold,
										int  numChoices,
										vector<LTKShapeRecoResult>& outResultVector)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"ActiveDTWShapeRecognizer::recognize()" << endl;
	
	m_cachedShapeFeature = shapeFeatureVec;
	
	//Creating a local copy of input inSubSetOfClasses, as it is const, STL's unique function modifies it
	vector<int> subSetOfClasses = inSubSetOfClasses;
	
	int numPrototypeShapes =  m_prototypeShapes.size();
	
	/*********Validation for m_prototypeShapes ***************************/
	if ( numPrototypeShapes == 0 )
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROTOTYPE_SET_EMPTY << " " <<
			" getErrorMessage(EPROTOTYPE_SET_EMPTY) "<<
			" ActiveDTWShapeRecognizer::recognize()" << endl;
		LTKReturnError(EPROTOTYPE_SET_EMPTY); 
	}
	
	/******************************************************************/
	/*******************VALIDATING INPUT ARGUMENTS*********************/
	/******************************************************************/
	
	// Validating numChoices: valid values: {-1, (0,m_numShapes]}
	if(numChoices <= 0 && numChoices != NUM_CHOICES_FILTER_OFF)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
			"numChoices ("<<numChoices<<")<=0, setting it to off (-1)"<<endl;
		numChoices = -1;
	}
	
	if(!m_projectTypeDynamic && numChoices > m_numShapes)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
			"numChoices ("<<numChoices<<") > numShapes ("<<
			m_numShapes<<"), using numShapes "<<m_numShapes<<" instead"<<endl;
		numChoices = m_numShapes;
	}
	
	
	//Validating confThreshold: valid values: [0,1]
	if(confThreshold > 1 || confThreshold < 0)
	{
		
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
			"Invalid value of confThreshold, valid values are: [0,1]"<<endl;
		LTKReturnError(ECONFIG_FILE_RANGE);
	}
	
	// Clearing cached Variables
	m_vecRecoResult.clear();
	m_neighborInfoVec.clear();
	
	//Temporary variable to be used to populate distIndexPairVector
	struct NeighborInfo tempPair;
	
	struct NeighborInfo tempDist;
	
	int i = 0;
	int j = 0;
	
	//Variable to store the DTW distance.
	float dtwDistance = 0.0f;
	
	//Variable to store the Euclidean distance.
	float euclideanDistance = 0.0f;
	
	
	/***************End of declarations and initializations of variables**************/
	
	
	
	/**CONVERTING THE FEATURE VECTOR TO DOUBLE***/
	
	doubleVector featureVector;
	floatVector floatFeatureVector;
	int errorCode = m_shapeRecUtil.shapeFeatureVectorToFloatVector(shapeFeatureVec,floatFeatureVector);
	
	if (errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::recognize()" << endl;
		
		LTKReturnError(errorCode);
	}
	
	int floatFeatureVectorSize = floatFeatureVector.size();
	
	
	for(i = 0; i < floatFeatureVectorSize; i++)
		featureVector.push_back(floatFeatureVector[i]);
	floatFeatureVector.clear();
	
	
	
	
    ActiveDTWShapeModel evalShapeModel;
    //current shape model evaluated against test Sample
	
    
    ActiveDTWClusterModel evalClusterModel;
    //currently evaluated cluster model
	
    
    vector<ActiveDTWClusterModel> clusterVector;
    //vector of cluster models of current shape model
	
    
    shapeMatrix evalSingletonVector;
    //vector of all singletonVectors of current shape model
	
    doubleVector eigenValues;
    //eigen values of cluster model
	
    double2DVector eigenVector;
    //eigen vectors of cluster model
	
    doubleVector clusterMean;
    //cluster mean of cluster model
	
    doubleVector deformationParameters;
    //paramters required to construct optimal Deformation
	
    doubleVector reconstructedSample;
    //double vector form of optima lDeformation
	
    shapeMatrix optimalDeformations;
    //vector of all optimalDeformations of a class
	
    vector<bool> clusterFilter;  
    //indicates which cluster are to be considered for computing DTW DISTANCE    
	
    vector<bool> singletonFilter;
    //indicates which singletons are to be considered for computing DTW DISTANCE
	
    vector<struct NeighborInfo> distInfo;
    //used in dtwEuclidean filter
	
    vector<LTKShapeFeaturePtr> shapeFeatureVector;
	
	
    /*****************COMPUTING DISTANCE******************************/
	if(subSetOfClasses.size() == 0)
	{
		for(i = 0; i < m_prototypeShapes.size(); i++)
		{
			evalShapeModel = m_prototypeShapes[i];
			clusterVector = evalShapeModel.getClusterModelVector();
			
			
			evalSingletonVector = evalShapeModel.getSingletonVector();
			
			
			int singletonSize = evalSingletonVector.size();
			int clusterVectorSize = clusterVector.size();
			
			
			
			//computing the optimalDeformations
			for(j = 0; j < clusterVectorSize; j++)
			{
				evalClusterModel = clusterVector[j];
				
				eigenVector = evalClusterModel.getEigenVectors();
				
				eigenValues = evalClusterModel.getEigenValues();
				
				clusterMean = evalClusterModel.getClusterMean();
				
				deformationParameters.assign(eigenVector.size(),0.0);
				
				
				int  errorCode = findOptimalDeformation(deformationParameters,eigenValues,eigenVector,
					clusterMean,featureVector);
				
				if (errorCode != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
						" ActiveDTWShapeRecognizer::recognize()" << endl;
					
					LTKReturnError(errorCode);
				}
				
				//reconstruct the sample
				double tempCoordinate = 0.0;
				int clusterMeanSize = clusterMean.size();		
				int deformationParametersSize = deformationParameters.size();
				
				for(int k = 0; k < clusterMeanSize; k++)
				{
					tempCoordinate = clusterMean[k];
					
					for(int l = 0; l < deformationParametersSize; l++)
						tempCoordinate += deformationParameters[l]*eigenVector[l][k];
					
					reconstructedSample.push_back(tempCoordinate);
				}
				
				//converting the reconstructed Sample to a featureVector
				errorCode = convertDoubleToFeatureVector(shapeFeatureVector,reconstructedSample);
				
				if(errorCode !=  SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
						" ActiveDTWShapeRecognizer::recognize()" << endl;
					
					LTKReturnError(errorCode);
				}
				
				optimalDeformations.push_back(shapeFeatureVector);
				
				//clearing vectors
				eigenValues.clear();
				eigenVector.clear();
				clusterMean.clear();	
				reconstructedSample.clear();
				shapeFeatureVector.clear();
				deformationParameters.clear();
			}
			
			
			//setting up dtweuclidean filter for the class
			if(m_dtwEuclideanFilter != EUCLIDEAN_FILTER_OFF)
			{
				//calculating euclidean distance to clusters
				for( j = 0; j < clusterVectorSize; j++)
				{
					
					euclideanDistance = 0.0;
					
					errorCode = computeEuclideanDistance(shapeFeatureVec,optimalDeformations[j],euclideanDistance);
					
					
					
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
							" ActiveDTWShapeRecognizer::recognize()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					tempDist.typeId = CLUSTER;
					tempDist.sampleId = j;
					tempDist.distance = euclideanDistance;
					
					distInfo.push_back(tempDist);
				}
				
				//calcualting euclidean distances to singletons
				if(m_useSingleton == true || clusterVectorSize == 0)
				{
					
					for(j = 0; j < singletonSize; j++)
					{
						
						euclideanDistance = 0.0;
						//computing euclidean distance between test sample
						//and singleton vectors
						
						errorCode = computeEuclideanDistance(shapeFeatureVec,evalSingletonVector[j],
							euclideanDistance);
						
						if(errorCode != SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
								" ActiveDTWShapeRecognizer::recognize()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						tempDist.typeId = SINGLETON;
						tempDist.sampleId = j;
						tempDist.distance = euclideanDistance;
						
						distInfo.push_back(tempDist);
						
						
					}
				}
				
				
				//sorting the euclidean distances in ascending order
				sort(distInfo.begin(), distInfo.end(), sortDist);
				
				
				//choose the top n
				int numTopChoices = (int)(distInfo.size() * m_dtwEuclideanFilter)/100;
				if(numTopChoices == 0)
				{
					numTopChoices = distInfo.size();
				}
				
				//setting the filter
				clusterFilter.assign(clusterVectorSize,false);
				
				if(m_useSingleton == true || clusterVectorSize == 0)
					singletonFilter.assign(singletonSize,false);
				
				
				for( j = 0; j < numTopChoices; j++)
				{
					if(distInfo[j].typeId == 0)
						clusterFilter[distInfo[j].sampleId] = true;
					
					if(distInfo[j].typeId == 1)
						singletonFilter[distInfo[j].sampleId] = true;
				}
				
				//clearing distInfo
				distInfo.clear();
			}
			else
			{
				clusterFilter.assign(clusterVectorSize,true);
				
				if(m_useSingleton == true || clusterVectorSize == 0)
					singletonFilter.assign(singletonSize,true);
			}
			
			
			/*****DETERMINING THE MINIMUM CLUSTER DISTANCE***************/
			float minDistance = FLT_MAX;
			float minClusterDistance;
			float minSingletonDistance;
			
			int clusterId;
			int singletonId;
			
			for( j = 0; j < clusterVectorSize; j++)
			{
				
				if(clusterFilter[j]) 
				{
					float tempDistance = 0.0;
					errorCode = computeDTWDistance(shapeFeatureVec,optimalDeformations[j],tempDistance);
					
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
							" ActiveDTWShapeRecognizer::recognize()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					if(tempDistance < minDistance)
					{
						minDistance = tempDistance;
						clusterId = j;
					}
					
					
					
				}
				
			}
			
			clusterVector.clear();
			clusterFilter.clear();
			optimalDeformations.clear();
			
			minClusterDistance = minDistance;
			
			/***DETERMINE THE MINIMUM DISTANCE FROM CLUSTERS ONLY IF THE 
			USE SINGLETON SWITCH IS TURNED ON. IF THE NUMBER OF CLUSTERS
			IN A CLASS IS 0 THEN AUTOMATICALLY TURN ON THE SINGLETON SWITCH
			FOR THAT CLASS ALONE ***/
			
			if(m_useSingleton == false && clusterVectorSize == 0)
				m_useSingleton = true;
			
			/***************DETERMINING MINIMUM DISTANCE FROM SINGLETON VECTORS*********/
			if(m_useSingleton == true)
			{
				evalSingletonVector = evalShapeModel.getSingletonVector();
				
				int evalSingletonVectorSize = evalSingletonVector.size();
				
				for(int j = 0; j < evalSingletonVectorSize; j++)
				{
					if(singletonFilter[j])
					{
						
						//calculate the dtw distance between testsamples and every singleton vector
						float tempDistance = 0.0;
						
						errorCode = computeDTWDistance(shapeFeatureVec,evalSingletonVector[j],tempDistance);
						
						if(errorCode != SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
								" ActiveDTWShapeRecognizer::recognize()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						if(tempDistance < minDistance)
						{
							minDistance = tempDistance;
							singletonId = j;
						}
						
					}
				}
				singletonFilter.clear();
			}
			
			//clearing vectors
			evalSingletonVector.clear();
			
			minSingletonDistance = minDistance;
			
			//choosing the minimum distance
			if(m_useSingleton == false)
			{
				tempPair.distance = minClusterDistance;
				tempPair.typeId = CLUSTER;
				tempPair.sampleId = clusterId;
			}
			else
			{
				if(clusterVectorSize == 0)
				{
					tempPair.distance = minSingletonDistance;
					tempPair.typeId = SINGLETON;
					tempPair.sampleId = singletonId;
				}
				else
				{
					if(minClusterDistance < minSingletonDistance)
					{
						tempPair.distance = minClusterDistance;
						tempPair.typeId = CLUSTER;
						tempPair.sampleId = clusterId;
					}
					else
					{
						tempPair.distance = minSingletonDistance;
						tempPair.typeId = SINGLETON;
						tempPair.sampleId = singletonId;
					}
				}
			}
			
			//turning off the singleton switch in case it was turned on automatically
			if(m_useSingleton == true && clusterVectorSize == 0)
				m_useSingleton = false;	
			
			
			tempPair.classId = evalShapeModel.getShapeId();
			m_neighborInfoVec.push_back(tempPair);
	}
    }
    else
    {
		/*****EVALUATE TEST SAMPLES ONLY AGAINST CLASSES SPECIFIED BY SUBSETOFCLASSES***/
		intVector::iterator subSetStart = subSetOfClasses.begin();
		intVector::iterator subSetEnd = subSetOfClasses.end();
		
		for(;subSetStart != subSetEnd; ++subSetStart)
		{
			evalShapeModel = m_prototypeShapes[(*subSetStart)];
			
			clusterVector = evalShapeModel.getClusterModelVector();
			
			evalSingletonVector = evalShapeModel.getSingletonVector();
			
			int clusterVectorSize = clusterVector.size();
			int singletonSize = evalSingletonVector.size();
			
			//computing the optimalDeformations
			for( j = 0; j < clusterVectorSize; j++)
			{
				evalClusterModel = clusterVector[j];
				
				eigenVector = evalClusterModel.getEigenVectors();
				
				eigenValues = evalClusterModel.getEigenValues();
				
				clusterMean = evalClusterModel.getClusterMean();
				
				deformationParameters.assign(eigenVector.size(),0.0);
				
				int  errorCode = findOptimalDeformation(deformationParameters,eigenValues,eigenVector,
					clusterMean,featureVector);
				
				if (errorCode != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
						" ActiveDTWShapeRecognizer::recognize()" << endl;
					
					LTKReturnError(errorCode);
				}
				
				//reconstruct the sample
				double tempCoordinate = 0.0;
				int clusterMeanSize = clusterMean.size();		
				int deformationParametersSize = deformationParameters.size();
				
				for(int k = 0; k < clusterMeanSize; k++)
				{
					tempCoordinate = clusterMean[k];
					
					for(int l = 0; l < deformationParametersSize; l++)
						tempCoordinate += deformationParameters[l]*eigenVector[l][k];
					
					reconstructedSample.push_back(tempCoordinate);
				}
				
				//converting the reconstructed Sample to a featureVector
				errorCode = convertDoubleToFeatureVector(shapeFeatureVector,reconstructedSample);
				
				if(errorCode !=  SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
						" ActiveDTWShapeRecognizer::recognize()" << endl;
					
					LTKReturnError(errorCode);
				}
				
				optimalDeformations.push_back(shapeFeatureVector);
				
				//clearing vectors
				eigenValues.clear();
				eigenVector.clear();
				clusterMean.clear();	
				reconstructedSample.clear();
				shapeFeatureVector.clear();
				deformationParameters.clear();
			}
			
			//setting up dtweuclidean filter for the class
			if(m_dtwEuclideanFilter != EUCLIDEAN_FILTER_OFF)
			{
				//calculating euclidean distance to clusters
				for(j = 0; j < clusterVectorSize; j++)
				{
					euclideanDistance = 0.0;
					
					errorCode = computeEuclideanDistance(shapeFeatureVec,optimalDeformations[j],euclideanDistance);
					
					
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
							" ActiveDTWShapeRecognizer::recognize()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					tempDist.typeId = CLUSTER;
					tempDist.sampleId = j;
					tempDist.distance = euclideanDistance;
					
					distInfo.push_back(tempDist);
				}
				
				//calcualting euclidean distances to singletons
				if(m_useSingleton == true || clusterVectorSize == 0)
				{
					for(j = 0; j < singletonSize; j++)
					{
						euclideanDistance = 0.0;
						//computing euclidean distance between test sample
						//and singleton vectors
						
						errorCode = computeEuclideanDistance(shapeFeatureVec,evalSingletonVector[j],
							euclideanDistance);
						
						if(errorCode != SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
								" ActiveDTWShapeRecognizer::recognize()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						tempDist.typeId = SINGLETON;
						tempDist.sampleId = j;
						tempDist.distance = euclideanDistance;
						
						distInfo.push_back(tempDist);
					}
				}
				
				//sorting the euclidean distances in ascending order
				
				sort(distInfo.begin(), distInfo.end(), sortDist);
				
				
				//choose the top n
				int numTopChoices = (int)(distInfo.size() * m_dtwEuclideanFilter)/100;
				
				if(numTopChoices == 0)
				{
					numTopChoices = distInfo.size();
				}
				
				//setting the filter
				clusterFilter.assign(clusterVectorSize,false);
				
				if(m_useSingleton == true || clusterVectorSize == 0)
					singletonFilter.assign(singletonSize,false);
				
				for( j = 0; j < numTopChoices; j++)
				{
					if(distInfo[j].typeId == 0)
						clusterFilter[distInfo[j].sampleId] = true;
					
					if(distInfo[j].typeId == 1)
						singletonFilter[distInfo[j].sampleId] = true;
				}
				
				//clearing distInfo
				distInfo.clear();
			}
			
			/*****DETERMINING THE MINIMUM CLUSTER DISTANCE***************/
			float minDistance = FLT_MAX;
			float minClusterDistance;
			float minSingletonDistance;
			
			int clusterId;
			int singletonId;
			
			for( j = 0; j < clusterVectorSize; j++)
			{
				if(clusterFilter[j]) 
				{
					float tempDistance = 0.0;
					errorCode = computeDTWDistance(shapeFeatureVec,optimalDeformations[j],tempDistance);
					
					if (errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
							" ActiveDTWShapeRecognizer::recognize()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					if(tempDistance < minDistance)
					{
						minDistance = tempDistance;
						clusterId = j;
					}
				}
			}
			
			clusterVector.clear();
			clusterFilter.clear();
			optimalDeformations.clear();
			
			minClusterDistance = minDistance;
			
			/***DETERMINE THE MINIMUM DISTANCE FROM CLUSTERS ONLY IF THE 
			USE SINGLETON SWITCH IS TURNED ON. IF THE NUMBER OF CLUSTERS
			IN A CLASS IS 0 THEN AUTOMATICALLY TURN ON THE SINGLETON SWITCH
			FOR THAT CLASS ALONE ***/
			
			if(m_useSingleton == false && clusterVectorSize == 0)
				m_useSingleton = true;
			
			/***************DETERMINING MINIMUM DISTANCE FROM SINGLETON VECTORS*********/
			if(m_useSingleton == true)
			{
				
				evalSingletonVector = evalShapeModel.getSingletonVector();
				int evalSingletonVectorSize = evalSingletonVector.size();
				
				for(j = 0; j < evalSingletonVectorSize; j++)
				{
					if(singletonFilter[j])
					{
						//calculate the dtw distance between testsamples and every singleton vector
						float tempDistance = 0.0;
						
						errorCode = computeDTWDistance(shapeFeatureVec,evalSingletonVector[j],tempDistance);
						
						if (errorCode != SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
								" ActiveDTWShapeRecognizer::recognize()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						
						if(tempDistance < minDistance)
						{
							minDistance = tempDistance;
							singletonId = j;
						}
						
						
					}
				}
				
				singletonFilter.clear();
			}
			
			//clearing vectors
			evalSingletonVector.clear();
			
			minSingletonDistance = minDistance;
			
			//choosing the minimum distance
			if(m_useSingleton == false)
			{
				tempPair.distance = minClusterDistance;
				tempPair.typeId = CLUSTER;
				tempPair.sampleId = clusterId;
			}
			else
			{
				if(clusterVectorSize == 0)
				{
					tempPair.distance = minSingletonDistance;
					tempPair.typeId = SINGLETON;
					tempPair.sampleId = singletonId;
				}
				else
				{
					if(minClusterDistance < minSingletonDistance)
					{
						tempPair.distance = minClusterDistance;
						tempPair.typeId = CLUSTER;
						tempPair.sampleId = clusterId;
					}
					else
					{
						tempPair.distance = minSingletonDistance;
						tempPair.typeId = SINGLETON;
						tempPair.sampleId = singletonId;
					}
				}
			}
			
			//turning off the singleton switch in case it was turned on automatically
			if(m_useSingleton == true && clusterVectorSize == 0)
				m_useSingleton = false;	
			
			
			tempPair.classId = evalShapeModel.getShapeId();
			
			
			m_neighborInfoVec.push_back(tempPair);
		}
    }
	
    featureVector.clear();
    
	
    //Sort the distIndexPairVector based on distances, in ascending order.
    sort(m_neighborInfoVec.begin(), m_neighborInfoVec.end(), sortDist);
	
    //Reject the sample if the similarity of the nearest sample is lower than m_rejectThreshold specified by the user.
    if(SIMILARITY(m_neighborInfoVec[0].distance) <= m_rejectThreshold)
    {
        m_vecRecoResult.clear();
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<"Test sample too distinct, rejecting the sample"<<endl;
        return SUCCESS;
    }
	
    //compute confidence
    if((errorCode = computeConfidence()) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);
    }
	
    // Temporary result vector to store the results based on confThreshold and numChoices specified by the user.
    vector<LTKShapeRecoResult> tempResultVector;
	
    //If confThreshold is specified, get the entries from resultVector with confidence >= confThreshold
    if(confThreshold != CONF_THRESHOLD_FILTER_OFF)
    {
        for(i = 0 ; i < m_vecRecoResult.size() ; i++)
        {
            if( m_vecRecoResult[i].getConfidence() >= confThreshold)
            {
                tempResultVector.push_back(m_vecRecoResult[i]);
            }
        }
        m_vecRecoResult.clear();
        m_vecRecoResult = tempResultVector;
        tempResultVector.clear();
    }
    //Check if outResultVector is empty, if so, log
    if(m_vecRecoResult.size() == 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO) <<
            "Size of the result vector is empty, could not satisfy confThreshold criteria"<<endl;
    }
	
    //If numChoices is specified, get the top numChoices entries from outResultVector
    if(numChoices != NUM_CHOICES_FILTER_OFF)
    {
        //Get the entries from outResultVector only if size of resultVector > numChoices
        if(m_vecRecoResult.size() > numChoices)
        {
            for( i = 0 ; i < numChoices ; ++i)
                tempResultVector.push_back(m_vecRecoResult[i]);
            m_vecRecoResult.clear();
            m_vecRecoResult = tempResultVector;
            tempResultVector.clear();
        }
		
    }
	
    outResultVector = m_vecRecoResult;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::recognize()" << endl;
	
    return SUCCESS;
	
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: sortDist
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
bool ActiveDTWShapeRecognizer::sortDist(const NeighborInfo& x, const NeighborInfo& y)
{
    return (x.distance) < (y.distance);
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME                  : computeConfidence
* DESCRIPTION   :
* ARGUMENTS             :
* RETURNS               :
* NOTES                 :
* CHANGE HISTROY
* Author                        Date                            Description
******************************************************************************/
int ActiveDTWShapeRecognizer::computeConfidence()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::computeConfidence()" << endl;
	
	/******************************************************************/
	/*******************VALIDATING INPUT ARGUMENTS*********************/
	/******************************************************************/
	if(m_neighborInfoVec.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENEIGHBOR_INFO_VECTOR_EMPTY << " " <<
			getErrorMessage(ENEIGHBOR_INFO_VECTOR_EMPTY) <<
			" ActiveDTWShapeRecognizer::computeConfidence()" << endl;
		
		LTKReturnError(ENEIGHBOR_INFO_VECTOR_EMPTY);
	}
	// Temporary vector to store the recognition results
	LTKShapeRecoResult outResult;
	vector<pair<int,float> > classIdSimilarityPairVec;
	pair<int, float> classIdSimilarityPair;
	
	// Temporary vector to store the distinct classes appearing in distIndexPairVector
	intVector distinctClassVector;
	intVector::iterator distinctClassVectorIter;
	
	vector <LTKShapeRecoResult>::iterator resultVectorIter = m_vecRecoResult.begin();
	vector <LTKShapeRecoResult>::iterator resultVectorIterEnd = m_vecRecoResult.end();
	
	// Variable to store sum of distances to all the prototypes in distIndexPairVector
	float similaritySum = 0.0f;
	// Temporary variable to store the confidence value.
	float confidence = 0.0f;
	
	
	
	// Confidence computation for the ActiveDTW (1-NN) Classifier
	if(m_nearestNeighbors == 1)
	{
		vector <struct NeighborInfo>::iterator distIndexPairIter = m_neighborInfoVec.begin();
		vector <struct NeighborInfo>::iterator distIndexPairIterEnd = m_neighborInfoVec.end();	
		
		
		
		for(; distIndexPairIter != distIndexPairIterEnd; ++distIndexPairIter)
		{
			//Check if the class is already present in distinctClassVector
			//The complexity of STL's find() is linear, with atmost last-first comparisons for equality
			distinctClassVectorIter = find(distinctClassVector.begin(), distinctClassVector.end(), (*distIndexPairIter).classId);
			
			//The distinctClassVectorIter will point to distinctClassVector.end() if the class is not present in distinctClassVector
			if(distinctClassVectorIter == distinctClassVector.end())	
			{
				classIdSimilarityPair.first = (*distIndexPairIter).classId ;
				float similarityValue = SIMILARITY((*distIndexPairIter).distance);
                
				classIdSimilarityPair.second = similarityValue;
				similaritySum += similarityValue;
                
				classIdSimilarityPairVec.push_back(classIdSimilarityPair);
				distinctClassVector.push_back((*distIndexPairIter).classId);
			}
		}
		
		/************COMPUTING CONFIDENCE VALUES FOR EACH CLASS************/
		int classIdSimilarityPairVecSize = classIdSimilarityPairVec.size();
		for( int i = 0 ; i < classIdSimilarityPairVecSize ; ++i)
		{
			int classID = classIdSimilarityPairVec[i].first;
			confidence = classIdSimilarityPairVec[i].second;
			confidence /= similaritySum;
			outResult.setConfidence(confidence);
			outResult.setShapeId(classID);
			
			if(confidence > 0)
				m_vecRecoResult.push_back(outResult);
		}
		classIdSimilarityPairVec.clear();
	}
	// Computing confidence for k-NN classifier, implementation of the confidence measure described in paper (cite)
	else
	{
		if(m_nearestNeighbors >= m_neighborInfoVec.size())
		{
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
				"m_nearestNeighbors >= m_prototypeSet.size(), using distIndexPairVector.size() for m_nearestNeighbors instead" << endl;
			m_nearestNeighbors = m_neighborInfoVec.size();
		}
		
		// Variable to store the maximum of the number of samples per class.
		int maxClassSize = (max_element(m_shapeIDNumPrototypesMap.begin(), m_shapeIDNumPrototypesMap.end(), &compareMap))->second;
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "maxClassSize: " <<maxClassSize<< endl;
		
		// Vector to store the cumulative similarity values
		vector<float> cumulativeSimilaritySum;
		
		// Populate the values in cumulativeSimilaritySum vector for the top m_nearestNeighbors prototypes
		// Assumption is m_nearestNeighbors >= MIN_NEARESTNEIGHBORS and m_nearestNeighbors < m_dtwEuclideanFilter, validation done in readClassifierConfig()
		
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Displaying cumulativeSimilaritySum..." << endl;
		int i = 0;
		for( i = 0 ; i < m_nearestNeighbors ; ++i)
		{
			
			classIdSimilarityPair.first = m_neighborInfoVec[i].classId;
			float similarityValue = SIMILARITY((m_neighborInfoVec[i]).distance);
			
			classIdSimilarityPair.second = similarityValue;
			
			classIdSimilarityPairVec.push_back(classIdSimilarityPair);
			similaritySum += similarityValue;
			cumulativeSimilaritySum.push_back(similaritySum);
			
			// Logging the cumulative similarity values for debugging
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "classID:" <<
				m_neighborInfoVec[i].classId << " confidence:" <<
				similarityValue << endl;
			
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << i << ": " << similaritySum << endl;
		}
		
		
		for(i = 0 ; i < classIdSimilarityPairVec.size() ; ++i)
		{
			
			int classID = classIdSimilarityPairVec[i].first;
			
			int finalNearestNeighbors = 0;
			
			//Check if the class is already present in distinctClassVector
			distinctClassVectorIter = find(distinctClassVector.begin(), distinctClassVector.end(), classID);
			
			//The distinctClassVectorIter will point to distinctClassVector.end() if the class is not present in distinctClassVector
			if(distinctClassVectorIter == distinctClassVector.end())
			{
				distinctClassVector.push_back(classID);
				confidence = 0.0f;
				
				//If the confidence is based on Adaptive k-NN scheme,
				//Computing number of nearest neighbours for the class to be used for computation of confidence
				if(m_adaptivekNN == true )
				{
					
					int sizeProportion = (int)ceil(1.0*m_nearestNeighbors*m_shapeIDNumPrototypesMap[classID]/maxClassSize);
					LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"sizeProportion of class " <<classID<<" is "<<sizeProportion<<endl;
					
					// Computing min(sizeProportion, m_shapeIDNumPrototypesMap[classID])
					int upperBound = (sizeProportion < m_shapeIDNumPrototypesMap[classID]) ? sizeProportion:m_shapeIDNumPrototypesMap[classID];
					LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"upperBound: " <<upperBound<<endl;
					
					// Computing max(upperBound, MIN_NEARESTNEIGHBORS)
					finalNearestNeighbors = (MIN_NEARESTNEIGHBORS > upperBound) ? MIN_NEARESTNEIGHBORS:upperBound;
				}
				//Else, compute kNN based confidence
				else if(m_adaptivekNN == false)
				{
					finalNearestNeighbors = m_nearestNeighbors;
				}	
				else
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Error: " << ECONFIG_FILE_RANGE << " " <<
						"m_adaptivekNN should be true or false" <<
						" ActiveDTWShapeRecognizer::computeConfidence()" << endl;
					LTKReturnError(ECONFIG_FILE_RANGE);
				}
				
				
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"finalNearestNeighbors: " <<finalNearestNeighbors<<endl;
				
				for( int j = 0 ; j < finalNearestNeighbors ; ++j)
				{
					if(classID == classIdSimilarityPairVec[j].first)
					{
						confidence += classIdSimilarityPairVec[j].second;
					}
				}
				confidence /= cumulativeSimilaritySum[finalNearestNeighbors-1];
				
				outResult.setShapeId(classID);
				outResult.setConfidence(confidence);
				
				if(confidence > 0)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"classId: " <<classID<<" confidence: "<<confidence<<endl;
					
					m_vecRecoResult.push_back(outResult);
				}
			}
		}
		classIdSimilarityPairVec.clear();
	}
	
	//Sort the result vector in descending order of confidence
	sort(m_vecRecoResult.begin(), m_vecRecoResult.end(), sortResultByConfidence);
	
	distinctClassVector.clear();
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::computeConfidence()" << endl;
	
	return SUCCESS;
	
}


/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME                  : sortResultByConfidence
* DESCRIPTION  	 : Sort the LTKShapeRecoResult vector based on the Confidence value
* ARGUMENTS             :
* RETURNS               :
* NOTES                 :
* CHANGE HISTROY
* Author                        Date                            Description
******************************************************************************/
bool ActiveDTWShapeRecognizer::sortResultByConfidence(const LTKShapeRecoResult& x, const LTKShapeRecoResult& y)
{
    return (x.getConfidence()) > (y.getConfidence());
}
/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME                  : compareMap
* DESCRIPTION  	 : Sort the STL's map based on the 'value'(second) field
* ARGUMENTS             :
* RETURNS               :
* NOTES                 :
* CHANGE HISTROY
* Author                        Date                            Description
******************************************************************************/

bool ActiveDTWShapeRecognizer::compareMap( const map<int, int>::value_type& lhs, const map<int, int>::value_type& rhs )
{
    return lhs.second < rhs.second;
}



/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: unloadModelData
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		: SUCCESS
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::unloadModelData()
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::unloadModelData()" << endl;
	
    int returnStatus = SUCCESS;
	
    //Update MDT file with any modification, if available in memory
    if(m_prototypeSetModifyCount >0)
    {
       	m_prototypeSetModifyCount = m_MDTUpdateFreq-1;
		
		returnStatus = writePrototypeShapesToMDTFile();
       	if(returnStatus != SUCCESS)
       	{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
               	" ActiveDTWShapeRecognizer::unloadModelData()" << endl;
       	}
        m_prototypeSetModifyCount = 0;
    }
	
    //Clearing the prototypSet
    m_prototypeShapes.clear();
    m_shapeIDNumPrototypesMap.clear();
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::unloadModelData()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: setDeviceContext
* DESCRIPTION	: New Function - Not yet added
* ARGUMENTS		:
* RETURNS		: SUCCESS
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::setDeviceContext(const LTKCaptureDevice& deviceInfo)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::setDeviceContext()" << endl;
	
    m_captureDevice = deviceInfo;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::setDeviceContext()" << endl;
	
    return SUCCESS;
}
/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: performClustering
* DESCRIPTION	        : This method performs clustering on each class data
and returns a vector specifying which samples
belong to which cluster 
* ARGUMENTS		: INPUT
shapeSamplesVec vector<LTKShapeSample> Class Data to be clustered 
: OUTPUT
outputVector int2DVector
(Here each row of the outputVector denotes a cluster
in turn each row holds the indices of the samples
belonging to that cluster)
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::performClustering(const vector<LTKShapeSample>& shapeSamplesVec,
												int2DVector& outputVector)
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::performClustering()" << endl;
	
    intVector tempVec;
    
    float2DVector distanceMatrix;
    int sampleCount=shapeSamplesVec.size();
    int returnStatus = SUCCESS;
	
    
	
    if(m_prototypeReductionFactor == -1)
    {
		//find number of clusters automatically
        //this is done when either of NumClusters or PrototypeReducrion factor is set
        //to automatic
        LTKHierarchicalClustering<LTKShapeSample,ActiveDTWShapeRecognizer> hc(shapeSamplesVec,AVERAGE_LINKAGE,AVG_SIL);
        
        returnStatus = hc.cluster(this,&ActiveDTWShapeRecognizer::computeDTWDistance);
        if(returnStatus != SUCCESS)
        {
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
				" ActiveDTWShapeRecognizer::performClustering()" << endl;
			LTKReturnError(returnStatus);
        }
        
        //Cluster results are populated in an outputVector
        hc.getClusterResult(outputVector);
		
    }
    else if(m_prototypeReductionFactor == 0|| m_numClusters >= sampleCount)
    {
		intVector clusterIndices;
        //case where clustering is not required every sample is a cluster by itself
        for(int i = 0; i < sampleCount; i++)
			clusterIndices.push_back(i);
		
		outputVector.push_back(clusterIndices);
		clusterIndices.clear();
		
    }
    else
    {
		//clustering has to be performed
		
        int numClusters;
        if(m_numClusters == NN_NUM_CLUST_INITIAL)
        {
            numClusters = (100-m_prototypeReductionFactor)*sampleCount/100;
            if(numClusters == 0)
            {
                numClusters = 1;
            }
        }
		
        else if(m_prototypeReductionFactor == NN_NUM_CLUST_INITIAL)
        {
            numClusters = m_numClusters;
        }
		
		try
		{
			
			LTKHierarchicalClustering<LTKShapeSample,ActiveDTWShapeRecognizer>
				hc(shapeSamplesVec,numClusters, AVERAGE_LINKAGE);
			
			if(numClusters == 1)
			{
				int tempVar;
				hc.computeProximityMatrix(this, &ActiveDTWShapeRecognizer::computeDTWDistance);
				
				
				for(tempVar=0;tempVar<shapeSamplesVec.size();tempVar++)
				{
					tempVec.push_back(tempVar);
				}
				
				outputVector.push_back(tempVec);
				tempVec.clear();
			}
			else
			{
				returnStatus = hc.cluster(this,&ActiveDTWShapeRecognizer::computeDTWDistance);
				if(returnStatus != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
						" ActiveDTWShapeRecognizer::performClustering()" << endl;
					LTKReturnError(returnStatus);
				}	
				
				//Cluster results are populated in an outputVector
				hc.getClusterResult(outputVector);
			}
			
		}
		catch(LTKException e)
		{
			int errorCode = e.getErrorCode();
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
				" ActiveDTWShapeRecognizer::performClustering()" << endl;
			LTKReturnError(errorCode);
		}
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::performClustering()" << endl;
	
    
	
    return SUCCESS;
	
}


/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getShapeSampleFromInkFile
* DESCRIPTION	: This method will get the ShapeSample by giving the ink
*				  file path as input
* ARGUMENTS		:
* RETURNS		: SUCCESS
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile(const string& inkFilePath,
														 vector<LTKShapeFeaturePtr>& shapeFeatureVec)
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
	
    if ( inkFilePath.empty() )
        return FAILURE;
	
    LTKCaptureDevice captureDevice;
    LTKScreenContext screenContext;
    
	
    LTKTraceGroup inTraceGroup, preprocessedTraceGroup;
    
	
    inTraceGroup.emptyAllTraces();
	
    int returnVal = m_shapeRecUtil.readInkFromFile(inkFilePath,
		m_lipiRootPath, inTraceGroup,
		captureDevice, screenContext);
	
    if (returnVal!= SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<returnVal<<
            " ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(returnVal);
    }
	
    m_ptrPreproc->setCaptureDevice(captureDevice);
    m_ptrPreproc->setScreenContext(screenContext);
	
    preprocessedTraceGroup.emptyAllTraces();
	
    //Preprocessing to be done for the trace group that was read
	int errorCode = preprocess(inTraceGroup, preprocessedTraceGroup);
    if(  errorCode != SUCCESS )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }
    
    
	
    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
		shapeFeatureVec);
    
	
    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
	
    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: trainFromListFile
* DESCRIPTION	: This method will do the training by giving the Train List
*				  file as input
* ARGUMENTS		:
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int ActiveDTWShapeRecognizer::trainFromListFile(const string& listFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
    
	
    //Count for the no. of samples read for a shape
    int sampleCount = 0;
	
    //Count of the no. of shapes read so far
    int shapeCount = 0;
	
    //Flag to skip reading a newline in the list file, when a new class starts
    bool lastshapeIdFlag = false;
	
    //Ink File Path
    string path = "";
	
    //Line from the list file
    string line = "";
	
    //Line is split into tokens
    stringVector tokens;
	
    //Flag is set when EOF is reached
    bool eofFlag = false;
	
    //ID for each shapes
    int shapeId = -1;
	
    //classId of the character
    int prevClassId = -1;
	
    //Indicates the first class
    bool initClassFlag = false;
	
    /**vectors for cluster indices
    *each vector corresponds to a cluster in a class
    *each vector has the indices of the shapesamples belonging to the cluster
    **/
    int2DVector clusterIndices;
	
    /**
	* this will hold a vector of indices pertaining to a certain cluster
    **/
    intVector cluster;
	
    /**
	* this will hold a temporary float feature vector
    **/
    doubleVector tempFeature;
	
	/**
    * Feature Matrix
    **/
    double2DVector featureMatrix;
	
	/**
    * Covariance Matrix
	**/
    double2DVector covarianceMatrix;
	
	/**
    * Eigen Vector Matrix
	**/
    double2DVector eigenVectors;
	
	/**
    * Eigen Values
	**/
    doubleVector eigenValues;
	
	/**
    * nrot --> number of iterations for eigen vector computation
	**/
    int nrot = 0;
	
    /**
	*   clusterModel
	**/
    
    ActiveDTWClusterModel clusterModel;
	
    
	
    /**
	*  vector of cluster models 
    **/
    vector<ActiveDTWClusterModel> clusterModelVector;
    
	
    /**
	*  shape Model
    **/
    
    ActiveDTWShapeModel shapeModel;
	
    /**
	*  vector of singleton clusters
    **/
    
    shapeMatrix singletonVector;
	
    /**
	*  selected eigen vectors
    **/
    double2DVector selectedEigenVectors;
	
    /**
	* cluster mean 
    **/
    doubleVector clusterMean;
	
	
	
    LTKShapeSample shapeSampleFeatures;
	
	
    vector<LTKShapeSample> shapeSamplesVec;
	
    vector<LTKShapeSample> clusteredShapeSampleVec;
	
    ofstream mdtFileHandle;
    ifstream listFileHandle;
	
    vector<LTKShapeFeaturePtr> shapeFeature;
	
    //Opening the train list file for reading mode
    listFileHandle.open(listFilePath.c_str(), ios::in);
	
    //Return error if unable to open the training list file
    if(!listFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ETRAINLIST_FILE_OPEN << " " <<
            getErrorMessage(ETRAINLIST_FILE_OPEN)<<
            " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(ETRAINLIST_FILE_OPEN);
    }
	
    //Open the Model data file for writing mode
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_activedtwMDTFilePath.c_str(), ios::out);
    }
    else
    {
        mdtFileHandle.open(m_activedtwMDTFilePath.c_str(),ios::out|ios::binary);
    }
	
    //Return error if unable to open the Model data file
    if(!mdtFileHandle)
    {
        listFileHandle.close();
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            getErrorMessage(EMODEL_DATA_FILE_OPEN)<<
            " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(EMODEL_DATA_FILE_OPEN);
    }
	
    //Write the number of Shapes
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
		mdtFileHandle << m_numShapes << endl;
    } 
    else
    {
		mdtFileHandle.write((char*) &m_numShapes, sizeof(unsigned short));
    }
	
    int errorCode = SUCCESS;
    while(!listFileHandle.eof())
    {
        // Not a sample of a new class
        if( lastshapeIdFlag == false )
        {
            //Get the line from the list file
            getline(listFileHandle, line, NEW_LINE_DELIMITER);
			
            path  = "";
			
            //Check if EOF is reached
            if( listFileHandle.eof() )
            {
                eofFlag = true;
            }
			
            //Skip commented line
            if ( line[0] == COMMENTCHAR )
            {
                continue;
            }
			
            if (eofFlag == false)
            {
                //Tokenize the string
				errorCode = LTKStringUtil::tokenizeString(line,  LIST_FILE_DELIMITER,  tokens);
				
				if( errorCode != SUCCESS )
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
						" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
					listFileHandle.close();
					mdtFileHandle.close();
					LTKReturnError(errorCode);
				}
				
				
                //Tokens must be of size 2, one is pathname and other is shapeId
                //If the end of file not reached then continue the looping
                if( tokens.size() != 2 && eofFlag == false )
                    continue;
				
                //Tokens[0] indicates the path name
                path = tokens[0];
				
                //Tokens[1] indicates the shapeId
                shapeId = atoi( tokens[1].c_str() );
				
                if(shapeId < 0)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
                        "The ActiveDTWShapeRecognizer requires training file class Ids to be positive integers and listed in the increasing order"<<
                        " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
                    errorCode = EINVALID_SHAPEID;
                    break;
                }
                else if(shapeId < prevClassId)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
                        "Shape IDs in the train list file should be in the increasing order. Please use scripts/validateListFile.pl to generate list files." <<
                        " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
                    errorCode = EINVALID_ORDER_LISTFILE;
                    break;
                }
				
				
                //This condition is used to check the first shape must be start from 0
                if( initClassFlag == false )
                {
                    initClassFlag = true;
                    prevClassId=shapeId;
                }
            }
        }
        else //Sample of a new class; do not read the next line during this iteration
        {
            //flag unset to read next line during the next iteration
            lastshapeIdFlag = false;
        }
		
        // Sample from the same class, extract features, and push the extracted features to shapeSamplesVec
        if( shapeId == prevClassId && ! path.empty())
        {
            if( getShapeFeatureFromInkFile(path, shapeFeature) != SUCCESS )
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
                    "Error extracting features from the ink file: " <<
                    path << ", extracting features from the next sample."<<
                    " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
                continue;
            }
			
            shapeSampleFeatures.setFeatureVector(shapeFeature);
            shapeSampleFeatures.setClassID(shapeId);
			
            ++sampleCount;
            shapeSamplesVec.push_back(shapeSampleFeatures);
			
            shapeFeature.clear();
			
        }
		
        // Sample of new class seen, or end of list file reached; train all the samples of previous class ID
        if( shapeId != prevClassId || eofFlag == true )
        {
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
                "Training for class : " << prevClassId << endl;
			
            //Increase shape count only if there are atleast one sample per class
            if( sampleCount > 0 )
                shapeCount++;
			
            //check that shapecount must not be greater than specified number
            //of shapes, if projecttype was not dynamic
            if( !m_projectTypeDynamic && shapeCount > m_numShapes )
            {
                errorCode = EINVALID_NUM_OF_SHAPES;
                break;
            }
			
            if( shapeCount > 0 && sampleCount > 0 )
            {
				
				/** PERFORM CLUSTERING **/
				
                
				
				errorCode = performClustering(shapeSamplesVec,clusterIndices);
				
				
				
				if( errorCode != SUCCESS )
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                        " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
					
                    listFileHandle.close();
                    mdtFileHandle.close();
                    LTKReturnError(errorCode);
                }
				
				
				
				
				int2DVector::iterator iter = clusterIndices.begin();
				int2DVector::iterator iEnd = clusterIndices.end();
				
				
				
				/**ITERATING THROUGH THE VARIOUS CLUSTERS **/
				for(;iter != iEnd; ++iter)
				{
					cluster = (*iter);
					
					
					
					/** SINGLETON VECTORS **/
					if(cluster.size() < m_minClusterSize)
					{
						/**CONSTRUCTING THE SINGLETON VECTOR**/
						for(int i = 0; i < cluster.size(); i++)
							singletonVector.push_back(shapeSamplesVec[cluster[i]].getFeatureVector());
					}
					
					/** CLUSTER PROCESSING **/
					else
					{
						//gather all the shape samples pertaining to a particular cluster
						int clusterSize = cluster.size();
						int i = 0;
						for( i = 0; i < clusterSize; i++)
						{
							//CONVERT ALL THE SHAPE SAMPLES TO FLOAT FEATURE VECTORS 
							floatVector floatFeatureVector;
							errorCode = m_shapeRecUtil.shapeFeatureVectorToFloatVector(shapeSamplesVec[cluster[i]].getFeatureVector(),
								floatFeatureVector);
							
							if (errorCode != SUCCESS)
							{
								LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
									" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
								
								LTKReturnError(errorCode);
							}
							
							int floatFeatureVectorSize = floatFeatureVector.size();
							for(int i = 0; i < floatFeatureVectorSize; i++)
								tempFeature.push_back(floatFeatureVector[i]);
							
							featureMatrix.push_back(tempFeature);
							tempFeature.clear();
							floatFeatureVector.clear();
							
						}
						
						
						/** COMPUTING COVARIANCE MATRIX **/
						errorCode = computeCovarianceMatrix(featureMatrix,covarianceMatrix,clusterMean);
						
						if(errorCode !=  SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
								" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						//setting cluster mean for cluster model
						
						clusterModel.setClusterMean(clusterMean);
						
						
						/** COMPUTING EIGEN VECTORS **/
						//computes the eigen vector for the larger covarianceMatrix
						//from the smaller covarianceMatrix
						errorCode = computeEigenVectorsForLargeDimension(featureMatrix,covarianceMatrix,eigenVectors,eigenValues);
						
						if(errorCode !=  SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
								" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						
						doubleVector tempEigenVector;
						int eigenVectorDimension = eigenVectors.size();
						
						if(eigenVectorDimension <= 0)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EEMPTY_EIGENVECTORS << " " <<
								" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
							
							LTKReturnError(EEMPTY_EIGENVECTORS);
						}
						
						int numEigenVectors = eigenVectors[0].size();
						
						if(numEigenVectors <= 0)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_NUM_OF_EIGENVECTORS << " " <<
								" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
							
							LTKReturnError(EINVALID_NUM_OF_EIGENVECTORS );
						}
						
						for( i = 0; i < numEigenVectors; i++)
						{
							for(int j = 0; j < eigenVectorDimension; j++)
								tempEigenVector.push_back(eigenVectors[j][i]);
							
							selectedEigenVectors.push_back(tempEigenVector);
							tempEigenVector.clear();
						}
						
						
						
						/**CONSTRUCTING CLUSTER MODEL **/
						errorCode = clusterModel.setNumSamples(cluster.size());
						if(errorCode != SUCCESS)
						{
							LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
								" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
							
							LTKReturnError(errorCode);
						}
						
						
						clusterModel.setEigenValues(eigenValues);
						
						clusterModel.setEigenVectors(selectedEigenVectors);
						
						
						/**APPENDING THE CLUSTER MODEL VECTOR**/
						clusterModelVector.push_back(clusterModel);
						
						featureMatrix.clear();
						covarianceMatrix.clear();
						selectedEigenVectors.clear();
						clusterMean.clear();
						eigenValues.clear();
						eigenVectors.clear();
			}
		}
		
		
		clusterIndices.clear();	
		
		
		
		/**CONSTRUCTING SHAPE MODEL**/
		
		errorCode = shapeModel.setShapeId(shapeCount - 1);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " "<< endl;
			LTKReturnError(errorCode);
		}
		
		shapeModel.setClusterModelVector(clusterModelVector);
		
		shapeModel.setSingletonVector(singletonVector);
		
		
		clusterModelVector.clear();
		singletonVector.clear();
		
		
		if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0)
		{
			
			//Writing results to the MDT file
			
			errorCode = appendShapeModelToMDTFile(shapeModel, mdtFileHandle);
			
			
			if( errorCode != SUCCESS )
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<errorCode << " " <<
					" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
				
				
				listFileHandle.close();
				mdtFileHandle.close();
				LTKReturnError(errorCode);
			}
			
			
		}
		
		//Clearing the shapeSampleVector and clusteredShapeSampleVector
		
		
		shapeSamplesVec.clear();
		
		//Resetting sampleCount for the next class
		sampleCount = 0;
		
		//Set the flag so that the already read line of next class in the list file is not lost
		lastshapeIdFlag = true;
		
		prevClassId = shapeId;
		
            }
        }
    }//End of while
	
    //Closing the Train List file and Model Data file
    listFileHandle.close();
    mdtFileHandle.close();
	
    if(!m_projectTypeDynamic && shapeCount != m_numShapes)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_SHAPES << " " <<
            getErrorMessage(EINVALID_NUM_OF_SHAPES)<<
            " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(EINVALID_NUM_OF_SHAPES);
    }
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(errorCode);
		
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
	
    return SUCCESS;
	
}


/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: computeCovarianceMatrix
* DESCRIPTION	        : This method computes the covariance matrix  and mean of
a feature matrix
* ARGUMENTS		: INPUT
featureMatrix double2DVector  Feature matrix whose covarianceMatrix 
is to be computed
;OUTPUT
covarianceMatrix double2DVector covarianceMatrix of feature matrix
meanFeature doubleVector mean of feature matrix

 * RETURNS		: integer Holds error value if occurs
 *						  Holds SUCCESS if no errors
 *************************************************************************************/
 int ActiveDTWShapeRecognizer::computeCovarianceMatrix(double2DVector& featureMatrix,
					double2DVector& covarianceMatrix,doubleVector& meanFeature)
 {
	 LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		 "ActiveDTWShapeRecognizer::computeCovarianceMatrix()" << endl;
	 
	 if(featureMatrix.empty())
	 {
		 LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_FEATUREMATRIX << " " <<
            	getErrorMessage(EEMPTY_FEATUREMATRIX) <<
				" ActiveDTWShapeRecognizer::computeCovarianceMatrix()" << endl;
		 
        	LTKReturnError(EEMPTY_FEATUREMATRIX);
	 }
	 
	 
	 doubleVector tempVector;
	 double mean;
	 
	 int numberOfRows;
	 int numberOfColumns;
	 
	 //rows
	 numberOfRows = featureMatrix.size();
	 //cols 
	 numberOfColumns = featureMatrix[0].size(); 
	 
	 
	 int i = 0;
	 int j = 0;
	 /***********CALCULATING MEAN*********/
	 for(i = 0; i < numberOfColumns;i++)
	 {
		 mean = 0.0;
		 for(j = 0;j < numberOfRows;j++)
		 {
			 mean = mean +  featureMatrix[j][i];
		 }
		 mean = mean /numberOfRows;
		 meanFeature.push_back(mean);
	 }
	 
	 /**********MEAN CORRECTED DATA************/
	 for( i = 0; i < numberOfRows; i++)
	 {
		 for(j = 0; j < numberOfColumns; j++)
		 {
			 featureMatrix[i][j] = featureMatrix[i][j] - meanFeature[j];
		 }
	 }
	 
	 /** COMPUTING COVARIANCE MATRIX**/
	 tempVector.assign(numberOfColumns,0.0);
	 covarianceMatrix.assign(numberOfColumns,tempVector);
	 tempVector.clear();
	 
	 bool bcovarianceMatrixFlag = false;
	 for( i = 0; i < numberOfColumns; i++)
	 {
		 for(j = 0;j < numberOfColumns; j++)
		 {
			 if(i <= j)
			 {
				 for(int k = 0; k < numberOfRows; k++) 
				 {
					 
					 covarianceMatrix[i][j] += featureMatrix[k][i]*featureMatrix[k][j];
					 
				 }
				 covarianceMatrix[i][j] = covarianceMatrix[i][j] /(numberOfRows - 1);
			 }
			 else
			 {
				 covarianceMatrix[i][j] = covarianceMatrix[j][i];
			 }
			 if(covarianceMatrix[i][j] != 0.0)
				bcovarianceMatrixFlag = true;
		 }
	 }	
	 
	 if(!bcovarianceMatrixFlag)
	 {
		 LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_COVARIANCEMATRIX << " " <<
            	getErrorMessage(EEMPTY_COVARIANCEMATRIX) <<
				" ActiveDTWShapeRecognizer::computeCovarianceMatrix()" << endl;
		 
        	LTKReturnError(EEMPTY_COVARIANCEMATRIX);
	 }
	 
	 
	 LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
		 "ActiveDTWShapeRecognizer::computeCovarianceMatrix()" << endl;
	 
	 return SUCCESS;
 }
 
 
 /**********************************************************************************
 * AUTHOR		: S Anand
 * DATE			: 3-MAR-2009
 * NAME			: computeEigenVectors
 * DESCRIPTION	        : This method computes the eigenValues and eigenVectors of
 a covarianceMatrix
 * ARGUMENTS		: INPUT
 covarianceMatrix double2DVector (matrix whose eigenValues and eigenVectors are to be computed)
 rank int  (rank of covarianceMatrix) 
 nrot int  (number of rotations) This a parameter used by the algorithm
 OUTPUT 
 eigenValueVec doubleVector Eigen values of covarianceMatrix 
 eigenVectorMatrix double2DVector Eigen vectors of covarianceMatrix 
 
  * RETURNS		: integer Holds error value if occurs
  *						  Holds SUCCESS if no erros
  * NOTES			:
  * CHANGE HISTROY
  * Author			Date				Description
  *************************************************************************************/
  int ActiveDTWShapeRecognizer::computeEigenVectors(double2DVector &covarianceMatrix,const int rank,
	  doubleVector &eigenValueVec, double2DVector &eigenVectorMatrix, int& nrot)
  {
	  
	  
	  LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		  "ActiveDTWShapeRecognizer::computeEigenVectors()" << endl;
	  
	  if(covarianceMatrix.empty())
	  {
		  LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_COVARIANCEMATRIX << " " <<
			  getErrorMessage(EEMPTY_COVARIANCEMATRIX) <<
			  " ActiveDTWShapeRecognizer::computeEigenVectors()" << endl;
		  
		  LTKReturnError(EEMPTY_COVARIANCEMATRIX);
	  }
	  
	  if(rank <= 0)
	  {
		  LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_RANK << " " <<
			  getErrorMessage(EINVALID_RANK) <<
			  " ActiveDTWShapeRecognizer::computeEigenVectors()" << endl;
		  
		  LTKReturnError(EINVALID_RANK);
	  }
	  
	  int MAX_ITER = 1000;
	  
	  int ip,count,p,q,r;
	  
	  double theta, tau, t,sm,s,h,g,c;
	  
	  double app,aqq,apq,vpp,vqq,vpq;
	  
	  double2DVector::iterator  itrRow,itrRowEnd;
	  
	  doubleVector::iterator itrCol,itrColEnd;
	  
	  itrRowEnd = eigenVectorMatrix.end();
	  
	  for(ip = 0,itrRow = eigenVectorMatrix.begin(); itrRow <itrRowEnd;++ip, ++itrRow)
	  {
		  itrColEnd = (*itrRow).end();
		  
		  for(itrCol = (*itrRow).begin(); itrCol < itrColEnd; ++itrCol)
		  {
            		*itrCol = 0;
		  }
		  
		  *((*itrRow).begin() + ip) = 1;
		  
		  eigenValueVec.push_back(0.0);
	  }
	  
	  nrot = 0;
	  
	  for(count = 0; count < MAX_ITER; count++)
	  {
		  nrot++;
		  
		  sm = 0.0;
		  
		  itrRowEnd = covarianceMatrix.end();
		  
		  for(p = 0,itrRow = covarianceMatrix.begin(); itrRow <itrRowEnd;++p, ++itrRow)
		  {
            		itrColEnd = (*itrRow).end();
					
					for(itrCol = (*itrRow).begin()+p+1; itrCol < itrColEnd; ++itrCol)
					{
						sm += fabs(*itrCol);
					}
		  }
		  
		  if(sm < EPS)
		  {
            		for(r=0;r<rank;r++)
					{
						eigenValueVec[r] = covarianceMatrix[r][r];
					}
					
		  }
		  
		  for(p=0; p<(rank-1) ; p++)
		  {
            		for(q=(p+1); q<rank; q++)
					{
						if(fabs(covarianceMatrix[p][q])>EPS1)
						{
							theta = (covarianceMatrix[q][q] - covarianceMatrix[p][p])/
								(2*covarianceMatrix[p][q]);
							
							t = sqrt(1+theta * theta) - theta;
							
							c = 1/(sqrt(1+t*t));
							
							s = t*c;
							
							tau = s/(1+c);
							
							apq = covarianceMatrix[p][q];
							
							app = covarianceMatrix[p][p];
							
							aqq = covarianceMatrix[q][q];
							
							vpp = eigenVectorMatrix[p][p];
							
							vpq = eigenVectorMatrix[p][q];
							
							vqq = eigenVectorMatrix[q][q];
							
							for(r=0;r<p;r++)
							{
								h = covarianceMatrix[r][p];
								
								g = covarianceMatrix[r][q];
								
								covarianceMatrix[r][p] = (double)(c*h - s*g);
								
								covarianceMatrix[r][q] = (double)(c*g + s*h);
							}
							covarianceMatrix[p][p] -= (double)(t*apq);
							
							covarianceMatrix[p][q] = (double)(0.0);
							
							for(r=p+1; r<q; r++)
							{
								h = covarianceMatrix[p][r];
								
								g = covarianceMatrix[r][q];
								
								covarianceMatrix[p][r] = (double)(c*h - s*g);
								
								covarianceMatrix[r][q] = (double)(c*g + s*h);
							}
							
							covarianceMatrix[q][q] += (double)(t*apq);
							
							for(r = q+1; r<rank; r++)
							{
								h = covarianceMatrix[p][r];
								
								g = covarianceMatrix[q][r];
								
								covarianceMatrix[p][r] = (double)(c*h - s*g);
								
								covarianceMatrix[q][r] = (double)(c*g + s*h);
							}
							
							for(r = 0; r<rank ; r++)
							{
								h = eigenVectorMatrix[r][p];
								
								g = eigenVectorMatrix[r][q];
								
								eigenVectorMatrix[r][p] = (double)(c*h - s*g);
								
								eigenVectorMatrix[r][q] = (double)(c*g + s*h);
							}
						}
						else
						{
							covarianceMatrix[p][q] = 0;
						}
					}
		  }
		  
		}
		
		
		
		for(r=0;r<rank;r++)
		{
			eigenValueVec[r] = covarianceMatrix[r][r];
		}
		
		double temp;
		
		for(p = 0; p<rank-1; p++)
		{
			for(q = p+1; q<rank; q++)
			{
				if(fabs(eigenValueVec[p]) < fabs(eigenValueVec[q]))
				{
					for(r = 0; r<rank; r++)
					{
						temp = eigenVectorMatrix[r][p];
						
						eigenVectorMatrix[r][p]  = eigenVectorMatrix[r][q];
						
						eigenVectorMatrix[r][q]  = temp;
                        
					}
					
					temp = eigenValueVec[p];
					
					eigenValueVec[p] = eigenValueVec[q];
					
					eigenValueVec[q] = temp;
					
				}
			}
		}
		
		
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
			"ActiveDTWShapeRecognizer::computeEigenVectors()" << endl;
		
		return SUCCESS;
		
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: initializeFeatureExtractorInstance
* DESCRIPTION	: This method get the Instance of the Feature Extractor
*				  from LTKShapeFeatureExtractorFactory
* ARGUMENTS		:
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int ActiveDTWShapeRecognizer::initializeFeatureExtractorInstance(const LTKControlInfo& controlInfo)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"ActiveDTWShapeRecognizer::initializeFeatureExtractorInstance()" << endl;
	
	
	LTKShapeFeatureExtractorFactory factory;
	int errorCode = factory.createFeatureExtractor(m_featureExtractorName,
		m_lipiRootPath,
		m_lipiLibPath,
		&m_libHandlerFE,
		controlInfo,
		&m_ptrFeatureExtractor);
	
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EFTR_EXTR_NOT_EXIST << " " <<
			" ActiveDTWShapeRecognizer::initializeFeatureExtractorInstance()" << endl;
		LTKReturnError(errorCode);
	}
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
		"ActiveDTWShapeRecognizer::initializeFeatureExtractorInstance()" << endl;
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: deleteFeatureExtractorInstance
* DESCRIPTION	: This method unloads the Feature extractor instance
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int ActiveDTWShapeRecognizer::deleteFeatureExtractorInstance()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::deleteFeatureExtractorInstance()" << endl;
	
    if (m_ptrFeatureExtractor != NULL)
    {
        typedef int (*FN_PTR_DELETE_SHAPE_FEATURE_EXTRACTOR)(LTKShapeFeatureExtractor *obj);
        FN_PTR_DELETE_SHAPE_FEATURE_EXTRACTOR deleteFeatureExtractor;
		void * functionHandle = NULL;
		
		// Map createpreprocessor and deletePreprocessor functions
        int returnVal = m_OSUtilPtr->getFunctionAddress(m_libHandlerFE, 
			DELETE_SHAPE_FEATURE_EXTRACTOR, 
			&functionHandle);
		
        // Could not map the createLipiPreprocessor function from the DLL
		if(returnVal != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << 
                "Error: "<< EDLL_FUNC_ADDRESS_DELETE_FEATEXT << " " <<
                getErrorMessage(EDLL_FUNC_ADDRESS_DELETE_FEATEXT) <<
                " ActiveDTWShapeRecognizer::deleteFeatureExtractorInstance()" << endl;
			
            LTKReturnError(EDLL_FUNC_ADDRESS_DELETE_FEATEXT);
		}
		
        deleteFeatureExtractor = (FN_PTR_DELETE_SHAPE_FEATURE_EXTRACTOR)functionHandle;
		
        deleteFeatureExtractor(m_ptrFeatureExtractor);
		
        m_ptrFeatureExtractor = NULL;
		
		// unload feature extractor dll
        if(m_libHandlerFE != NULL)
		{
            //Unload the DLL
			m_OSUtilPtr->unloadSharedLib(m_libHandlerFE);
			m_libHandlerFE = NULL;
			
		}
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::deleteFeatureExtractorInstance()" << endl;
	
    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: updateHeaderWithAlgoInfo
* DESCRIPTION	: This method will Update the Header information for the MDT file
* ARGUMENTS		:
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
void ActiveDTWShapeRecognizer::updateHeaderWithAlgoInfo()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;
	
    m_headerInfo[RECVERSION] = m_currentVersion;
    string algoName = ACTIVEDTW;
    m_headerInfo[RECNAME] = algoName;
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;
	
}


/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getDistance
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
void ActiveDTWShapeRecognizer::getDistance(const LTKShapeFeaturePtr& f1,
										  const LTKShapeFeaturePtr& f2,
										  float& distance)
{
    f1->getDistance(f2, distance);
}



/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getFeaturesFromTraceGroup
* DESCRIPTION	: 1. PreProcess 2. Extract Features 3.Add to PrototypeSet
*				  4. Add to MDT
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/

int ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup(const LTKTraceGroup& inTraceGroup,
														   vector<LTKShapeFeaturePtr>& featureVec)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
	
    //TraceGroup after Preprocessing
    LTKTraceGroup preprocessedTraceGroup; 	
	
    //Check for empty traces in inTraceGroup
    if(inTraceGroup.containsAnyEmptyTrace())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_TRACE << " " <<
            getErrorMessage(EEMPTY_TRACE) <<
            " ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(EEMPTY_TRACE);
    }
	
    //Preprocess the inTraceGroup
	int errorCode = preprocess(inTraceGroup, preprocessedTraceGroup);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(errorCode);
    }
	
    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
		featureVec);
	
    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(errorCode);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting ActiveDTWShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: getTraceGroup
* DESCRIPTION	        : Returns the tracegroups of the shape
*                        For singletons it returns the tracegroups from the feature vector
*                        For clusters it returns the tracegroup of the cluster mean 
* ARGUMENTS		: INPUT:
shapeID - class  whose trace groups are to be returned
numberOfTraceGroups - maximum number of trace groups to be returned
: OUTPUT
outTraceGroups - tracegroups of the class   
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/

int ActiveDTWShapeRecognizer::getTraceGroups(int shapeID, int numberOfTraceGroups,
											 vector<LTKTraceGroup> &outTraceGroups)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Entering ActiveDTWShapeRecognizer::getTraceGroups"
        <<endl;
	
    if(m_shapeIDNumPrototypesMap.find(shapeID) == m_shapeIDNumPrototypesMap.end())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_SHAPEID << " " <<
            getErrorMessage(EINVALID_SHAPEID) <<
            " ActiveDTWShapeRecognizer::getTraceGroups()" << endl;
        LTKReturnError(EINVALID_SHAPEID);
    }
	
    if(m_shapeIDNumPrototypesMap[shapeID] < numberOfTraceGroups)
    {
        numberOfTraceGroups = m_shapeIDNumPrototypesMap[shapeID];
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)
            << "Number of TraceGroup in PrototypeShapes is less than specified."
            << "Returning all TraceGroups :"
            << numberOfTraceGroups <<endl;
    }
	
    vector<ActiveDTWShapeModel>::iterator prototypeShapesIter = m_prototypeShapes.begin();
    int counter =0;
    vector<ActiveDTWClusterModel> clusterModelVector;
    shapeMatrix singletonVector;
	
    for(;prototypeShapesIter!=m_prototypeShapes.end();)
    {
        int currentShapeId = (*prototypeShapesIter).getShapeId();
		
        if(currentShapeId == shapeID)
        {
            LTKTraceGroup traceGroup;
			
			clusterModelVector = (*prototypeShapesIter).getClusterModelVector();
			singletonVector = (*prototypeShapesIter).getSingletonVector();
			
			int clusterModelVectorSize = clusterModelVector.size();
			int singletonVectorSize = singletonVector.size();
			
			if(singletonVectorSize > 0)
			{
				//convert singletons into tracegroups
				for(int i = 0; i < singletonVectorSize; i++)
				{
					//converting the featureVector to traceGroup
					int errorCode = m_ptrFeatureExtractor->convertFeatVecToTraceGroup(
						singletonVector[i],traceGroup);
					
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
							" ActiveDTWShapeRecognizer::getTraceGroups()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					outTraceGroups.push_back(traceGroup);
					counter++;
					if(counter==numberOfTraceGroups)
						break;
				}
			}
			
			
			if(clusterModelVectorSize > 0)
			{
				//converting all the cluster means into traceGroups
				for(int i = 0; i < clusterModelVectorSize; i++)
				{
					doubleVector clusterMean = clusterModelVector[i].getClusterMean();
					
					//converting the doubleVector to featureVector
					vector<LTKShapeFeaturePtr> shapeFeatureVec;
					
					int errorCode = convertDoubleToFeatureVector(shapeFeatureVec,clusterMean);
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
							" ActiveDTWShapeRecognizer::getTraceGroups()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					//converting the featureVector to traceGroup
					errorCode = m_ptrFeatureExtractor->convertFeatVecToTraceGroup(
						shapeFeatureVec,traceGroup);
					
					if(errorCode != SUCCESS)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
							" ActiveDTWShapeRecognizer::getTraceGroups()" << endl;
						
						LTKReturnError(errorCode);
					}
					
					outTraceGroups.push_back(traceGroup);
					clusterMean.clear();
					counter++;
					if(counter==numberOfTraceGroups)
						break;
				}
			}
		}
        prototypeShapesIter++;
    }
	
    //clearing vectors
    clusterModelVector.clear();
    singletonVector.clear();
	
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting ActiveDTWShapeRecognizer::getTraceGroups"
        <<endl;
    return SUCCESS;
}

/***********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: initializePreprocessor
* DESCRIPTION	: This method is used to initialize the PreProcessor
* ARGUMENTS		: preprocDLLPath : string : Holds the Path of the Preprocessor DLL,
*				  returnStatus    : int    : Holds SUCCESS or Error Values, if occurs
* RETURNS		: preprocessor instance
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::initializePreprocessor(const LTKControlInfo& controlInfo,
													 LTKPreprocessorInterface** preprocInstance)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
	
    FN_PTR_CREATELTKLIPIPREPROCESSOR createLTKLipiPreProcessor = NULL;
    
    int errorCode;
	
    // Load the DLL with path=preprocDLLPath
    void* functionHandle = NULL;
	
    int returnVal = m_OSUtilPtr->loadSharedLib(controlInfo.lipiLib, PREPROC, &m_libHandler);
    
    if(returnVal != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  ELOAD_PREPROC_DLL << " " <<
            getErrorMessage(ELOAD_PREPROC_DLL) <<
            " ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(ELOAD_PREPROC_DLL);
    }
	
    // Map createpreprocessor and deletePreprocessor functions
    returnVal = m_OSUtilPtr->getFunctionAddress(m_libHandler, 
		CREATEPREPROCINST, 
		&functionHandle);
    // Could not map the createLipiPreprocessor function from the DLL
    if(returnVal != SUCCESS)
    {
        //Unload the dll
        unloadPreprocessorDLL();
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EDLL_FUNC_ADDRESS_CREATE << " " <<
            getErrorMessage(EDLL_FUNC_ADDRESS_CREATE) <<
            " ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
		
        LTKReturnError(EDLL_FUNC_ADDRESS_CREATE);
    }
	
	createLTKLipiPreProcessor = (FN_PTR_CREATELTKLIPIPREPROCESSOR)functionHandle;
	
	functionHandle = NULL;
	
    // Map createpreprocessor and deletePreprocessor functions
    returnVal = m_OSUtilPtr->getFunctionAddress(m_libHandler, 
		DESTROYPREPROCINST, 
		&functionHandle);
	
    // Could not map the createLipiPreprocessor function from the DLL
    if(returnVal != SUCCESS)
    {
        //Unload the dll
        unloadPreprocessorDLL();
		
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EDLL_FUNC_ADDRESS_CREATE << " " <<
            getErrorMessage(EDLL_FUNC_ADDRESS_CREATE) <<
            " ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(EDLL_FUNC_ADDRESS_CREATE);
    }
    
    m_deleteLTKLipiPreProcessor = (FN_PTR_DELETELTKLIPIPREPROCESSOR)functionHandle;
	
    // Create preprocessor instance
    errorCode = createLTKLipiPreProcessor(controlInfo, preprocInstance);
	
    if(errorCode!=SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  errorCode << " " <<
            " ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(errorCode);
    }
	
    // Could not create a LTKLipiPreProcessor
    if(*preprocInstance == NULL)
    {
        // Unload the DLL
        unloadPreprocessorDLL();
		
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ECREATE_PREPROC << " " <<
            getErrorMessage(ECREATE_PREPROC) <<
            " ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(ECREATE_PREPROC);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::initializePreprocessor()" << endl;
	
    return SUCCESS;
	
    
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: deletePreprocessor
* DESCRIPTION	: This method is used to deletes the PreProcessor instance
* ARGUMENTS		: ptrPreprocInstance : Holds the pointer to the LTKPreprocessorInterface
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/

//int ActiveDTWShapeRecognizer::deletePreprocessor(LTKPreprocessorInterface *ptrPreprocInstance)
int ActiveDTWShapeRecognizer::deletePreprocessor()
{
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::deletePreprocessor()" << endl;
	
    //deleting the preprocessor instance
    if(m_ptrPreproc != NULL)
    {
        m_deleteLTKLipiPreProcessor(m_ptrPreproc);
        m_ptrPreproc = NULL;
    }
	
    //Unload the dll
    int returnStatus = unloadPreprocessorDLL();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
            getErrorMessage(returnStatus) <<
            " ActiveDTWShapeRecognizer::deletePreprocessor()" << endl;
        LTKReturnError(returnStatus);
    }
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::deletePreprocessor()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: unloadPreprocessorDLL
* DESCRIPTION	: This method is used to Unloads the preprocessor DLL.
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::unloadPreprocessorDLL()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::unloadPreprocessorDLL()" << endl;
	
    //Check the preprocessor DLL was loaded already
    if(m_libHandler != NULL)
    {
        //Unload the DLL
		m_OSUtilPtr->unloadSharedLib(m_libHandler);
        m_libHandler = NULL;
    }
    
	
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::unloadPreprocessorDLL()" << endl;
	
    return SUCCESS;
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: findOptimalDeformation
* DESCRIPTION	        : This method computes the parameters required to construct the optimal
deformation of each cluster that is closest to the test sample
* ARGUMENTS		: INPUT
eigenValues doubleVector   (eigenValues of cluster) 
eigenVector double2DVector (eigenVectors of cluster)
clusterMean doubleVector   (mean of cluster)
testSample doubleVector    (test sample)
:OUTPUT
deformationParameters doubleVector (deformation parameters)
*				  
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::findOptimalDeformation(doubleVector& deformationParameters,doubleVector& eigenValues, double2DVector& eigenVector,
													 doubleVector& clusterMean, doubleVector& testSample)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
	
	if(eigenValues.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_EIGENVALUES << " " <<
			getErrorMessage(EEMPTY_EIGENVALUES) <<
			" ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
		
		LTKReturnError(EEMPTY_EIGENVALUES);
	}
	
	if(eigenVector.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_EIGENVECTORS << " " <<
			getErrorMessage(EEMPTY_EIGENVECTORS) <<
			" ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
		
		LTKReturnError(EEMPTY_EIGENVECTORS);
	}
	
	if(clusterMean.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_CLUSTERMEAN << " " <<
			getErrorMessage(EEMPTY_CLUSTERMEAN) <<
			" ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
		
		LTKReturnError(EEMPTY_CLUSTERMEAN);
	}
	
	if(eigenValues.size() != eigenVector.size())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENUM_EIGVALUES_NOTEQUALTO_NUM_EIGVECTORS << " " <<
			getErrorMessage(ENUM_EIGVALUES_NOTEQUALTO_NUM_EIGVECTORS) <<
			" ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
		
		LTKReturnError(ENUM_EIGVALUES_NOTEQUALTO_NUM_EIGVECTORS);
	}
	
	
	doubleVector diffVec;
	doubleVector linearConstant;
	doubleVector tempDoubleVec;
	doubleVector lowerBounds;
	doubleVector upperBounds;
	double tempSum;
	
    int n ;
	
	int i = 0;
	
	
	
	/***************CALCULATING PARAMETERS***********************/
	/**CALCULATING THE DIFFERENCE VECTOR**/
	diffVec.assign(clusterMean.size(),0.0);
	
	for(i = 0; i < diffVec.size(); i++)
		diffVec[i] = testSample[i] - clusterMean[i];
	
	
	/**CALCULATING THE LINEAR CONSTANT TERM**/
	//linearConstant is calculated as:
	//linearConstant = eigVec*diffVec
	double2DVector::iterator iStart = eigenVector.begin();
	double2DVector::iterator iEnd = eigenVector.end();
	
	for(;iStart != iEnd;++iStart)
	{
		tempDoubleVec = (*iStart);
		
		tempSum = 0.0;
		for(i = 0; i < tempDoubleVec.size(); i++ )
		{
			
			tempSum += (tempDoubleVec[i] * diffVec[i]);
		}
		
		linearConstant.push_back(tempSum);
	}	
	
	//problem dimension	
	n = eigenVector.size();                            
	
	
	
	double tempBounds;
	
	for( i = 0; i < n; i++)
	{
		
		tempBounds = sqrt(eigenValues[i] * m_eigenSpreadValue);
		lowerBounds.push_back((-1) * tempBounds);
		upperBounds.push_back(tempBounds);
	}
	
	/**OPTIMIZATION PROCEDURE**/
	for( i = 0; i < n; i++)
	{
		if(linearConstant[i] >= lowerBounds[i] && linearConstant[i] <= upperBounds[i])
			deformationParameters[i] = linearConstant[i];
		else
		{
			if(linearConstant[i] < lowerBounds[i])
				deformationParameters[i] = lowerBounds[i];
			else
				deformationParameters[i] = upperBounds[i];
		}
	}
	
	//clearing vectors
	linearConstant.clear();
	lowerBounds.clear();
	upperBounds.clear();
	diffVec.clear();
	tempDoubleVec.clear();
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::findOptimalDeformation()" << endl;
	return SUCCESS;
};

/**************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: validatePreprocParameters
* DESCRIPTION	: This method is used to validate the preproc parameters with
*						mdt header values
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
****************************************************************************/
int ActiveDTWShapeRecognizer::validatePreprocParameters(stringStringMap& headerSequence)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::validatePreprocParameters()" << endl;
	
    
    string tempStrVar = "";
    string headerValue = "";
    int headerValueInt   = 0;
    float headerValueFloat = 0.0f;
    int tempIntegerValue = 0;
    float tempFloatValue = 0.0f;
    
    //preproc sequence
    string mdtPreprocSeqn = headerSequence[PREPROC_SEQ];
    if(LTKSTRCMP(m_preProcSeqn.c_str(), mdtPreprocSeqn.c_str()) != 0 &&
		LTKSTRCMP("NA", mdtPreprocSeqn.c_str()) != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of preprocSeqn in config file ("<<
            m_preProcSeqn <<") does not match with the value in MDT file ("<<
            mdtPreprocSeqn <<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    
	
    //ResampTraceDimension
    headerValue = "";
	if(LTKSTRCMP("NA", headerSequence[TRACE_DIM].c_str()) != 0)
    {
		headerValueInt = atoi(headerSequence[TRACE_DIM].c_str());
		tempIntegerValue = m_ptrPreproc->getTraceDimension();
		
		if(headerValueInt != tempIntegerValue )
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of TraceDimension in config file ("<<
				tempIntegerValue<<") does not match with the value in MDT file ("<<
				headerValueInt <<")"<<
				" ActiveDTWShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}    
    
	
    // preserve aspect ratio
	bool preProcPreserveAspectRatio = m_ptrPreproc->getPreserveAspectRatio();
	tempStrVar = "false";
	if (preProcPreserveAspectRatio == true)
	{
		tempStrVar = "true";
	}
	
    if(LTKSTRCMP((headerSequence[PRESER_ASP_RATIO]).c_str(), tempStrVar.c_str()) != 0 &&
		LTKSTRCMP((headerSequence[PRESER_ASP_RATIO]).c_str(), "NA") != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of preProcPreserveAspectRatio in config file ("<<
            tempStrVar<<") does not match with the value in MDT file ("<<
            headerSequence[PRESER_ASP_RATIO] <<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
    
	
	//NormPreserveRelativeYPosition
	bool preProcNormPreserveRelativeYPosition = m_ptrPreproc->getPreserveRealtiveYPosition();
	tempStrVar = "false";
	if (preProcNormPreserveRelativeYPosition == true)
	{
		tempStrVar = "true";
	}
	
    if(LTKSTRCMP((headerSequence[PRESER_REL_Y_POS]).c_str(), tempStrVar.c_str()) != 0 &&
		LTKSTRCMP((headerSequence[PRESER_REL_Y_POS]).c_str(), "NA") != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of preProcNormPreserveRelativeYPosition in config file ("<<
            tempStrVar<<") does not match with the value in MDT file ("<<
            headerSequence[PRESER_REL_Y_POS] <<")"<<
            " ActiveDTWShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	
    
	
	// NormPreserveAspectRatioThreshold
	tempFloatValue = m_ptrPreproc->getAspectRatioThreshold();
	if(LTKSTRCMP((headerSequence[ASP_RATIO_THRES]).c_str(), "NA") != 0)
	{
        headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[ASP_RATIO_THRES]);
		
		if(headerValueFloat != tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcPreserveAspectRatioThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" ActiveDTWShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
	
	
	
	// NormLineWidthThreshold
	if(LTKSTRCMP((headerSequence[DOT_SIZE_THRES]).c_str(), "NA") != 0)
	{
        headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[DOT_SIZE_THRES]);
		tempFloatValue = m_ptrPreproc->getSizeThreshold();
		
		if(headerValueFloat !=  tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcNormLineWidthThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" ActiveDTWShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
	
	
	// NormDotSizeThreshold
	if(LTKSTRCMP((headerSequence[DOT_THRES]).c_str(), "NA") != 0)
	{
        headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[DOT_THRES]);
		tempFloatValue = m_ptrPreproc->getDotThreshold();
		
		if(headerValueFloat != tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcNormDotSizeThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" ActiveDTWShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
	
	
	
	//ResampPointAllocation
	tempStrVar = "";
	tempStrVar = m_ptrPreproc->getResamplingMethod();
	if(LTKSTRCMP((headerSequence[RESAMP_POINT_ALLOC]).c_str(), tempStrVar.c_str()) != 0 &&
		LTKSTRCMP((headerSequence[RESAMP_POINT_ALLOC]).c_str(), "NA") != 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
			"Value of preProcResampPointAllocation in config file ("<<
			tempStrVar<<") does not match with the value in MDT file ("<<
			headerSequence[RESAMP_POINT_ALLOC] <<")"<<
			" ActiveDTWShapeRecognizer::loadModelData()" << endl;
		LTKReturnError(ECONFIG_MDT_MISMATCH);
	}
	
	
	//SmoothWindowSize
	if(LTKSTRCMP((headerSequence[SMOOTH_WIND_SIZE]).c_str(), "NA") != 0)
	{
		headerValueInt = atoi(headerSequence[SMOOTH_WIND_SIZE].c_str());
		tempIntegerValue = m_ptrPreproc->getFilterLength();
		
		if(headerValueInt != tempIntegerValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcSmoothWindowSize in config file ("<<
				tempIntegerValue<<") does not match with the value in MDT file ("<<
				headerValueInt <<")"<<
				" ActiveDTWShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
	
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::validatePreprocParameters()" << endl;
	return SUCCESS;
	
}


/*********************************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: computeEigenVectorsForLargeDimension
* DESCRIPTION	        :  This method computes the eigen values and eigen vectors for larger
covariance matrices of large dimension using the eigen value and 
eigen vectors of the smaller covariance matrix
* ARGUMENTS		 :INPUT
meanCorrectedData double2DVector 
-- used for constructing the smaller covariance matrix
covarianceMatrix double2DVector
-- this is the larger covariance matrix whose eigen values and
eigen vectors are to be computed
:OUTPUT
eigenValues doubleVector
-- eigenValues of the larger covarianceMatrix
eigenVectors double2DVector
-- eigenVectors of the larger covarianceMatrix
*				  
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
* NOTES		: This based on a paper implementation
*************************************************************************************/
int ActiveDTWShapeRecognizer::computeEigenVectorsForLargeDimension(double2DVector& meanCorrectedData,double2DVector& covarianceMatrix,
																   double2DVector& eigenVector,doubleVector& eigenValues)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::computeEigenVectorsForLargeDimension()" << endl;
	
	if(meanCorrectedData.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_MEANCORRECTEDDATA << " " <<
			getErrorMessage(EEMPTY_MEANCORRECTEDDATA) <<
			" ActiveDTWShapeRecognizer::computeEigenVectorsForLargeDimension()" << endl;
		
		LTKReturnError(EEMPTY_MEANCORRECTEDDATA);
	}
	
	if(covarianceMatrix.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_COVARIANCEMATRIX << " " <<
			getErrorMessage(EEMPTY_COVARIANCEMATRIX) <<
			" ActiveDTWShapeRecognizer::computeEigenVectorsForLargeDimension()" << endl;
		
		LTKReturnError(EEMPTY_COVARIANCEMATRIX);
	}
	
	
	//covarianceSmall = A(transpose) * A
	double2DVector covarianceSmall;
	
	doubleVector tempVector;
	
	//eigen values and eigen vectors of A(transpose) * A
	double2DVector eigVecMat;
	doubleVector eigValVec;
	
	//number of iterations for computing eigen vectors
	int nrot = 0;
	
	int meanCorrectedDataSize = meanCorrectedData.size();
	int rowSize = meanCorrectedData[0].size();
	
	tempVector.assign(meanCorrectedDataSize,0.0);
	covarianceSmall.assign(meanCorrectedDataSize,tempVector);
	tempVector.clear();
	
	int i = 0;
	int j = 0;
	
	//calculating A(transpose)* A
	for(i = 0; i < meanCorrectedDataSize; i++)
	{
		for(j = 0;j < meanCorrectedDataSize; j++)
		{
			if(i <= j)
			{
				for(int k = 0; k < rowSize; k++) 
				{
					covarianceSmall[i][j] += meanCorrectedData[i][k]*meanCorrectedData[j][k];
				}
				covarianceSmall[i][j] = covarianceSmall[i][j] /(meanCorrectedDataSize -1);
			}
			else
			{
				covarianceSmall[i][j] = covarianceSmall[j][i];
			}
		}
	}
	
	
	//allocating memory for eigVecMat
	tempVector.assign(meanCorrectedDataSize,0.0);
	eigVecMat.assign(meanCorrectedDataSize,tempVector);
	tempVector.clear();	
	
	
	//computing the eigen vectors of A(transpose)*A
	
	
	int errorCode = computeEigenVectors(covarianceSmall,covarianceSmall.size(),
		eigValVec,eigVecMat,nrot);
	
	if(errorCode !=  SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
			" ActiveDTWShapeRecognizer::trainFromListFile()" << endl;
		LTKReturnError(errorCode);
	}
	
	
	
	
	
	/**CALCULATING EIGEN ENERGY **/
	double totalEigenEnergy = 0.0;
	double currentEigenEnergy = 0.0;
	int numEigenVectors = 0;
	
	int eigenValSize = eigValVec.size();
	for( i = 0; i < eigenValSize; i++)
		totalEigenEnergy += eigValVec[i];
	
	/**DETERMINING NUMBER OF EIGEN VECTORS**/
	while(currentEigenEnergy <= ((m_percentEigenEnergy*totalEigenEnergy)/100) && numEigenVectors < eigenValSize)
		currentEigenEnergy += eigValVec[numEigenVectors++];
	
	/**COMPUTING THE EIGEN VECTORS OF THE LARGER COVARIANCE MATRIX AS (A * EIGENVECTOR) **/
	
	tempVector.assign(numEigenVectors,0.0);
	eigenVector.assign(rowSize,tempVector);
	tempVector.clear();
	
	for( i = 0; i < rowSize; i++)
	{
		for(int j = 0; j < numEigenVectors; j++)
		{
			for(int k = 0; k < meanCorrectedDataSize; k++)
			{
				eigenVector[i][j] += meanCorrectedData[k][i] * eigVecMat[k][j]; 
			}  
		}
	}
	
	//calculating the magnitude of each eigenVectors
	doubleVector magnitudeVector;
	for( i = 0; i < numEigenVectors; i++)
	{
		double tempMagnitude = 0;
		for(j = 0; j < rowSize; j++)
			tempMagnitude += (eigenVector[j][i] * eigenVector[j][i]);
		double sqrtMagnitude = sqrt(tempMagnitude);
		magnitudeVector.push_back(sqrtMagnitude);
	}
	
	
	//normalizing eigenVectors
	for( i = 0; i < numEigenVectors; i++)
	{
		for(j = 0; j < rowSize; j++)
			eigenVector[j][i] = (eigenVector[j][i]/magnitudeVector[i]);
	}
	magnitudeVector.clear();
	
	//selecting the corresponding eigen values
	for( i = 0; i < numEigenVectors; i++)
		eigenValues.push_back(eigValVec[i]);
	
	eigVecMat.clear();
	eigValVec.clear();
	covarianceSmall.clear();
	
	
	return SUCCESS;	
	
	
}

/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: convertDoubleToFeatureVector
* DESCRIPTION	        : This method converts the double Vector (flatenned version of the feature vector)
to the vector<LTKShapeFeaturePtr> form 
* ARGUMENTS		: INPUT
*			       featureVec doubleVector
:OUTPUT
shapeFeatureVec vector<LTKShapeFeaturePtr>
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no erros
*************************************************************************************/
int ActiveDTWShapeRecognizer::convertDoubleToFeatureVector(vector<LTKShapeFeaturePtr>& shapeFeatureVec,doubleVector& featureVec)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::convertDoubleToFeatureVector" << endl;
	
	LTKShapeFeaturePtr shapeFeature;
	int featureVectorSize = featureVec.size();
	int featureDimension;
	floatVector tempFeature;
	
	for(int i = 0; i < featureVectorSize;)
	{
		shapeFeature = m_ptrFeatureExtractor->getShapeFeatureInstance();
		featureDimension = shapeFeature->getFeatureDimension();
		
		
		for(int j = 0; j <  featureDimension; j++)
		{
			tempFeature.push_back((float)featureVec[i]);
			i++;
		}
		
		
		if (shapeFeature->initialize(tempFeature) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_INPUT_FORMAT << " " <<
				"Number of features extracted from a trace is not correct"<<
				" ActiveDTWShapeRecognizer::convertDoubleToFeatureVector" << endl;
			
			LTKReturnError(EINVALID_INPUT_FORMAT);
		}
		shapeFeatureVec.push_back(shapeFeature);
		
		tempFeature.clear();
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::convertDoubleToFeatureVector" << endl;
	
    return SUCCESS;
	
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: adapt
* DESCRIPTION	: adapt recent recognized sample 
* ARGUMENTS		: shapeID : True shapeID of sample
* RETURNS		: Success : If sample was adapted successfully
*				  Failure : Returns Error Code
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::adapt(int shapeId)
{
	try{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
			<< "Enter ActiveDTWShapeRecognizer::adapt()"<<endl;
		
		//check if shapeId has already seen by classifier else add shapeId
		if(m_shapeIDNumPrototypesMap.find(shapeId) == m_shapeIDNumPrototypesMap.end())
		{
			
			LTKReturnError(EINVALID_SHAPEID);
		}
		
		//Adaptation Code
		LTKAdapt* adaptObj = LTKAdapt::getInstance(this);
		
		int nErrorCode;
		nErrorCode = adaptObj->adapt(shapeId);
		if(nErrorCode !=0)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
				<< "Error during ActiveDTWShapeRecognizer::adapt()"<<endl;
			LTKReturnError(nErrorCode);
		}
		
		//Clear Variables cached 
		m_neighborInfoVec.clear();
		m_vecRecoResult.clear();
		
	}
	catch(...)
	{	
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
			<< "Error during ActiveDTWShapeRecognizer::adapt()"<<endl;
		return FAILURE;
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
		<< "Exit ActiveDTWShapeRecognizer::adapt()"<<endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: adapt
* DESCRIPTION	: adapt sample passed as argument
* ARGUMENTS		: sampleTraceGroup :  TraceGroup of sample to be adapted
*				  shapeID : True shapeID of sample
* RETURNS		: Success : If sample was adapted successfully
*				  Failure : Returns Error Code
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::adapt(const LTKTraceGroup& sampleTraceGroup, int shapeId )
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
		<< "Enter ActiveDTWShapeRecognizer::Adapt()"<<endl;
	
	int errorCode;
	
	if(shapeId < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
			<< "ActiveDTWShapeRecognizer::Adapt(): Invalid shapeId"<< endl;
		LTKReturnError(EINVALID_SHAPEID);	
	}
	
	//check if shapeId has already seen by classifier else add shapeId 
	if(m_shapeIDNumPrototypesMap.find(shapeId) == m_shapeIDNumPrototypesMap.end())
	{
		//add class
		errorCode = addClass(sampleTraceGroup, shapeId);
		
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
				<< "Error during call to recognize in ActiveDTWShapeRecognizer::Adapt()"<<endl;
			LTKReturnError(errorCode);	
		}
	}
	else
	{
		//recognize and adapt
		vector<int> vecSubSet;
		vector<LTKShapeRecoResult> vecRecoResult;
		LTKScreenContext objScreenContext;
		
		errorCode = recognize(sampleTraceGroup,objScreenContext,
			vecSubSet,CONF_THRESHOLD_FILTER_OFF,
			NN_DEF_RECO_NUM_CHOICES,vecRecoResult);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
				<< "Error during call to recognize in ActiveDTWShapeRecognizer::Adapt()"<<endl;
			LTKReturnError(errorCode);	
		}
		
		errorCode = adapt(shapeId);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
				<< "Error during call to recognize in ActiveDTWShapeRecognizer::Adapt()"<<endl;
			LTKReturnError(errorCode);	
		}
		
	}
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
		<< "Exit ActiveDTWShapeRecognizer::Adapt()"<<endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: S Anand
* DATE			: 3-MAR-2009
* NAME			: deleteAdaptInstance
* DESCRIPTION	: delete AdaptInstance
* ARGUMENTS		: 
* RETURNS		: None
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int ActiveDTWShapeRecognizer::deleteAdaptInstance()
{
	//Implemented as deleteAdaptInstance is called by ~NNShapeRecognizer
	//and adaptObj object is not available in NN.cpp
	
	LTKAdapt *adaptInstance = LTKAdapt::getInstance(this);
	if(adaptInstance)
	{
		adaptInstance->deleteInstance();
	}
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR               ; S Anand
* DATE                 : 3-MAR-2009
* NAME			: writePrototypeShapesToMDTFile
* DESCRIPTION	        : Flushes the prototype shapes data to activedtw.mdt file
* ARGUMENTS		: 
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/

int ActiveDTWShapeRecognizer::writePrototypeShapesToMDTFile()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::writePrototypeShapesToMDTFile()" << endl;
	
	int errorCode = SUCCESS;
	
	//Flush to MDT only after m_MDTUpdateFreq modifications
	m_prototypeSetModifyCount++;
	
	if(m_prototypeSetModifyCount == m_MDTUpdateFreq)
	{
		m_prototypeSetModifyCount = 0;
		
		ofstream mdtFileHandle;
		
		//Open the Model data file for writing mode
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			mdtFileHandle.open(m_activedtwMDTFilePath.c_str(), ios::out);
		}
		else
		{
			mdtFileHandle.open(m_activedtwMDTFilePath.c_str(),ios::out|ios::binary);
		}
		
		//Return error if unable to open the Model data file
		if(!mdtFileHandle)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
				getErrorMessage(EMODEL_DATA_FILE_OPEN) <<
				" ActiveDTWShapeRecognizer::writePrototypeShapesToMDTFile()" << endl;
			
			LTKReturnError(EMODEL_DATA_FILE_OPEN);
		}
		
		//write the number of shapes to MDT file 
		
		if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
		{
			// 0 represents dynamic
			mdtFileHandle << 0 << endl;
		}	 
		else
		{
			int numShapes = 0;
			mdtFileHandle.write((char*) &numShapes, sizeof(unsigned short));
		}
		
		int prototypeShapesSize = m_prototypeShapes.size();
		
		for(int i = 0; i < prototypeShapesSize; i++)
		{
			errorCode = appendShapeModelToMDTFile(m_prototypeShapes[i],mdtFileHandle);		
			
			if(errorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
					" ActiveDTWShapeRecognizer::writePrototypeShapesToMDTFile()" << endl;
				LTKReturnError(errorCode);
			}
		}
		
		mdtFileHandle.close();
		
		//Updating the Header Information
		updateHeaderWithAlgoInfo();
		
		//Adding header information	and checksum generation
		string strModelDataHeaderInfoFile = "";
		LTKCheckSumGenerate cheSumGen;
		
		errorCode = cheSumGen.addHeaderInfo(
			strModelDataHeaderInfoFile,
			m_activedtwMDTFilePath,
			m_headerInfo);
		
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  errorCode << " " <<
				" ActiveDTWShapeRecognizer::writePrototypeSetToMDTFile()" << endl;
			
			LTKReturnError(errorCode);
		}
	}
	
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::writePrototypeShapesToMDTFile()" << endl;
	
	return SUCCESS;
}
/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: addClass
* DESCRIPTION	        : Adds a new trace group as a class
* ARGUMENTS		: INPUT
sampleTraceGroup LTKTraceGroup 
shapeID int 
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::addClass(const LTKTraceGroup& sampleTraceGroup, int& shapeID)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::addClass()" << endl;
	
	int errorCode;
	
	//unless projecttype is dynamic we cannot add classes
	if(!m_projectTypeDynamic)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROJ_NOT_DYNAMIC << " " <<
			"Not allowed to ADD shapes to a project with fixed number of shapes"<<
			" ActiveDTWShapeRecognizer::addClass()" << endl;
		
		LTKReturnError(EPROJ_NOT_DYNAMIC);
	}
	
	
	//if the sample does not have a shape id, we assign it NEW_SHAPEID = -2 in runshaperecInternal
	//then in the following module we determine the new shape id based on the existing shape ids
	if(shapeID = NEW_SHAPEID)
	{
		int tempShapeID;
		
		if(m_shapeIDNumPrototypesMap.size() > 0)
		{
			map<int,int>::reverse_iterator m_shapeIDNumPrototypesMapIter;
			m_shapeIDNumPrototypesMapIter = m_shapeIDNumPrototypesMap.rbegin();
			tempShapeID = m_shapeIDNumPrototypesMapIter->first;
			shapeID = tempShapeID+1;
		}
		else
		{
			shapeID = LTK_START_SHAPEID;
		}
		
	}
	
	vector<LTKShapeFeaturePtr> tempFeatureVec;
	
	errorCode = extractFeatVecFromTraceGroup(sampleTraceGroup,tempFeatureVec);
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
			" ActiveDTWShapeRecognizer::addClass()" << endl;
		LTKReturnError(errorCode);
	}	
	
	shapeMatrix newShapeMatrix;
	
	newShapeMatrix.push_back(tempFeatureVec);
	
	//add the feature vector as a singleton Vector
	ActiveDTWShapeModel newShapeModel;
	
	
	errorCode = newShapeModel.setShapeId(shapeID);
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " "<< endl;
		LTKReturnError(errorCode);
	}
	
	newShapeModel.setSingletonVector(newShapeMatrix);
	
	if(m_prototypeShapes.empty())
	{
		m_prototypeShapes.push_back(newShapeModel);
	}
	else
	{
		int prototypeShapesSize = m_prototypeShapes.size();
		int maxClassId = m_prototypeShapes[prototypeShapesSize - 1].getShapeId();
		
		if(shapeID > maxClassId)
			m_prototypeShapes.push_back(newShapeModel);		
		else
		{
			vector<ActiveDTWShapeModel>::iterator prototypeShapesIter = m_prototypeShapes.begin();
			
			while(prototypeShapesIter != m_prototypeShapes.end())
			{
				int currentShapeId = (*prototypeShapesIter).getShapeId();
				
				if(currentShapeId > shapeID)
				{
					m_prototypeShapes.insert(prototypeShapesIter,newShapeModel);
					break;
				}
				prototypeShapesIter++;
			}	
		}
	}
	
	//Update m_shapeIDNumPrototypesMap
	m_shapeIDNumPrototypesMap[shapeID]= 1;
	
	//Update MDT File
	errorCode = writePrototypeShapesToMDTFile();
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
			" ActiveDTWShapeRecognizer::addClass()" << endl;
		LTKReturnError(errorCode);
	}	
	
	//clearing vectors
	tempFeatureVec.clear();
	newShapeMatrix.clear();
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::addClass()" << endl;
	
	return SUCCESS;
}
/**********************************************************************************
* AUTHOR               : S Anand
* DATE                 : 3-MAR-2009
* NAME			: deleteClass
* DESCRIPTION	        : Deletes the class with shapeID
* ARGUMENTS		: INPUT
shapeID int 
* RETURNS		: integer Holds error value if occurs
*						  Holds SUCCESS if no errors
*************************************************************************************/
int ActiveDTWShapeRecognizer::deleteClass(int shapeID)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "ActiveDTWShapeRecognizer::deleteClass()" << endl;	
	
	if(!m_projectTypeDynamic)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROJ_NOT_DYNAMIC << " " <<
			"Not allowed to delete shapes to a project with fixed number of Shapes"<<
			" ActiveDTWShapeRecognizer::deleteClass()" << endl;
		
		LTKReturnError(EPROJ_NOT_DYNAMIC);
	}
	
	//Validate Input Parameters - shapeID
	if(m_shapeIDNumPrototypesMap.find(shapeID) ==m_shapeIDNumPrototypesMap.end())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
			"shapeID is not valid"<<
			" ActiveDTWShapeRecognizer::deleteClass()" << endl;
		
		LTKReturnError(EINVALID_SHAPEID);
	}
	
	vector<ActiveDTWShapeModel>::iterator prototypeShapesIter;
	
	int prototypeShapesSize = m_prototypeShapes.size();
	int k = 0;
	
	for(int i = 0; i < prototypeShapesSize; i++)
	{
		prototypeShapesIter = m_prototypeShapes.begin() + k;
		
		int classId = (*prototypeShapesIter).getShapeId();
		
		if(classId == shapeID)
		{
			vector<ActiveDTWClusterModel> currentClusterModelVector;
			shapeMatrix currentSingletonVector;
			
			//clearing cluster Model and singleton vectors
			currentClusterModelVector = (*prototypeShapesIter).getClusterModelVector();
			currentClusterModelVector.clear();
			(*prototypeShapesIter).setClusterModelVector(currentClusterModelVector);
			
			currentSingletonVector = (*prototypeShapesIter).getSingletonVector();
			currentSingletonVector.clear();
			(*prototypeShapesIter).setSingletonVector(currentSingletonVector);
			
			m_prototypeShapes.erase(prototypeShapesIter);
			continue;
		}
		k++;
		prototypeShapesIter++;
	}
	
	//Update m_shapeIDNumPrototypesMap
	m_shapeIDNumPrototypesMap.erase(shapeID);
	
	//Update MDT File
	int returnStatus = writePrototypeShapesToMDTFile();
	
	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " << returnStatus << " " <<
			"Exiting ActiveDTWShapeRecognizer::deleteClass" <<endl;
		LTKReturnError(returnStatus);
	}
	
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "ActiveDTWShapeRecognizer::deleteClass()" << endl;
	
	return SUCCESS;
}
