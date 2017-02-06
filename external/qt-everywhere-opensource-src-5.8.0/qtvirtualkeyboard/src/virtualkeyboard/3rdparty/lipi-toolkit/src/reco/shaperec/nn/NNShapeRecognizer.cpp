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
 * $LastChangedDate: 2011-02-08 16:57:52 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 834 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation for NN Shape Recognition module
 *
 * CONTENTS:
 *
 * AUTHOR:     Saravanan R.
 *
 w
 * DATE:       January 23, 2004
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/

#include "LTKConfigFileReader.h"

#include "NNShapeRecognizer.h"

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
#include "NNAdapt.h"
#include "LTKLoggerUtil.h"
#include "LTKShapeRecoUtil.h"
#include "LTKTraceGroup.h"
#include "LTKErrors.h"
#include "LTKShapeSample.h"
#include "LTKException.h"
#include "LTKErrorsList.h"
#include "LTKStringUtil.h"
#include "LTKDynamicTimeWarping.h"

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: NNShapeRecognizer
 * DESCRIPTION	: Default Constructor that initializes all data members
 * ARGUMENTS		: none
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/

void NNShapeRecognizer::assignDefaultValues()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::assignDefaultValues()" << endl;

    m_numShapes = 0;
    m_nnCfgFilePath = "";
    m_nnMDTFilePath = "";
    m_ptrPreproc = NULL;
    m_projectTypeDynamic=false;
    m_prototypeSelection=NN_DEF_PROTOTYPESELECTION;
    m_prototypeReductionFactor=NN_DEF_PROTOTYPEREDUCTIONFACTOR;
    m_prototypeDistance=NN_DEF_PROTOTYPEDISTANCE;
    m_nearestNeighbors=NN_DEF_NEARESTNEIGHBORS;
    m_dtwBanding=NN_DEF_BANDING;
    m_dtwEuclideanFilter=NN_DEF_DTWEUCLIDEANFILTER;
    m_preProcSeqn=NN_DEF_PREPROC_SEQ;
    m_ptrFeatureExtractor=NULL;
    m_featureExtractorName=NN_DEF_FEATURE_EXTRACTOR;
    m_numClusters=NN_NUM_CLUST_INITIAL; // just to check that this is not what is mentioned by the user
    m_MDTUpdateFreq=NN_DEF_MDT_UPDATE_FREQ;
    m_prototypeSetModifyCount=0;
    m_rejectThreshold=NN_DEF_REJECT_THRESHOLD;
    m_adaptivekNN=false;
    m_deleteLTKLipiPreProcessor=NULL;
    m_MDTFileOpenMode = NN_MDT_OPEN_MODE_ASCII;
	m_LVQIterationScale=NN_DEF_LVQITERATIONSCALE;
	m_LVQInitialAlpha=NN_DEF_LVQINITIALALPHA;
	m_LVQDistanceMeasure=NN_DEF_LVQDISTANCEMEASURE;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::assignDefaultValues()" << endl;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: initialize
 * DESCRIPTION	: This method initializes the NN shape recognizer
 * ARGUMENTS		: string  Holds the Project Name
 *				  string  Holds the Profile Name
 * RETURNS		: integer Holds error value if occurs
 *						  Holds SUCCESS if no erros
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
NNShapeRecognizer::NNShapeRecognizer(const LTKControlInfo& controlInfo):
m_OSUtilPtr(LTKOSUtilFactory::getInstance()),
m_libHandler(NULL),
m_libHandlerFE(NULL)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::NNShapeRecognizer()" << endl;

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

	    //Holds the path of the Project.cfg
	    string projectCFGPath = strProfileDirectory + PROJECT_CFG_STRING;

	    // Config file

	    m_nnCfgFilePath = m_lipiRootPath + PROJECTS_PATH_STRING +
	        (tmpControlInfo.projectName) + PROFILE_PATH_STRING +
	        (tmpControlInfo.profileName) + SEPARATOR +
	        NN + CONFIGFILEEXT;


	    //Set the path for nn.mdt
	    m_nnMDTFilePath = strProfileDirectory + strProfileName + SEPARATOR + NN + DATFILEEXT;


	    //To find whether the project was dynamic or not andto read read number of shapes from project.cfg
	    int errorCode = m_shapeRecUtil.isProjectDynamic(projectCFGPath,
	            m_numShapes, strNumShapes, m_projectTypeDynamic);

	    if( errorCode != SUCCESS)
	    {
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NNShapeRecognizer::NNShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }


	    //Set the NumShapes to the m_headerInfo
	    m_headerInfo[NUMSHAPES] = strNumShapes;

	    //Currently preproc cfg also present in NN
	    tmpControlInfo.cfgFileName = NN;
	    errorCode = initializePreprocessor(tmpControlInfo,
	            &m_ptrPreproc);

	    if( errorCode != SUCCESS)
	    {
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NNShapeRecognizer::NNShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }

	    //Reading NN configuration file
	    errorCode = readClassifierConfig();

	    if( errorCode != SUCCESS)
	    {
			cout<<endl<<"Encountered error in readClassifierConfig"<<endl;
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NNShapeRecognizer::NNShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }

	    //Writing Feature extractor name and version into the header
	    m_headerInfo[FE_NAME] = m_featureExtractorName;
	    m_headerInfo[FE_VER] = SUPPORTED_MIN_VERSION; //FE version

	    //Writting mdt file open mode to the mdt header
	    m_headerInfo[MDT_FOPEN_MODE] = m_MDTFileOpenMode;

	    errorCode = initializeFeatureExtractorInstance(tmpControlInfo);

	    if( errorCode != SUCCESS)
	    {
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NNShapeRecognizer::NNShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }

	}
	catch(LTKException e)
	{
		
		deletePreprocessor();
		m_prototypeSet.clear();

		m_cachedShapeSampleFeatures.clearShapeSampleFeatures();
	    
	    //Unloading the feature Extractor instance
	    deleteFeatureExtractorInstance();

		delete m_OSUtilPtr;
		throw e;
	}
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::NNShapeRecognizer()" << endl;

}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: readClassifierConfig
 * DESCRIPTION	: Reads the NN.cfg and initializes the data members of the class
 * ARGUMENTS		: none
 * RETURNS		: SUCCESS   - If config file read successfully
 *				  errorCode - If failure
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::readClassifierConfig()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::readClassifierConfig()" << endl;
    string tempStringVar = "";
    int tempIntegerVar = 0;
    float tempFloatVar = 0.0;
    LTKConfigFileReader *shapeRecognizerProperties = NULL;
    int errorCode = FAILURE;

    try
    {
        shapeRecognizerProperties = new LTKConfigFileReader(m_nnCfgFilePath);
    }
    catch(LTKException e)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)<< "Info: " <<
            "Config file not found, using default values of the parameters" <<
            "NNShapeRecognizer::readClassifierConfig()"<<endl;

        delete shapeRecognizerProperties;

		return FAILURE;
    }

    //Pre-processing sequence
    errorCode = shapeRecognizerProperties->getConfigValue(PREPROCSEQUENCE, m_preProcSeqn);

    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO) << "Info: " <<
            "Using default value of prerocessing sequence: "<< m_preProcSeqn <<
            " NNShapeRecognizer::readClassifierConfig()"<<endl;

        m_preProcSeqn = NN_DEF_PREPROC_SEQ;
    }
	
	m_headerInfo[PREPROC_SEQ] = m_preProcSeqn;
	

    if((errorCode = mapPreprocFunctions()) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<" Error: " << errorCode << 
            " NNShapeRecognizer::readClassifierConfig()"<<endl;

        delete shapeRecognizerProperties;

        LTKReturnError(errorCode);
    }

    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(PROTOTYPESELECTION,
            tempStringVar);

    if (errorCode == SUCCESS)
    {
        if( (LTKSTRCMP(tempStringVar.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0) || (LTKSTRCMP(tempStringVar.c_str(), PROTOTYPE_SELECTION_LVQ) == 0))
        {
            m_prototypeSelection = tempStringVar;
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
                PROTOTYPESELECTION << " = " << tempStringVar <<
                "NNShapeRecognizer::readClassifierConfig()"<<endl;
        }

        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                "Error: " << ECONFIG_FILE_RANGE << " " <<
                PROTOTYPESELECTION << " : " << tempStringVar
                <<  " method is not supported"  <<
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << PROTOTYPESELECTION << " : " << 
            m_prototypeSelection <<
            " NNShapeRecognizer::readClassifierConfig()"<<endl;
    }


    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(PROTOREDFACTOR,
                                                          tempStringVar);

    string tempStringVar1 = "";
    int errorCode1 = shapeRecognizerProperties->getConfigValue(NUMCLUSTERS,
                                                               tempStringVar1);

    if(errorCode1 == SUCCESS && errorCode == SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Error: " << ECONFIG_FILE_RANGE
            << " Cannot use both config parameters " <<
            PROTOREDFACTOR << " and " << NUMCLUSTERS << " at the same time "<<
            " NNShapeRecognizer::readClassifierConfig()"<<endl;

        delete shapeRecognizerProperties;

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
                        " NNShapeRecognizer::readClassifierConfig()"<<endl;

                    delete shapeRecognizerProperties;
                    
                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<
                    "Error: " << ECONFIG_FILE_RANGE <<
                    PROTOREDFACTOR << " is out of permitted range"<<
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

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
                        " NNShapeRecognizer::readClassifierConfig()"<<endl;

                    delete shapeRecognizerProperties;
                    
                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    " Error: " << ECONFIG_FILE_RANGE <<
                    NUMCLUSTERS << " is out of permitted range"<<
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Assuming default value of " NUMCLUSTERS << " : " << 
            m_numClusters << endl;
    }
    
    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(PROTOTYPEDISTANCE,
            tempStringVar);

    if(errorCode == SUCCESS )
    {
        if((LTKSTRCMP(tempStringVar.c_str(), EUCLIDEAN_DISTANCE) == 0) ||
                (LTKSTRCMP(tempStringVar.c_str(), DTW_DISTANCE) == 0))
        {
            m_prototypeDistance = tempStringVar;
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                "Prototype Distance Method = " <<tempStringVar<<endl;
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << " " <<
                PROTOTYPEDISTANCE << " : " << tempStringVar <<
                " is not supported" <<
                "NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " PROTOTYPEDISTANCE " : " <<
            m_prototypeDistance << endl;
    }

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
                // If the value of NearestNeighbors = 1, NN recognizer is used
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
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;
                
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEARESTNEIGHBORS <<
                " is out of permitted range" <<
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Debug: " << "Using default value for " << NEARESTNEIGHBORS <<
            " : " << m_nearestNeighbors << endl;
    }

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
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << REJECT_THRESHOLD <<
                " should be in the range (0-1)" <<
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << REJECT_THRESHOLD <<
            " : " << m_rejectThreshold << endl;
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
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;
                
                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: "<< ECONFIG_FILE_RANGE <<
                " DTWBANDING is out of permitted range" <<
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << DTWBANDING << " : " << 
            m_dtwBanding << endl;
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
                        " NNShapeRecognizer::readClassifierConfig()"<<endl;

                    delete shapeRecognizerProperties;

                    LTKReturnError(ECONFIG_FILE_RANGE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << DTWEUCLIDEANFILTER <<
                    " is out of permitted range"<<
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

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
	//LVQ Paramaters
	//LVQ Iteration Scale
	errorCode = shapeRecognizerProperties->getConfigValue(LVQITERATIONSCALE,
			tempStringVar);
	if(errorCode == SUCCESS)
	{
		m_LVQIterationScale=atoi((tempStringVar).c_str());

		if(!((m_LVQIterationScale>0)&& LTKStringUtil::isInteger(tempStringVar)))
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                        "Error: " << ECONFIG_FILE_RANGE <<
                        LVQITERATIONSCALE << " should be a positive integer " <<
                        " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;
            
            LTKReturnError(ECONFIG_FILE_RANGE);
		}
	}
	else
	{
		m_LVQIterationScale = NN_DEF_LVQITERATIONSCALE;
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            "Using default value for LVQIterationScale: " << m_LVQIterationScale <<
            " NNShapeRecognizer::readClassifierConfig()"<<endl;

	}
	//LVQ Initial Alpha
	tempStringVar="";
	errorCode = shapeRecognizerProperties->getConfigValue(LVQINITIALALPHA,
			tempStringVar);

	if(errorCode == SUCCESS)
	{
        m_LVQInitialAlpha = LTKStringUtil::convertStringToFloat(tempStringVar);

		if((m_LVQInitialAlpha>1)||(m_LVQInitialAlpha<0))
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                        "Error: " << ECONFIG_FILE_RANGE <<
                        LVQINITIALALPHA << " is out of permitted range " <<
                        " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;
            
            LTKReturnError(ECONFIG_FILE_RANGE);
		}

	}
	else
	{
		m_LVQInitialAlpha = NN_DEF_LVQINITIALALPHA ;
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            "Using default value for LVQInitialAlpha: " << m_LVQInitialAlpha <<
            " NNShapeRecognizer::readClassifierConfig()"<<endl;
	}
	//LVQ Distance Measure
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(LVQDISTANCEMEASURE,
            tempStringVar);

    if(errorCode == SUCCESS )
    {
        if((LTKSTRCMP(tempStringVar.c_str(), EUCLIDEAN_DISTANCE) == 0) ||
                (LTKSTRCMP(tempStringVar.c_str(), DTW_DISTANCE) == 0))
        {
            m_LVQDistanceMeasure = tempStringVar;
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                "LVQ Prototype Distance Method = " <<tempStringVar<<endl;
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << " " <<
                LVQDISTANCEMEASURE << " : " << tempStringVar <<
                " is not supported" <<
                "NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " LVQDISTANCEMEASURE " : " <<
            m_prototypeDistance << endl;
    }

    tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(MDT_UPDATE_FREQUENCY,
                                                          tempStringVar);

    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
            m_MDTUpdateFreq = atoi(tempStringVar.c_str());
            if(m_MDTUpdateFreq<=0)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << MDT_UPDATE_FREQUENCY <<
                    " should be zero or a positive integer" <<
                    " NNShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << MDT_UPDATE_FREQUENCY <<
                " should be zero or a positive integer" <<
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << MDT_UPDATE_FREQUENCY <<
            " : " << m_MDTUpdateFreq << endl;
    }

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
                " NNShapeRecognizer::readClassifierConfig()"<<endl;

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
        "NNShapeRecognizer::readClassifierConfig()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R
 * DATE          		: 23-Jan-2007
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
int NNShapeRecognizer::mapPreprocFunctions()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::mapPreprocFunctions()" << endl;

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
            " NNShapeRecognizer::mapPreprocFunctions()"<<endl;

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
                        " NNShapeRecognizer::mapPreprocFunctions()"<<endl;
                    LTKReturnError(EINVALID_PREPROC_SEQUENCE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                    "Wrong preprocessor sequence entry in cfg file  : " << module<<
                    " NNShapeRecognizer::mapPreprocFunctions()"<<endl;
                LTKReturnError(EINVALID_PREPROC_SEQUENCE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                "Wrong preprocessor sequence entry in cfg file  : "<<module<<
                " NNShapeRecognizer::mapPreprocFunctions()"<<endl;
            LTKReturnError(EINVALID_PREPROC_SEQUENCE);
        }
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::mapPreprocFunctions()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2004
 * NAME			: ~NNShapeRecognizer
 * DESCRIPTION	: destructor
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
NNShapeRecognizer::~NNShapeRecognizer()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::~NNShapeRecognizer()" << endl;

    deleteAdaptInstance();

	int returnStatus = SUCCESS;
    //To update MDT File
    if(m_prototypeSetModifyCount >0)
    {
        m_prototypeSetModifyCount = m_MDTUpdateFreq-1;

        returnStatus = writePrototypeSetToMDTFile();
        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                " NNShapeRecognizer::~NNShapeRecognizer()" << endl;
             throw LTKException(returnStatus);

        }
    }

    m_neighborInfoVec.clear();

    returnStatus = deletePreprocessor();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
            " NNShapeRecognizer::~NNShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }

    m_prototypeSet.clear();

	m_cachedShapeSampleFeatures.clearShapeSampleFeatures();
    
    //Unloading the feature Extractor instance
    returnStatus = deleteFeatureExtractorInstance();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " <<  returnStatus << " " <<
            " NNShapeRecognizer::~NNShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }

    delete m_OSUtilPtr;
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::~NNShapeRecognizer()" << endl;


}


/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 25-Jan-2004
 * NAME			: train
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::train(const string& trainingInputFilePath,
        const string& mdtHeaderFilePath,
        const string &comment,const string &dataset,
        const string &trainFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::train()" << endl;

    int returnStatus = SUCCESS;


    if(comment.empty() != true)
    {
        m_headerInfo[COMMENT] = comment;
    }

    if(dataset.empty() != true)
    {
        m_headerInfo[DATASET] = dataset;
    }

    //Check the prototype Selection method and call accordingly

    if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_LVQ) == 0)
    {
        returnStatus = trainLVQ(trainingInputFilePath, mdtHeaderFilePath, trainFileType);

        if(returnStatus != SUCCESS)
        {
            LTKReturnError(returnStatus);
        }

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
        "NNShapeRecognizer::train()" << endl;
    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
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
int NNShapeRecognizer::trainClustering(const string& trainingInputFilePath,
        const string &mdtHeaderFilePath,
        const string& inFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::trainClustering()" << endl;

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
                " NNShapeRecognizer::trainClustering()" << endl;
            LTKReturnError(returnStatus);
        }
    }
    else if(LTKSTRCMP(inFileType.c_str(), FEATURE_FILE) == 0)
    {
        //If the Input file is Feature file
        returnStatus = trainFromFeatureFile(trainingInputFilePath);
        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
                getErrorMessage(returnStatus) <<
                " NNShapeRecognizer::trainClustering()" << endl;
            LTKReturnError(returnStatus);
        }

		PreprocParametersForFeatureFile(m_headerInfo);
    }

    //Updating the Header Information
    updateHeaderWithAlgoInfo();

    //Adding header information	and checksum generation
    LTKCheckSumGenerate cheSumGen;

    returnStatus = cheSumGen.addHeaderInfo(mdtHeaderFilePath,
            m_nnMDTFilePath,
            m_headerInfo);

    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
            getErrorMessage(returnStatus) <<
            " NNShapeRecognizer::trainClustering()" << endl;

        LTKReturnError(returnStatus);
    }

    //Time at the end of Train Clustering
    m_OSUtilPtr->recordEndTime();

    string timeTaken = "";
    m_OSUtilPtr->diffTime(timeTaken);
    
    cout << "Time Taken  = " << timeTaken << endl;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::trainClustering()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: appendPrototypesToMDTFile
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::appendPrototypesToMDTFile(const vector<LTKShapeSample>& prototypeVec,
        ofstream & mdtFileHandle)
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::appendPrototypesToMDTFile()" << endl;

    //iterators to iterate through the result vector
    vector<LTKShapeSample>::const_iterator sampleFeatureIter = prototypeVec.begin();
    vector<LTKShapeSample>::const_iterator sampleFeatureIterEnd = prototypeVec.end();

    string strFeature = "";

    if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_FILE_HANDLE << " " <<
            "Invalid file handle for MDT file"<<
            " NNShapeRecognizer::appendPrototypesToMDTFile()" << endl;
        LTKReturnError(EINVALID_FILE_HANDLE);
    }


    for(; sampleFeatureIter != sampleFeatureIterEnd; sampleFeatureIter++)
    {
        //Write the class Id
        int classId = (*sampleFeatureIter).getClassID();

        if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
        {
            mdtFileHandle << classId << " ";
        }
        else
        {
            mdtFileHandle.write((char*) &classId,sizeof(int));
        }

        const vector<LTKShapeFeaturePtr>& shapeFeatureVector = (*sampleFeatureIter).getFeatureVector();

        if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_BINARY )
        {
            int numberOfFeatures = shapeFeatureVector.size();
            int featureDimension = shapeFeatureVector[0]->getFeatureDimension();

            mdtFileHandle.write((char *)(&numberOfFeatures), sizeof(int));
            mdtFileHandle.write((char *)(&featureDimension), sizeof(int));

            floatVector floatFeatureVector;
            m_shapeRecUtil.shapeFeatureVectorToFloatVector(shapeFeatureVector,
                                                           floatFeatureVector);

            int vectorSize = floatFeatureVector.size();

            for (int i=0; i< vectorSize; i++)
            {
                float floatValue = floatFeatureVector[i];
                mdtFileHandle.write((char *)(&floatValue), sizeof(float));
            }
        }
        else
        {

            vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = shapeFeatureVector.begin();
            vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = shapeFeatureVector.end();

            for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
            {
                (*shapeFeatureIter)->toString(strFeature);
                mdtFileHandle << strFeature << FEATURE_EXTRACTOR_DELIMITER;
            }

            mdtFileHandle << "\n";
        }

    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::appendPrototypesToMDTFile()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: preprocess
 * DESCRIPTION	: calls the required pre-processing functions from the LTKPreprocessor library
 * ARGUMENTS		: inTraceGroup - reference to the input trace group
 *				  outPreprocessedTraceGroup - pre-processed inTraceGroup
 * RETURNS		: SUCCESS on successful pre-processing operation
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::preprocess (const LTKTraceGroup& inTraceGroup,
        LTKTraceGroup& outPreprocessedTraceGroup)
{
    int errorCode;
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::preprocess()" << endl;

    int indx = 0;

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
                        " NNShapeRecognizer::preprocess()" << endl;
                    LTKReturnError(errorCode);
                }

                local_inTraceGroup = outPreprocessedTraceGroup;
            }
            indx++;
        }
    }
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting NNShapeRecognizer::preprocess()"<<endl;
    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: calculateMedian
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/

int NNShapeRecognizer::calculateMedian(const int2DVector& clusteringResult,
        const float2DVector& distanceMatrix, vector<int>& outMedianIndexVec)

{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::calculateMedian()" << endl;




    int clusteringResultSize = clusteringResult.size();

    for (int clusterID = 0; clusterID < clusteringResultSize ; clusterID++)
    {
		double minDist = FLT_MAX;
		int medianIndex = -1;
		for (int clusMem = 0; clusMem < clusteringResult[clusterID].size(); clusMem++)// for each element of the cluster
		{
			double dist = 0;
			for(int otherClusMem = 0; otherClusMem < clusteringResult[clusterID].size(); otherClusMem++)
			{
				if(clusteringResult[clusterID][clusMem] != clusteringResult[clusterID][otherClusMem])
				{
					if(clusteringResult[clusterID][otherClusMem] > clusteringResult[clusterID][clusMem])
					{
						int tempi = clusteringResult[clusterID][clusMem];
						int tempj = clusteringResult[clusterID][otherClusMem];
						dist += distanceMatrix[tempi][tempj-tempi-1];
					}
					else
					{
						int tempi = clusteringResult[clusterID][otherClusMem];
						int tempj = clusteringResult[clusterID][clusMem];
						dist += distanceMatrix[tempi][tempj-tempi-1];
					}
				}
			}
			if(dist < minDist)
			{
				minDist = dist;
				medianIndex = clusteringResult[clusterID][clusMem];

			}

		}
		outMedianIndexVec.push_back(medianIndex);
	}
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::calculateMedian()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Ramnath. K
 * DATE			: 19-05-2005
 * NAME			: computerDTWDistanceClusteringWrapper
 * DESCRIPTION	: Wrapper function to the function whichcomputes DTW distance between
 two characters
 * ARGUMENTS		: train character, test character
 * RETURNS		: DTWDistance
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::computeDTWDistance(
        const LTKShapeSample& inFirstShapeSampleFeatures,
        const LTKShapeSample& inSecondShapeSampleFeatures,
        float& outDTWDistance)

{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::computeDTWDistance()" << endl;

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
            " NNShapeRecognizer::computeDTWDistance()" << endl;
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::computeDTWDistance()" << endl;

    return SUCCESS;
}


/**********************************************************************************
 * AUTHOR		: Sridhar Krishna
 * DATE			: 24-04-2006
 * NAME			: computeEuclideanDistance
 * DESCRIPTION	: computes euclidean distance between two characters
 * ARGUMENTS		: LTKShapeFeaturePtrtor - 2
 * RETURNS		: euclidean distance
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::computeEuclideanDistance(
        const LTKShapeSample& inFirstShapeSampleFeatures,
        const LTKShapeSample& inSecondShapeSampleFeatures,
        float& outEuclideanDistance)
{
    const vector<LTKShapeFeaturePtr>& firstFeatureVec = inFirstShapeSampleFeatures.getFeatureVector();
    const vector<LTKShapeFeaturePtr>& secondFeatureVec = inSecondShapeSampleFeatures.getFeatureVector();

    int firstFeatureVectorSize = firstFeatureVec.size();
    int secondFeatureVectorSize = secondFeatureVec.size();

    if(firstFeatureVectorSize != secondFeatureVectorSize)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EUNEQUAL_LENGTH_VECTORS << " " <<
            getErrorMessage(EUNEQUAL_LENGTH_VECTORS) <<
            " NNShapeRecognizer::computeEuclideanDistance()" << endl;

        LTKReturnError(EUNEQUAL_LENGTH_VECTORS);
    }

    for(int i = 0; i < firstFeatureVectorSize; ++i)
    {
        float tempDistance = 0.0f;
        getDistance(firstFeatureVec[i],
                secondFeatureVec[i],
                tempDistance);
        outEuclideanDistance += tempDistance;
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::computeEuclideanDistance()" << endl;
    return SUCCESS;
}


/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 23-01-2007
 * NAME			: loadModelData
 * DESCRIPTION	: loads the reference model file into memory
 * ARGUMENTS		:
 * RETURNS		: SUCCESS on successful loading of the reference model file
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::loadModelData()
{
    int errorCode;
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::loadModelData()" << endl;

    int numofShapes = 0;

    // variable for shape Id
    int classId = -1;

    //Algorithm version
    string algoVersionReadFromMDT = "";

//    int iMajor, iMinor, iBugfix = 0;

    stringStringMap headerSequence;
    LTKCheckSumGenerate cheSumGen;

    if(errorCode = cheSumGen.readMDTHeader(m_nnMDTFilePath,headerSequence))
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::loadModelData()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

    string feVersion = headerSequence[FE_VER];

    // comparing the mdt open mode read from cfg file with value in the mdt header
    string mdtOpenMode = headerSequence[MDT_FOPEN_MODE];

    if (LTKSTRCMP(m_MDTFileOpenMode.c_str(), mdtOpenMode.c_str()) != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of NNMDTFileOpenMode parameter in config file ("<<
            m_MDTFileOpenMode <<") does not match with the value in MDT file ("<<
            mdtOpenMode<<")"<<
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }


    // validating preproc parameters
    int iErrorCode = validatePreprocParameters(headerSequence);
    if (iErrorCode != SUCCESS )
    {
    	LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Values of NNMDTFileOpenMode parameter in config file does not match with "
            <<"the values in MDT file " << "NNShapeRecognizer::loadModelData()" << endl;

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
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(EINCOMPATIBLE_VERSION);
    }
    // Version comparison END

    //Input Stream for Model Data file
    ifstream mdtFileHandle;

    if (m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::in);
    }
    else
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::in | ios::binary);
    }

    //If error while opening, return an error
    if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            " Unable to open model data file : " <<m_nnMDTFilePath<<
            " NNShapeRecognizer::loadModelData()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

    if(m_projectTypeDynamic)
    {
        m_numShapes = numofShapes;
    }

    // validating the header values

    stringVector tokens;
    stringVector classToken;

    string strFeatureVector = "";

    LTKShapeSample shapeSampleFeatures;

    int floatSize = atoi(headerSequence[SIZEOFFLOAT].c_str());

	int intSize = atoi(headerSequence[SIZEOFINT].c_str());


	if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
	{
		while(getline(mdtFileHandle, strFeatureVector, NEW_LINE_DELIMITER ))
		{
			LTKStringUtil::tokenizeString(strFeatureVector,
					CLASSID_FEATURES_DELIMITER,  classToken);

			if(classToken.size() != 2)
				continue;

			classId = atoi((classToken[0]).c_str());

			if(classId == -1)
				continue;

			LTKStringUtil::tokenizeString(classToken[1],  FEATURE_EXTRACTOR_DELIMITER,  tokens);

			vector<LTKShapeFeaturePtr> shapeFeatureVector;
			LTKShapeFeaturePtr shapeFeature;

			for(int i = 0; i < tokens.size(); ++i)
			{
				shapeFeature = m_ptrFeatureExtractor->getShapeFeatureInstance();

				if (shapeFeature->initialize(tokens[i]) != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_INPUT_FORMAT << " " <<
						"Number of features extracted from a trace is not correct"<<
						" NNShapeRecognizer::loadModelData()" << endl;

					LTKReturnError(EINVALID_INPUT_FORMAT);
				}

				shapeFeatureVector.push_back(shapeFeature);
			}
			//Set the feature vector and class id to the shape sample features
			shapeSampleFeatures.setFeatureVector(shapeFeatureVector);
			shapeSampleFeatures.setClassID(classId);
			
			//cout << "load mdt class id :" << classId << endl;
			//Adding all shape sample feature to the prototypeset
			m_prototypeSet.push_back(shapeSampleFeatures);
			//Add to Map
			if(	m_shapeIDNumPrototypesMap.find(classId)==m_shapeIDNumPrototypesMap.end())
			{
				m_shapeIDNumPrototypesMap[classId] = 1;
			}
			else
			{
				++(m_shapeIDNumPrototypesMap[classId]);
			}


			//Clearing the vectors
			shapeFeatureVector.clear();
			tokens.clear();
			classToken.clear();
			classId = -1;
			strFeatureVector = "";


		}
	}

	else
	{
		floatVector floatFeatureVectorBuffer;

		while(!mdtFileHandle.eof())
		{
			mdtFileHandle.read((char*) &classId, intSize);

			if ( mdtFileHandle.fail() )
			{
				break;
			}

			int numberOfFeatures;
			int featureDimension;

			mdtFileHandle.read((char*) &numberOfFeatures, intSize);
			mdtFileHandle.read((char*) &featureDimension, intSize);

			m_prototypeSet.push_back(shapeSampleFeatures);
			LTKShapeSample &shapeSampleFeaturesRef = m_prototypeSet.back();
			shapeSampleFeaturesRef.setClassID(classId);

			// Read all features in one batch
			size_t floatFeatureVectorElementCount = numberOfFeatures * featureDimension;
			floatFeatureVectorBuffer.resize(floatFeatureVectorElementCount);
			mdtFileHandle.read((char*) &floatFeatureVectorBuffer.front(), floatFeatureVectorElementCount * floatSize);
			if ( mdtFileHandle.fail() )
			{
				break;
			}

			int featureIndex = 0;
			
			vector<LTKShapeFeaturePtr>& shapeFeatureVector = shapeSampleFeaturesRef.getFeatureVectorRef();
			shapeFeatureVector.reserve(numberOfFeatures);
			floatVector::const_pointer floatFeatureVectorData = floatFeatureVectorBuffer.data();
			LTKShapeFeaturePtr shapeFeature;

			for ( ; featureIndex < numberOfFeatures ; featureIndex++)
			{

				shapeFeature = m_ptrFeatureExtractor->getShapeFeatureInstance();

				if (shapeFeature->initialize(floatFeatureVectorData + featureIndex * featureDimension, featureDimension) != SUCCESS)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
						EINVALID_INPUT_FORMAT << " " <<
						"Number of features extracted from a trace is not correct"<<
						" NNShapeRecognizer::loadModelData()" << endl;

					LTKReturnError(EINVALID_INPUT_FORMAT);
				}

				shapeFeatureVector.push_back(shapeFeature);

			}

			//Add to Map
			intIntMap::iterator mapEntry;
			if(	(mapEntry = m_shapeIDNumPrototypesMap.find(classId))==m_shapeIDNumPrototypesMap.end())
			{
				m_shapeIDNumPrototypesMap[classId] = 1;
			}
			else
			{
				++mapEntry->second;
			}
		}
	}

	mdtFileHandle.close();

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::loadModelData()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 2-Mar-2007
 * NAME			: sortDist
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
bool NNShapeRecognizer::sortDist(const NeighborInfo& x, const NeighborInfo& y)
{
    return (x.distance) < (y.distance);
}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 23-01-2007
 * NAME			: recognize
 * DESCRIPTION	: recognizes the incoming tracegroup
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
int NNShapeRecognizer::recognize(const LTKTraceGroup& traceGroup,
        const LTKScreenContext& screenContext,
        const vector<int>& inSubSetOfClasses,
        float confThreshold,
        int  numChoices,
        vector<LTKShapeRecoResult>& outResultVector)
{
	int errorCode;
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::recognize()" << endl;


    //Check for empty traces in traceGroup

    if(traceGroup.containsAnyEmptyTrace())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<EEMPTY_TRACE << " " <<
            " Input trace is empty"<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(EEMPTY_TRACE);
    }


    //Contains TraceGroup after Preprocessing is done
    LTKTraceGroup preprocessedTraceGroup;


    //Preprocess the traceGroup
    if( preprocess(traceGroup, preprocessedTraceGroup) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            getErrorMessage(errorCode)<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);
    }

    //Extract the shapeSample from preprocessedTraceGroup
    if(!m_ptrFeatureExtractor)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ENULL_POINTER << " " <<
            " m_ptrFeatureExtractor is NULL"<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(ENULL_POINTER);
    }

    vector<LTKShapeFeaturePtr> shapeFeatureVec;
    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
            shapeFeatureVec);

    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::recognize()" << endl;

        LTKReturnError(errorCode);
    }

	// call recognize with featureVector

	if(recognize( shapeFeatureVec, inSubSetOfClasses, confThreshold,
			numChoices, outResultVector) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            getErrorMessage(errorCode)<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);

	}

	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::recognize()" << endl;

    return SUCCESS;

}


int NNShapeRecognizer::recognize(const vector<LTKShapeFeaturePtr>& shapeFeatureVector,
			const vector<int>& inSubSetOfClasses,
			float confThreshold,
			int  numChoices,
			vector<LTKShapeRecoResult>& outResultVector)
{
	int errorCode;
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::recognize()" << endl;

    m_cancelRecognition = false;

    m_cachedShapeSampleFeatures.setFeatureVector(shapeFeatureVector);

    //Creating a local copy of input inSubSetOfClasses, as it is const, STL's unique function modifies it!!!
    vector<int> subSetOfClasses = inSubSetOfClasses;

	int numPrototypes = m_prototypeSet.size();

    /*********Validation for m_prototypeSet ***************************/
    if ( numPrototypes == 0 )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROTOTYPE_SET_EMPTY << " " <<
            " getErrorMessage(EPROTOTYPE_SET_EMPTY) "<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(EPROTOTYPE_SET_EMPTY); //modify
    }

	int dtwEuclideanFilter = (m_dtwEuclideanFilter == EUCLIDEAN_FILTER_OFF)? 
                                              EUCLIDEAN_FILTER_OFF : (int)((m_dtwEuclideanFilter * numPrototypes) / 100);
	
	if(dtwEuclideanFilter == 0)
		dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;

	if( dtwEuclideanFilter != EUCLIDEAN_FILTER_OFF && dtwEuclideanFilter < m_nearestNeighbors)
	{

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
			"Error: " << ECONFIG_FILE_RANGE <<
			DTWEUCLIDEANFILTER << " is out of permitted range " <<
			" NNShapeRecognizer::recognize()"<<endl;

		LTKReturnError(ECONFIG_FILE_RANGE);
		//	dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;
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



    /*****************************************************************************************/
    /*********************** DECLARING IMPORTANT LOCAL VARIABLES *****************************/
    /*****************************************************************************************/

    //Variable to store the DTW distance.
    float dtwDistance = 0.0f;

    //Variable to store the Euclidean distance.
    float euclideanDistance = 0.0f;


    // begin and end iterators for m_prototypeSet
    vector<LTKShapeSample>::iterator prototypeSetIter = m_prototypeSet.begin();
    vector<LTKShapeSample>::iterator prototypeSetIterEnd = m_prototypeSet.end();

    //iterator for iterating the input shape subset vector

    vector<int>::iterator subSetOfClassesIter;

    int prototypeIndexOffset=0;

    int numPrototypesForSubset=0;

    if(subSetOfClasses.size()>0)
    {
        sort(subSetOfClasses.begin(),subSetOfClasses.end());
        subSetOfClasses.erase(unique(subSetOfClasses.begin(),subSetOfClasses.end()),subSetOfClasses.end());
    }
    
	// Clearing cached Variables
    m_vecRecoResult.clear();
    m_neighborInfoVec.clear();

    //Temporary variable to be used to populate distIndexPairVector
    struct NeighborInfo tempPair;


    /***************End of declarations and initializations of variables**************/



    /***************************************************************/
    /*************** Computation of Distance ***********************/
    /***************************************************************/


    if(subSetOfClasses.size()>0)
    {
        vector<int>::iterator tempSubsetIter = subSetOfClasses.begin();
        vector<int>::iterator tempSubsetIterEnd = subSetOfClasses.end();

        for(; tempSubsetIter!= tempSubsetIterEnd; ++tempSubsetIter)
        {
            if(m_shapeIDNumPrototypesMap.find(*tempSubsetIter)==m_shapeIDNumPrototypesMap.end())
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
                    "Invalid class ID in the shape subset vector:"<<(*tempSubsetIter)<<
                    " NNShapeRecognizer::recognize()" << endl;
                LTKReturnError(EINVALID_SHAPEID);
            }
            else
            {
                numPrototypesForSubset = numPrototypesForSubset + m_shapeIDNumPrototypesMap[*tempSubsetIter];
            }
        }

        if(dtwEuclideanFilter!= EUCLIDEAN_FILTER_OFF)
        {
            if(numPrototypesForSubset < dtwEuclideanFilter)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
                    "Number of prototypes corresponding to subset of classes asked for is less than Euclidean filter size; switching Euclidean filter off" << endl;
                dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;
            }
        }

        subSetOfClassesIter=subSetOfClasses.begin();
    }
    // If Euclidean filter size >= size of the m_prototypeSet, do not use twEuclideanFilter
    else if(subSetOfClasses.size()==0 && dtwEuclideanFilter >= m_prototypeSet.size())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            "dtwEuclideanFilter >= m_prototypeSet.size(), switching Euclidean filter off" << endl;
        dtwEuclideanFilter = EUCLIDEAN_FILTER_OFF;
    }

    // If the distance metric is Euclidean, compute the distances of test sample to all the samples in the prototype set
    if(LTKSTRCMP(m_prototypeDistance.c_str(), EUCLIDEAN_DISTANCE) == 0)
    {
        for (int j = 0; prototypeSetIter != prototypeSetIterEnd; )
        {

            if(subSetOfClasses.size()>0)
            {
                if((*prototypeSetIter).getClassID()<(*subSetOfClassesIter))
                {
                    j=j + m_shapeIDNumPrototypesMap[(*prototypeSetIter).getClassID()];
                    prototypeSetIter=prototypeSetIter +
                        m_shapeIDNumPrototypesMap[(*prototypeSetIter).getClassID()];

                    continue;
                }


            }
            if(subSetOfClasses.size()==0 || (*prototypeSetIter).getClassID()==(*subSetOfClassesIter))
            {
                while(subSetOfClasses.size()==0 || (*prototypeSetIter).getClassID()==(*subSetOfClassesIter))
                {
					euclideanDistance = 0.0f;
                    errorCode = computeEuclideanDistance(*prototypeSetIter,
                            m_cachedShapeSampleFeatures,
                            euclideanDistance);

                    if(errorCode == SUCCESS && m_cancelRecognition)
                        return SUCCESS;

                    if(errorCode != SUCCESS)
                    {
                        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                            " NNShapeRecognizer::recognize()" << endl;

                        LTKReturnError(errorCode);

                    }

                    tempPair.distance = euclideanDistance;
                    tempPair.classId = (*prototypeSetIter).getClassID();
                    tempPair.prototypeSetIndex = j;

                    m_neighborInfoVec.push_back(tempPair);
                    ++prototypeSetIter;
                    ++j;

                    if(prototypeSetIter==m_prototypeSet.end())
                    {
                        break;
                    }
                }

                if(subSetOfClasses.size()>0)
                {
                    ++subSetOfClassesIter;
                    if(subSetOfClassesIter==subSetOfClasses.end())
                    {
                        break;
                    }
                }
            }
        }
    }
    // If the distance metric is DTW
    else if(LTKSTRCMP(m_prototypeDistance.c_str(), DTW_DISTANCE) == 0)
    {
        vector<bool> filterVector(m_prototypeSet.size(), true);

        // If Euclidean Filter is specified, find Euclidean distance to all the samples in the prototypeset,
        // choose the top dtwEuclideanFilter number of prototypes, to which the DTW distance will be computed.
        if(dtwEuclideanFilter != EUCLIDEAN_FILTER_OFF)
        {
            vector <struct NeighborInfo> eucDistIndexPairVector;

            filterVector.assign(m_prototypeSet.size(), false);


            for (int j = 0; prototypeSetIter != prototypeSetIterEnd;)
            {
                if(subSetOfClasses.size()>0)
                {
                    if((*prototypeSetIter).getClassID()<(*subSetOfClassesIter))
                    {

                        j=j + m_shapeIDNumPrototypesMap[(*prototypeSetIter).getClassID()];
                        prototypeSetIter = prototypeSetIter + m_shapeIDNumPrototypesMap
                            [(*prototypeSetIter).getClassID()];

                        continue;
                    }
                }
                if(subSetOfClasses.size()==0 || (*prototypeSetIter).getClassID()==(*subSetOfClassesIter))
                {
                    while(subSetOfClasses.size()==0 ||
                            (*prototypeSetIter).getClassID()==(*subSetOfClassesIter))
                    {
						euclideanDistance = 0.0f;
                        errorCode = computeEuclideanDistance(*prototypeSetIter,
                                m_cachedShapeSampleFeatures,
                                euclideanDistance);

                        if(errorCode == SUCCESS && m_cancelRecognition)
                            return SUCCESS;

                        if(errorCode != SUCCESS)
                        {
                            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                                " NNShapeRecognizer::recognize()" << endl;

                            LTKReturnError(errorCode);
                        }


                        tempPair.distance = euclideanDistance;
                        tempPair.classId = (*prototypeSetIter).getClassID();
                        tempPair.prototypeSetIndex = j;

                        eucDistIndexPairVector.push_back(tempPair);
                        ++prototypeSetIter;
                        ++j;

                        if(prototypeSetIter==m_prototypeSet.end())
                        {
                            break;
                        }
                    }

                    if(subSetOfClasses.size()>0)
                    {
                        ++subSetOfClassesIter;
                        if(subSetOfClassesIter==subSetOfClasses.end())
                        {
                            break;
                        }
                    }
                }
            }

            //Sort the eucDistIndexPairVector in ascending order of distance, and used only top dtwEuclideanFilter for DTW distance computation
            sort(eucDistIndexPairVector.begin(), eucDistIndexPairVector.end(), sortDist);

            for (int z = 0; z < dtwEuclideanFilter; ++z )
            {
                int prototypeSetIndex = eucDistIndexPairVector[z].prototypeSetIndex ;
                filterVector[prototypeSetIndex] = true;
            }
        }
        else
        {
            if(subSetOfClasses.size()>0)
            {
                filterVector.assign(m_prototypeSet.size(), false);

                for(map<int,int>::iterator shapeIDNumPrototypesMapIter=m_shapeIDNumPrototypesMap.begin();
                        shapeIDNumPrototypesMapIter!=m_shapeIDNumPrototypesMap.end();)
                {
                    if(shapeIDNumPrototypesMapIter->first<(*subSetOfClassesIter))
                    {

                        prototypeIndexOffset=prototypeIndexOffset +
                            shapeIDNumPrototypesMapIter->second;

                        ++shapeIDNumPrototypesMapIter;

                        continue;
                    }
                    else if(shapeIDNumPrototypesMapIter->first==(*subSetOfClassesIter))
                    {
                        while(m_prototypeSet[prototypeIndexOffset].getClassID()==
							(*subSetOfClassesIter))
                        {
                            filterVector[prototypeIndexOffset]=true;
                            ++prototypeIndexOffset;
							if( prototypeIndexOffset == m_prototypeSet.size())								break;                        }
						 if(subSetOfClassesIter==subSetOfClasses.end())                        {                            break;                        }                        //filterVector.assign(filterVector.begin()+prototypeIndexOffset,filterVector.begin()+prototypeIndexOffset+shapeIDNumPrototypesMapIter->second,true);
                        ++shapeIDNumPrototypesMapIter;
                        ++subSetOfClassesIter;
                        if(subSetOfClassesIter==subSetOfClasses.end())
                        {
                            break;
                        }
                       
                    }
                }
            }
        }


        //Iterate through all the prototypes, and compute DTW distance to the prototypes for which corresponding entry in filterVector is true
        for (int i = 0 ; i < m_prototypeSet.size(); ++i )
        {

            if(filterVector[i])
            {
				dtwDistance = 0.0f;
                errorCode = computeDTWDistance(m_prototypeSet[i],
                        m_cachedShapeSampleFeatures,
                        dtwDistance);

                if(errorCode == SUCCESS && m_cancelRecognition)
                    return SUCCESS;

                if(errorCode != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                        " NNShapeRecognizer::recognize()" << endl;

                    LTKReturnError(errorCode);
                }

                tempPair.distance = dtwDistance;
                tempPair.classId = (m_prototypeSet[i]).getClassID();
                tempPair.prototypeSetIndex = i;
                m_neighborInfoVec.push_back(tempPair);
            }
        }

        filterVector.clear();

    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_FILE_RANGE << " " <<
            "The selected prototype distance method \""<<
            m_prototypeDistance<<"\" is not supported "<<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(ECONFIG_FILE_RANGE);
    }


    //Sort the distIndexPairVector based on distances, in ascending order.
    sort(m_neighborInfoVec.begin(), m_neighborInfoVec.end(), sortDist);

    //Reject the sample if the similarity of the nearest sample is lower than m_rejectThreshold specified by the user.
    if(SIMILARITY(m_neighborInfoVec[0].distance) <= m_rejectThreshold)
    {
		
        m_vecRecoResult.clear();
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<"Test sample too distinct, rejecting the sample"<<endl;
        return SUCCESS;
    }

    //Compute the confidences of the classes appearing in distIndexPairVector
    //outResultVector is an output argument, populated in computeConfidence()
    if((errorCode = computeConfidence()) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);
    }

    // Temporary result vector to store the results based on confThreshold and numChoices specified by the user.
    vector<LTKShapeRecoResult> tempResultVector;

    //If confThreshold is specified, get the entries from resultVector with confidence >= confThreshold
    if(confThreshold != CONF_THRESHOLD_FILTER_OFF)
    {
        for(int i = 0 ; i < m_vecRecoResult.size() ; i++)
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
            for( int i = 0 ; i < numChoices ; ++i)
                tempResultVector.push_back(m_vecRecoResult[i]);
            m_vecRecoResult.clear();
            m_vecRecoResult = tempResultVector;
            tempResultVector.clear();
        }

    }

    if(m_cancelRecognition)
        m_vecRecoResult.clear();

    outResultVector = m_vecRecoResult;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::recognize()" << endl;

    return SUCCESS;

}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 23-Jan-2007
 * NAME			: unloadModelData
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		: SUCCESS
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::unloadModelData()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::unloadModelData()" << endl;

    int returnStatus = SUCCESS;

    //Update MDT file with any modification, if available in memory
    if(m_prototypeSetModifyCount >0)
    {
        m_prototypeSetModifyCount = m_MDTUpdateFreq-1;

        returnStatus = writePrototypeSetToMDTFile();
        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                " NNShapeRecognizer::unloadModelData()" << endl;
        }
        m_prototypeSetModifyCount = 0;
    }

    //Clearing the prototypSet
    m_prototypeSet.clear();
    m_shapeIDNumPrototypesMap.clear();

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::unloadModelData()" << endl;

    return SUCCESS;
}

/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 27-Feb-2007
 * NAME			: setDeviceContext
 * DESCRIPTION	: New Function - Not yet added
 * ARGUMENTS		:
 * RETURNS		: SUCCESS
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::setDeviceContext(const LTKCaptureDevice& deviceInfo)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::setDeviceContext()" << endl;

    if(m_ptrPreproc == NULL)
    {
		int returnStatus = ECREATE_PREPROC;
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
            " NNShapeRecognizer::setDeviceContext()" << endl;
        LTKReturnError(returnStatus);
    }

	m_ptrPreproc->setCaptureDevice(deviceInfo);

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::setDeviceContext()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Nidhi sharma
 * DATE			: 22-02-2007
 * NAME			: peformClustering
 * DESCRIPTION	: This method will do Custering for the given ShapeSamples
 * ARGUMENTS		:
 * RETURNS		: medianIndex
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::performClustering(const vector<LTKShapeSample> & shapeSamplesVec,
        vector<LTKShapeSample>& outClusteredShapeSampleVec)

{
	int errorCode;
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::performClustering()" << endl;

    intVector tempVec;
    int2DVector outputVector;
    float2DVector distanceMatrix;
    int sampleCount=shapeSamplesVec.size();
    int returnStatus = SUCCESS;

    if(m_prototypeReductionFactor == -1)
    {
        //find number of clusters automatically
        //this is done when either of NumClusters or PrototypeReducrion factor is set
        //to automatic
        LTKHierarchicalClustering<LTKShapeSample,NNShapeRecognizer> hc(shapeSamplesVec,AVERAGE_LINKAGE,AVG_SIL);
        if(LTKSTRCMP(m_prototypeDistance.c_str(), DTW_DISTANCE) == 0)
        {
            returnStatus = hc.cluster(this,&NNShapeRecognizer::computeDTWDistance);
            if(returnStatus != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                    " NNShapeRecognizer::performClustering()" << endl;
                LTKReturnError(returnStatus);
            }
        }
        else if(LTKSTRCMP(m_prototypeDistance.c_str(), EUCLIDEAN_DISTANCE) == 0)
        {
            returnStatus = hc.cluster(this,&NNShapeRecognizer::computeEuclideanDistance);
            if(returnStatus != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                    " NNShapeRecognizer::performClustering()" << endl;
                LTKReturnError(returnStatus);
            }
        }

        //Cluster results are populated in an outputVector
        hc.getClusterResult(outputVector);
        distanceMatrix = hc.getProximityMatrix();

    }
    else if(m_prototypeReductionFactor == 0|| m_numClusters >= sampleCount)
    {
        //case where clustering is not required every sample is a cluster by itself
        outClusteredShapeSampleVec = shapeSamplesVec;
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
        LTKHierarchicalClustering<LTKShapeSample,NNShapeRecognizer>
            hc(shapeSamplesVec,numClusters, AVERAGE_LINKAGE);


        if(numClusters == 1)
        {
            int tempVar;
            if(LTKSTRCMP(m_prototypeDistance.c_str(), DTW_DISTANCE) == 0)
            {
                hc.computeProximityMatrix(this, &NNShapeRecognizer::computeDTWDistance);
            }
            else if(LTKSTRCMP(m_prototypeDistance.c_str(), EUCLIDEAN_DISTANCE) == 0)
            {
                hc.computeProximityMatrix(this, &NNShapeRecognizer::computeEuclideanDistance);
            }

            for(tempVar=0;tempVar<shapeSamplesVec.size();tempVar++)
            {
                tempVec.push_back(tempVar);
            }

            outputVector.push_back(tempVec);
            tempVec.clear();
        }
        else
        {
            if(LTKSTRCMP(m_prototypeDistance.c_str(), DTW_DISTANCE) == 0)
            {
                returnStatus = hc.cluster(this,&NNShapeRecognizer::computeDTWDistance);
                if(returnStatus != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                        " NNShapeRecognizer::performClustering()" << endl;
                    LTKReturnError(returnStatus);
                }

            }
            else if(LTKSTRCMP(m_prototypeDistance.c_str(), EUCLIDEAN_DISTANCE) == 0)
            {
                returnStatus = hc.cluster(this,&NNShapeRecognizer::computeEuclideanDistance);
                if(returnStatus != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
                        " NNShapeRecognizer::performClustering()" << endl;
                    LTKReturnError(returnStatus);
                }


            }

            //Cluster results are populated in an outputVector
            hc.getClusterResult(outputVector);
        }
        distanceMatrix = hc.getProximityMatrix();
    }
	catch(LTKException e)
	{
		errorCode = e.getErrorCode();
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                    " NNShapeRecognizer::performClustering()" << endl;
		LTKReturnError(errorCode);
	}
    }

    if((m_prototypeReductionFactor != 0 && m_prototypeReductionFactor != NN_NUM_CLUST_INITIAL)||
            (m_numClusters>0 && m_numClusters<sampleCount))
    {

	    vector<int> medianIndexVec;

	    errorCode = calculateMedian(outputVector, distanceMatrix, medianIndexVec);

            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode <<
                    " NNShapeRecognizer::performClustering()" << endl;
                LTKReturnError(errorCode);

            }

	    for(int numMedians = 0 ; numMedians < medianIndexVec.size() ; numMedians++)
	    {
            	outClusteredShapeSampleVec.push_back(shapeSamplesVec[medianIndexVec[numMedians]]);
	    }
	    /*
        int medianIndex = 0;
        for (int clusNum = 0; clusNum < outputVector.size(); clusNum++)
        {

            errorCode = calculateMedian(outputVector, distanceMatrix, clusNum,medianIndex);
            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<
                    "Error computing median, calculateMedian()" <<
                    " NNShapeRecognizer::performClustering()" << endl;
                LTKReturnError(errorCode);

            }
            outClusteredShapeSampleVec.push_back(shapeSamplesVec[medianIndex]);
        }
	*/
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::performClustering()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Nidhi sharma
 * DATE			: 22-02-2007
 * NAME			: getShapeSampleFromInkFile
 * DESCRIPTION	: This method will get the ShapeSample by giving the ink
 *				  file path as input
 * ARGUMENTS		:
 * RETURNS		: SUCCESS
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::getShapeFeatureFromInkFile(const string& inkFilePath,
        vector<LTKShapeFeaturePtr>& shapeFeatureVec)
{
	int errorCode;
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::getShapeFeatureFromInkFile()" << endl;

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
            " NNShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(returnVal);
    }

    m_ptrPreproc->setCaptureDevice(captureDevice);
    m_ptrPreproc->setScreenContext(screenContext);

    preprocessedTraceGroup.emptyAllTraces();

    //Preprocessing to be done for the trace group that was read
    if( preprocess(inTraceGroup, preprocessedTraceGroup) != SUCCESS )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }

    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
            shapeFeatureVec);

    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::getShapeFeatureFromInkFile()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Saravanan
 * DATE			: 22-02-2007
 * NAME			: trainFromListFile
 * DESCRIPTION	: This method will do the training by giving the Train List
 *				  file as input
 * ARGUMENTS		:
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::trainFromListFile(const string& listFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::trainFromListFile()" << endl;


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

    LTKShapeSample shapeSampleFeatures;

    vector<LTKShapeSample> shapeSamplesVec;

    vector<LTKShapeSample> clusteredShapeSampleVec;

    ofstream mdtFileHandle;
    ifstream listFileHandle;

    vector<LTKShapeFeaturePtr> shapeFeature;

    //Get the Instance of LTKShapeFeatureExtractor
    //initializeFeatureExtractorInstance();

    //Opening the train list file for reading mode
    listFileHandle.open(listFilePath.c_str(), ios::in);

    //Throw an error if unable to open the training list file
    if(!listFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ETRAINLIST_FILE_OPEN << " " <<
            getErrorMessage(ETRAINLIST_FILE_OPEN)<<
            " NNShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(ETRAINLIST_FILE_OPEN);
    }

    //Open the Model data file for writing mode
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::out);
    }
    else
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(),ios::out|ios::binary);
    }

    //Throw an error if unable to open the Model data file
    if(!mdtFileHandle)
    {
        listFileHandle.close();
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            getErrorMessage(EMODEL_DATA_FILE_OPEN)<<
            " NNShapeRecognizer::trainFromListFile()" << endl;

        listFileHandle.close();
        
        LTKReturnError(EMODEL_DATA_FILE_OPEN);
    }

    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        //Write the number of Shapes
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
						" NNShapeRecognizer::trainFromListFile()" << endl;

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
						"The NNShapeRecognizer requires training file class Ids to be positive integers and listed in the increasing order"<<
						" NNShapeRecognizer::trainFromListFile()" << endl;
					errorCode = EINVALID_SHAPEID;
					break;
				}
				else if(shapeId < prevClassId)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
						"Shape IDs in the train list file should be in the increasing order. Please use scripts/validateListFile.pl to generate list files." <<
						" NNShapeRecognizer::trainFromListFile()" << endl;
					errorCode = EINVALID_ORDER_LISTFILE;
					break;
				}


				//This condition is used to handle the first sample in the file
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
					" NNShapeRecognizer::trainFromListFile()" << endl;
				continue;
			}

			shapeSampleFeatures.setFeatureVector(shapeFeature);
			shapeSampleFeatures.setClassID(shapeId);

			++sampleCount;
			shapeSamplesVec.push_back(shapeSampleFeatures);

			shapeFeature.clear();

			//All the samples are pushed to m_trainSet used only for trainLVQ

			if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_LVQ)
					== 0 && m_prototypeReductionFactor != 0)
				m_trainSet.push_back(shapeSampleFeatures);
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
				// No LVQ training being done?
				errorCode = performClustering(shapeSamplesVec,
						clusteredShapeSampleVec);
				if( errorCode != SUCCESS )
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
						" NNShapeRecognizer::trainFromListFile()" << endl;

					listFileHandle.close();
					mdtFileHandle.close();
					LTKReturnError(errorCode);
				}

				if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_LVQ) == 0)
				{
					//Push all the samples after clustering into prototypeSet
					for( int i = 0; i < clusteredShapeSampleVec.size(); ++i )
					{
						m_prototypeSet.push_back(clusteredShapeSampleVec[i]);
					}
				}
				else if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0)
				{
					//Writing results to the MDT file
					errorCode = appendPrototypesToMDTFile(clusteredShapeSampleVec, mdtFileHandle);
					if( errorCode != SUCCESS )
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<errorCode << " " <<
							" NNShapeRecognizer::trainFromListFile()" << endl;

						listFileHandle.close();
						mdtFileHandle.close();
						LTKReturnError(errorCode);
					}

				}

				//Clearing the shapeSampleVector and clusteredShapeSampleVector
				clusteredShapeSampleVec.clear();
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
            " NNShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(EINVALID_NUM_OF_SHAPES);
    }
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(errorCode);

    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::trainFromListFile()" << endl;

    return SUCCESS;

}


/******************************************************************************
* AUTHOR		: Saravanan
* DATE			: 23-03-2007
* NAME			: getShapeSampleFromString
* DESCRIPTION	: This method get the Shape Sample Feature from a given line
* ARGUMENTS		:
* RETURNS		: none
* NOTES			: w
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int NNShapeRecognizer::getShapeSampleFromString(const string& inString, LTKShapeSample& outShapeSample)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::getShapeSampleFromString()" << endl;


    //Line is split into tokens
    stringVector tokens;

    //Class Id
    int classId = -1;

    //Feature Vector string
    string strFeatureVector = "";

    //Tokenize the string
    int errorCode = LTKStringUtil::tokenizeString(inString, EMPTY_STRING,  tokens);
    if(errorCode != SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Error: "<< errorCode << " " <<
		    " NNShapeRecognizer::getShapeSampleFromString()" << endl;
	    LTKReturnError(errorCode);
    }


    //Tokens must be of size 2, one is classId and other is Feature Vector
    if( tokens.size() != 2)
        return FAILURE;

    //Tokens[0] indicates the path name
    classId = atoi(tokens[0].c_str());

    //Tokens[1] indicates the shapeId
    strFeatureVector = tokens[1];

    errorCode = LTKStringUtil::tokenizeString(strFeatureVector,  FEATURE_EXTRACTOR_DELIMITER,  tokens);
    if(errorCode != SUCCESS)
    {
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Error: "<< errorCode << " " <<
		    " NNShapeRecognizer::getShapeSampleFromString()" << endl;
	    LTKReturnError(errorCode);
    }



    vector<LTKShapeFeaturePtr> shapeFeatureVector;
    LTKShapeFeaturePtr shapeFeature;

    for(int i = 0; i < tokens.size(); ++i)
    {
        shapeFeature = m_ptrFeatureExtractor->getShapeFeatureInstance();
        if(shapeFeature->initialize(tokens[i]) != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) <<"Error: "<< EINVALID_INPUT_FORMAT << " " <<
                "Number of features extracted from a trace is not correct" <<
                " NNShapeRecognizer::getShapeSampleFromString()" << endl;
            LTKReturnError(EINVALID_INPUT_FORMAT);
        }
        shapeFeatureVector.push_back(shapeFeature);
    }

    //Set the feature vector and class id to the shape sample features
    outShapeSample.setFeatureVector(shapeFeatureVector);
    outShapeSample.setClassID(classId);

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::getShapeSampleFromString()" << endl;

    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Saravanan
* DATE			: 22-02-2007
* NAME			: initializeFeatureExtractorInstance
* DESCRIPTION	: This method get the Instance of the Feature Extractor
*				  from LTKShapeFeatureExtractorFactory
* ARGUMENTS		:
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int NNShapeRecognizer::initializeFeatureExtractorInstance(const LTKControlInfo& controlInfo)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"NNShapeRecognizer::initializeFeatureExtractorInstance()" << endl;


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
			" NNShapeRecognizer::initializeFeatureExtractorInstance()" << endl;
		LTKReturnError(errorCode);
	}
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
		"NNShapeRecognizer::initializeFeatureExtractorInstance()" << endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Saravanan
* DATE			: 26-03-2007
* NAME			: deleteFeatureExtractorInstance
* DESCRIPTION	: This method unloads the Feature extractor instance
* ARGUMENTS		: none
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int NNShapeRecognizer::deleteFeatureExtractorInstance()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

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
                " NNShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

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
        "NNShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Saravanan
* DATE			: 22-02-2007
* NAME			: updateHeaderWithAlgoInfo
* DESCRIPTION	: This method will Update the Header information for the MDT file
* ARGUMENTS		:
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
void NNShapeRecognizer::updateHeaderWithAlgoInfo()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;

    m_headerInfo[RECVERSION] = m_currentVersion;
    string algoName = NN;
    m_headerInfo[RECNAME] = algoName;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;

}


/******************************************************************************
* AUTHOR		: Saravanan
* DATE			: 19-03-2007
* NAME			: getDistance
* DESCRIPTION	:
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
void NNShapeRecognizer::getDistance(const LTKShapeFeaturePtr& f1,
    const LTKShapeFeaturePtr& f2,
    float& distance)
{
    f1->getDistance(f2, distance);
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 11-06-2007
 * NAME			: getFeaturesFromTraceGroup
 * DESCRIPTION	: 1. PreProcess 2. Extract Features 3.Add to PrototypeSet
 *				  4. Add to MDT
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::extractFeatVecFromTraceGroup(const LTKTraceGroup& inTraceGroup,
        vector<LTKShapeFeaturePtr>& featureVec)
{
	int errorCode;
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;

    LTKTraceGroup preprocessedTraceGroup; 	//TraceGroup after Preprocessing

    //Check for empty traces in inTraceGroup
    if(inTraceGroup.containsAnyEmptyTrace())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_TRACE << " " <<
            getErrorMessage(EEMPTY_TRACE) <<
            " NNShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(EEMPTY_TRACE);
    }

    //Preprocess the inTraceGroup
    if(preprocess(inTraceGroup, preprocessedTraceGroup) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(errorCode);
    }

    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
            featureVec);

    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NNShapeRecognizer::extractFeatVecFromTraceGroup()" << endl;
    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 07-06-2007
 * NAME			: addClass
 * DESCRIPTION	: 1. PreProcess 2. Extract Features 3.Add to PrototypeSet
 *				  4. Add to MDT
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::addClass(const LTKTraceGroup& sampleTraceGroup, int& shapeID)
{
    // Should be moved to nnInternal
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::addClass()" << endl;

    LTKShapeSample shapeSampleFeatures;

    if(!m_projectTypeDynamic)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROJ_NOT_DYNAMIC << " " <<
            "Not allowed to ADD shapes to a project with fixed number of shapes"<<
            " NNShapeRecognizer::addClass()" << endl;

        LTKReturnError(EPROJ_NOT_DYNAMIC);
    }

    //Compute ClassID
    int tempShapeID;
    if(m_shapeIDNumPrototypesMap.size()>0)
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

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<< "New ShapeID = "<<shapeID<<endl;
    //Calculate Features
    int errorCode = SUCCESS;
    vector<LTKShapeFeaturePtr> tempFeatureVec;

    errorCode = extractFeatVecFromTraceGroup(sampleTraceGroup,tempFeatureVec);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::addClass()" << endl;
        LTKReturnError(errorCode);
    }

    shapeSampleFeatures.setFeatureVector(tempFeatureVec);

    shapeSampleFeatures.setClassID(shapeID);

    //Update m_prototypeSet
    errorCode = insertSampleToPrototypeSet(shapeSampleFeatures);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::addClass()" << endl;
        LTKReturnError(errorCode);
    }

    //Update m_shapeIDNumPrototypesMap
    m_shapeIDNumPrototypesMap[shapeID]= 1;


    //Update MDT File
    errorCode = writePrototypeSetToMDTFile();
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::addClass()" << endl;
        LTKReturnError(errorCode);
    }


    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NNShapeRecognizer::addClass"<<endl;
    return SUCCESS;
}
/******************************************************************************
* AUTHOR		: Anish Kumar
* DATE			: 07-06-2007
* NAME			: addSample
* DESCRIPTION	: Add Sample to given class
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
int NNShapeRecognizer::addSample(const LTKTraceGroup& sampleTraceGroup, int shapeID)
{
	// Should be moved to nnInternal
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"NNShapeRecognizer::addSample()" << endl;

	LTKShapeSample shapeSampleFeatures;

	if(!m_projectTypeDynamic)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROJ_NOT_DYNAMIC << " " <<
			"Not allowed to ADD shapes to a project with fixed number of shapes"<<
			" NNShapeRecognizer::addSample()" << endl;

		LTKReturnError(EPROJ_NOT_DYNAMIC);
	}

	//Calculate Features
	int errorCode = SUCCESS;
	vector<LTKShapeFeaturePtr> tempFeatureVec;

	errorCode = extractFeatVecFromTraceGroup(sampleTraceGroup,tempFeatureVec);
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
			" NNShapeRecognizer::addSample()" << endl;
		LTKReturnError(errorCode);
	}

	shapeSampleFeatures.setFeatureVector(tempFeatureVec);

	shapeSampleFeatures.setClassID(shapeID);

	//Update m_prototypeSet
	errorCode = insertSampleToPrototypeSet(shapeSampleFeatures);
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
			" NNShapeRecognizer::addSample()" << endl;
		LTKReturnError(errorCode);
	}

	//Update m_shapeIDNumPrototypesMap
	int currentNum = m_shapeIDNumPrototypesMap[shapeID];
	m_shapeIDNumPrototypesMap[shapeID]= currentNum+1;

	//Update MDT File
	errorCode = writePrototypeSetToMDTFile();
	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
			" NNShapeRecognizer::addSample()" << endl;
		LTKReturnError(errorCode);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting NNShapeRecognizer::addSample"<<endl;
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 07-06-2007
 * NAME			: deleteClass
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::deleteClass(int shapeID)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering NNShapeRecognizer::deleteClass" <<endl;

    LTKShapeSample shapeSampleFeatures;
    vector<LTKShapeSample>::iterator prototypeSetIter;

    int prototypeSetSize = m_prototypeSet.size();

    if(!m_projectTypeDynamic)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EPROJ_NOT_DYNAMIC << " " <<
            "Not allowed to delete shapes to a project with fixed number of Shapes"<<
            " NNShapeRecognizer::deleteClass()" << endl;

        LTKReturnError(EPROJ_NOT_DYNAMIC);
    }

    //Validate Input Parameters - shapeID
    if(m_shapeIDNumPrototypesMap.find(shapeID) ==m_shapeIDNumPrototypesMap.end())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_SHAPEID << " " <<
            "shapeID is not valid"<<
            " NNShapeRecognizer::deleteClass()" << endl;

        LTKReturnError(EINVALID_SHAPEID);
    }

    //Update m_prototypeSet
    int k =0;
    for (int i=0;i<prototypeSetSize;i++)
    {
        prototypeSetIter = m_prototypeSet.begin() + k;
        int classId = (*prototypeSetIter).getClassID();

        if(classId == shapeID)
        {
            m_prototypeSet.erase(prototypeSetIter);
            continue;
        }
        k++;
        prototypeSetIter++;
    }
    //Update m_shapeIDNumPrototypesMap
    m_shapeIDNumPrototypesMap.erase(shapeID);

    //Update MDT File
    int returnStatus = writePrototypeSetToMDTFile();

    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: " << returnStatus << " " <<
            "Exiting NNShapeRecognizer::deleteClass" <<endl;
        LTKReturnError(returnStatus);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting NNShapeRecognizer::deleteClass" <<endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 09-06-2007
 * NAME			: writePrototypeSetToMDTFile
 * DESCRIPTION	: Creates a MDTFile with updated m_prototypeSet
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::writePrototypeSetToMDTFile()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::writePrototypeSetToMDTFile()" << endl;

    int returnStatus = SUCCESS;

    //Flush to MDT only after m_MDTUpdateFreq modifications
    m_prototypeSetModifyCount++;
    if(m_prototypeSetModifyCount == m_MDTUpdateFreq)
    {
        m_prototypeSetModifyCount = 0;

        ofstream mdtFileHandle;
        vector<LTKShapeSample> vecShapeSampleFeatures;
        LTKShapeSample shapeSampleFeatures;

        vector<LTKShapeSample>::iterator prototypeSetIter;

        int prototypeSetSize = m_prototypeSet.size();

        //Open the Model data file for writing mode
        if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
        {
            mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::out);
        }
        else
        {
            mdtFileHandle.open(m_nnMDTFilePath.c_str(),ios::out|ios::binary);
        }


        //Throw an error if unable to open the Model data file
        if(!mdtFileHandle)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
                getErrorMessage(EMODEL_DATA_FILE_OPEN) <<
                " NNShapeRecognizer::writePrototypeSetToMDTFile()" << endl;

            LTKReturnError(EMODEL_DATA_FILE_OPEN);
        }


        if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
        {
            //Write the number of Shapes
            mdtFileHandle << 0 << endl; //Representing Dynamic
        }
        else
        {
            int numShapes = 0;
            mdtFileHandle.write((char*) &numShapes, sizeof(unsigned short));
        }
        prototypeSetIter = m_prototypeSet.begin();

        for (int i=0;i<prototypeSetSize;i++)
        {
            int classId = (*prototypeSetIter).getClassID();
            shapeSampleFeatures.setClassID(classId);
            shapeSampleFeatures.setFeatureVector((*prototypeSetIter).getFeatureVector());

            vecShapeSampleFeatures.push_back(shapeSampleFeatures);
            prototypeSetIter++;
        }

        returnStatus = appendPrototypesToMDTFile(vecShapeSampleFeatures,mdtFileHandle);

        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< returnStatus << " " <<
                " NNShapeRecognizer::writePrototypeSetToMDTFile()" << endl;

            LTKReturnError(returnStatus);
        }

        mdtFileHandle.close();

        //Updating the Header Information
        updateHeaderWithAlgoInfo();

        //Adding header information	and checksum generation
        string strModelDataHeaderInfoFile = "";
        LTKCheckSumGenerate cheSumGen;

        returnStatus = cheSumGen.addHeaderInfo(
                strModelDataHeaderInfoFile,
                m_nnMDTFilePath,
                m_headerInfo
                );

        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  returnStatus << " " <<
                " NNShapeRecognizer::writePrototypeSetToMDTFile()" << endl;

            LTKReturnError(returnStatus);
        }

        vecShapeSampleFeatures.clear();
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NNShapeRecognizer::writePrototypeSetToMDTFile"<<endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 09-06-2007
 * NAME			: Add Sample to m_prototypeSet
 * DESCRIPTION	: Add  LTKShapeSample to m_prototypeSet
 *
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::insertSampleToPrototypeSet(const LTKShapeSample &shapeSampleFeatures)
{
    //Error??

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering NNShapeRecognizer::insertSampleToPrototypeSet"<<endl;

    vector<LTKShapeSample>::iterator prototypeSetIter;
    int classID = shapeSampleFeatures.getClassID();
    int maxClassID;

    int prototypeSize = m_prototypeSet.size();
    if(prototypeSize > 0)
    {
        maxClassID = m_prototypeSet.at(prototypeSize-1).getClassID();
    }
    else
    {
        maxClassID = LTK_START_SHAPEID;
    }

    prototypeSetIter = m_prototypeSet.begin();

    if(classID >= maxClassID)
    {
        m_prototypeSet.push_back(shapeSampleFeatures);
    }
    else
    {
        for(;prototypeSetIter!=m_prototypeSet.end();)
        {
            int currentClassId = (*prototypeSetIter).getClassID();

            if(currentClassId >= classID)
            {
                m_prototypeSet.insert(prototypeSetIter,shapeSampleFeatures);
                break;
            }
            int count = m_shapeIDNumPrototypesMap[currentClassId];
            prototypeSetIter = prototypeSetIter + count; //To point to first sample of next classID
        }
    }
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting NNShapeRecognizer::insertSampleToPrototypeSet"<<endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR                : Vandana Roy
 * DATE                  : 05-07-2007
 * NAME                  : computeConfidence
 * DESCRIPTION   :
 * ARGUMENTS             :
 * RETURNS               :
 * NOTES                 :
 * CHANGE HISTROY
 * Author                        Date                            Description
 ******************************************************************************/
int NNShapeRecognizer::computeConfidence()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::computeConfidence()" << endl;

    /******************************************************************/
    /*******************VALIDATING INPUT ARGUMENTS*********************/
    /******************************************************************/
    if(m_neighborInfoVec.empty())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENEIGHBOR_INFO_VECTOR_EMPTY << " " <<
            getErrorMessage(ENEIGHBOR_INFO_VECTOR_EMPTY) <<
            " NNShapeRecognizer::computeConfidence()" << endl;

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

    // Confidence computation for the NN (1-NN) Classifier
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
                //outResult.setShapeId( (*distIndexPairIter).classId );
                classIdSimilarityPair.first = (*distIndexPairIter).classId ;
                float similarityValue = SIMILARITY((*distIndexPairIter).distance);
                //outResult.setConfidence(similarityValue);
                classIdSimilarityPair.second = similarityValue;
                similaritySum += similarityValue;
                //m_vecRecoResult.push_back(outResult);
                classIdSimilarityPairVec.push_back(classIdSimilarityPair);
                distinctClassVector.push_back((*distIndexPairIter).classId);
            }
        }

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
//        vector<pair<int,float> > classIdSimilarityPairVec;
  //      pair<int, float> classIdSimilarityPair;

        // Variable to store the maximum of the number of samples per class.
        int maxClassSize = (max_element(m_shapeIDNumPrototypesMap.begin(), m_shapeIDNumPrototypesMap.end(), &compareMap))->second;
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "maxClassSize: " <<maxClassSize<< endl;

        // Vector to store the cumulative similarity values
        vector<float> cumulativeSimilaritySum;

        // Populate the values in cumulativeSimilaritySum vector for the top m_nearestNeighbors prototypes
        // Assumption is m_nearestNeighbors >= MIN_NEARESTNEIGHBORS and
		// m_nearestNeighbors < dtwEuclideanFilter, validation done in recognize()

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Displaying cumulativeSimilaritySum..." << endl;
        int i = 0;
        for( i = 0 ; i < m_nearestNeighbors ; ++i)
        {
            //outResult.setShapeId(m_neighborInfoVec[i].classId);
            classIdSimilarityPair.first = m_neighborInfoVec[i].classId;
            float similarityValue = SIMILARITY((m_neighborInfoVec[i]).distance);
//            outResult.setConfidence(similarityValue);
            classIdSimilarityPair.second = similarityValue;
//            classIdSimilarityPairVector.push_back(outResult);
            classIdSimilarityPairVec.push_back(classIdSimilarityPair);
            similaritySum += similarityValue;
            cumulativeSimilaritySum.push_back(similaritySum);

            // Logging the cumulative similarity values for debugging
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "classID:" <<
                m_neighborInfoVec[i].classId << " confidence:" <<
                similarityValue << endl;

            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << i << ": " << similaritySum << endl;
        }

//        for(i = 0 ; i < classIdSimilarityPairVector.size() ; ++i)
        for(i = 0 ; i < classIdSimilarityPairVec.size() ; ++i)
        {
//            int classID = classIdSimilarityPairVector[i].getShapeId();
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
			" NNShapeRecognizer::computeConfidence()" << endl;
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

    //Logging the results at info level
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Printing the Classes and respective Confidences" <<endl;

    for( int i=0; i < m_vecRecoResult.size() ; i++)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Class ID: " <<m_vecRecoResult[i].getShapeId()
            <<" Confidence: "<<m_vecRecoResult[i].getConfidence()
            <<endl;
    }

    distinctClassVector.clear();

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::computeConfidence()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR                : Vandana Roy
 * DATE                  : 05-07-2007
 * NAME                  : sortResultByConfidence
 * DESCRIPTION  	 : Sort the LTKShapeRecoResult vector based on the Confidence value
 * ARGUMENTS             :
 * RETURNS               :
 * NOTES                 :
 * CHANGE HISTROY
 * Author                        Date                            Description
 ******************************************************************************/

bool NNShapeRecognizer::sortResultByConfidence(const LTKShapeRecoResult& x, const LTKShapeRecoResult& y)
{
    return (x.getConfidence()) > (y.getConfidence());
}
/******************************************************************************
 * AUTHOR                : Vandana Roy
 * DATE                  : 05-07-2007
 * NAME                  : compareMap
 * DESCRIPTION  	 : Sort the STL's map based on the 'value'(second) field
 * ARGUMENTS             :
 * RETURNS               :
 * NOTES                 :
 * CHANGE HISTROY
 * Author                        Date                            Description
 ******************************************************************************/

bool NNShapeRecognizer::compareMap( const map<int, int>::value_type& lhs, const map<int, int>::value_type& rhs )
{
    return lhs.second < rhs.second;
}

/******************************************************************************
 * AUTHOR		: Tarun Madan
 * DATE			: 08-07-2007
 * NAME			: getTraceGroup
 * DESCRIPTION	:
 *
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NNShapeRecognizer::getTraceGroups(int shapeID, int numberOfTraceGroups,
        vector<LTKTraceGroup> &outTraceGroups)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Entering NNShapeRecognizer::getTraceGroups"
        <<endl;

    if(m_shapeIDNumPrototypesMap.find(shapeID) == m_shapeIDNumPrototypesMap.end())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_SHAPEID << " " <<
            getErrorMessage(EINVALID_SHAPEID) <<
            " NNShapeRecognizer::getTraceGroups()" << endl;
        LTKReturnError(EINVALID_SHAPEID);
    }

    if(m_shapeIDNumPrototypesMap[shapeID] < numberOfTraceGroups)
    {
        numberOfTraceGroups = m_shapeIDNumPrototypesMap[shapeID];
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)
            << "Number of TraceGroup in PrototypeSet is less than specified."
            << "Returning all TraceGroups :"
            << numberOfTraceGroups <<endl;
    }

    vector<LTKShapeSample>::iterator prototypeSetIter = m_prototypeSet.begin();
    int counter =0;
    for(;prototypeSetIter!=m_prototypeSet.end();)
    {
        int currentShapeId = (*prototypeSetIter).getClassID();

        if(currentShapeId == shapeID)
        {
            LTKTraceGroup traceGroup;

            int errorCode = m_ptrFeatureExtractor->convertFeatVecToTraceGroup(
                    (*prototypeSetIter).getFeatureVector(),
                    traceGroup);
            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                    " NNShapeRecognizer::getTraceGroups()" << endl;

                LTKReturnError(errorCode);
            }
            outTraceGroups.push_back(traceGroup);

            counter++;
            if(counter==numberOfTraceGroups)
                break;

            prototypeSetIter++;
        }
        else
        {
            //To point to first sample of next classID
            int offset = m_shapeIDNumPrototypesMap[currentShapeId];
            prototypeSetIter = prototypeSetIter + offset;
        }

    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NNShapeRecognizer::getTraceGroups"
        <<endl;
    return SUCCESS;
}

/***********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 19-01-2007
 * NAME			: initializePreprocessor
 * DESCRIPTION	: This method is used to initialize the PreProcessor
 * ARGUMENTS		: preprocDLLPath : string : Holds the Path of the Preprocessor DLL,
 *				  returnStatus    : int    : Holds SUCCESS or Error Values, if occurs
 * RETURNS		: preprocessor instance
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/
int NNShapeRecognizer::initializePreprocessor(const LTKControlInfo& controlInfo,
        LTKPreprocessorInterface** preprocInstance)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::initializePreprocessor()" << endl;

    FN_PTR_CREATELTKLIPIPREPROCESSOR createLTKLipiPreProcessor = NULL;
    int errorCode;

    // Load the DLL with path=preprocDLLPath
    void* functionHandle = NULL;

    int returnVal = m_OSUtilPtr->loadSharedLib(controlInfo.lipiLib, PREPROC, &m_libHandler);

    
	if(returnVal != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  ELOAD_PREPROC_DLL << " " <<
            getErrorMessage(ELOAD_PREPROC_DLL) <<
            " NNShapeRecognizer::initializePreprocessor()" << endl;
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
            " NNShapeRecognizer::initializePreprocessor()" << endl;

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
            " NNShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(EDLL_FUNC_ADDRESS_CREATE);
	}
    
    m_deleteLTKLipiPreProcessor = (FN_PTR_DELETELTKLIPIPREPROCESSOR)functionHandle;
    
    // Create preprocessor instance
    errorCode = createLTKLipiPreProcessor(controlInfo, preprocInstance);

    if(errorCode!=SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  errorCode << " " <<
            " NNShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(errorCode);
    }

    // Could not create a LTKLipiPreProcessor
    if(*preprocInstance == NULL)
    {
        // Unload the DLL
        unloadPreprocessorDLL();

        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ECREATE_PREPROC << " " <<
            getErrorMessage(ECREATE_PREPROC) <<
            " NNShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(ECREATE_PREPROC);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::initializePreprocessor()" << endl;

    return SUCCESS;

}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 23-Jan-2007
 * NAME			: trainLvq
 * DESCRIPTION	: This function is the train method using LVQ
 * ARGUMENTS		:
 * RETURNS		: SUCCESS : if training done successfully
 *				  errorCode : if traininhas some errors
 * NOTES			:
 * CHANGE HISTROY
 * Author	Naveen Sundar G		Date				Description
 * Balaji MNA                   13th Jan, 2011      Prototype set must be emptied before 
 *                                                  returning from call to LVQ train     
 *************************************************************************************/
int NNShapeRecognizer::trainLVQ(const string& inputFilePath,
		const string &strModelDataHeaderInfoFile,
		const string& inFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::trainLVQ()" << endl;
    //Time calculation requirements

    int errorCode = SUCCESS;
    m_OSUtilPtr->recordStartTime();

    //Time at the beginning of Train LVQ
    if(LTKSTRCMP(inFileType.c_str(), INK_FILE) == 0)
    {
        //If the Input file is UNIPEN Ink file
        errorCode = trainFromListFile(inputFilePath);
        if(errorCode != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                " NNShapeRecognizer::trainLVQ()" << endl;

            LTKReturnError(errorCode);
        }
    }
    else if(LTKSTRCMP(inFileType.c_str(), FEATURE_FILE) == 0)
    {
        //If the Input file is Feature file
        errorCode = trainFromFeatureFile(inputFilePath);
        if(errorCode != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                " NNShapeRecognizer::trainLVQ()" << endl;

            LTKReturnError(errorCode);
        }
		PreprocParametersForFeatureFile(m_headerInfo);

    }

    // Now the Clustered ShapeSamples are stored in "prototypeSet"
    // Already computed "trainVec" contains all of the training data

    //Learning Part
    if(m_prototypeReductionFactor != 0)
    {
        errorCode = processPrototypeSetForLVQ();
        if(errorCode != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                " NNShapeRecognizer::trainLVQ()" << endl;

            LTKReturnError(errorCode);
        }
    }

    ofstream mdtFileHandle;

    //Open the model data file
    if (m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::app);
    }
    else
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::app | ios::binary);
    }

    //If file not opened throw an exception
    if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            " Unable to open model data file : " <<m_nnMDTFilePath<<
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(EMODEL_DATA_FILE_OPEN);
    }
    //Writing results to the Model Data file
    errorCode = appendPrototypesToMDTFile(m_prototypeSet,mdtFileHandle);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::trainLVQ()" << endl;

        LTKReturnError(errorCode);
    }

    //Close the Model Data file
    mdtFileHandle.close();

    //Updating the Header Information
    updateHeaderWithAlgoInfo();

    //Adding header information	and checksum generation
    LTKCheckSumGenerate cheSumGen;
    errorCode = cheSumGen.addHeaderInfo(strModelDataHeaderInfoFile, m_nnMDTFilePath, m_headerInfo);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::trainLVQ()" << endl;

        LTKReturnError(errorCode);
    }

    //Time at the end of LVQ training
    m_OSUtilPtr->recordEndTime();

	m_prototypeSet.clear();

    string timeTaken = "";
    m_OSUtilPtr->diffTime(timeTaken);

    cout << "Time Taken  = " << timeTaken << endl;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::trainLVQ()" << endl;
    return SUCCESS;
}
/******************************************************************************
 * AUTHOR		: Saravanan
 * DATE			: 22-02-2007
 * NAME			: processPrototypeSetForLVQ
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author Naveen Sundar G.	Date 11-Oct-2007	Description
 ******************************************************************************/
int NNShapeRecognizer::processPrototypeSetForLVQ()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::processPrototypeSetForLVQ()" << endl;
    //Reference : http://www.cis.hut.fi/research/lvq_pak/lvq_doc.txt
    LTKShapeSample bestShapeSample;

	int codeVecIndex = 5;
	int trainSize = m_trainSet.size() ;
	int train_index = 0;

	//Number of iterations for LVQ
	long length = m_prototypeSet.size() * m_LVQIterationScale;
	long iter ;
	long index;
	// learning parameter
	double c_alpha=m_LVQInitialAlpha;

        int errorCode = SUCCESS;

	// initialize random seed
	unsigned int randSeedVal ;

	#ifdef WINCE
		char szTime[10] ;
		SYSTEMTIME st ;
		GetLocalTime(&st) ;
		sprintf(szTime, "%d%d%d", st.wHour, st.wMinute, st.wSecond) ;
		randSeedVal = atoi(szTime);
	#else
		randSeedVal = time(NULL);
	#endif
		srand(randSeedVal) ;

	for (iter=0; iter < length; ++iter )
	{


		cout<<"\n Amount of LVQ Training Completed = "<<(double)iter*100/length<<" %\n\n Current Value of Alpha \t  = "<<c_alpha<<"\n";
		//To take the train vector at a random index
                index = rand()%trainSize;
                errorCode = trainRecognize(m_trainSet.at(index), bestShapeSample, codeVecIndex);
                
                if(errorCode != SUCCESS)
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                        " NNShapeRecognizer::morphVector()" << endl;

                    LTKReturnError(errorCode);
                }


                if(bestShapeSample.getClassID() == m_trainSet.at(index).getClassID())
                {
                    //Move the codeVec closer (Match)
                    c_alpha = linearAlpha(iter,length,m_LVQInitialAlpha,c_alpha,+1);
                    errorCode = morphVector(m_trainSet.at(index), -c_alpha, bestShapeSample);
                    if(errorCode != SUCCESS)
                    {
                        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                            " NNShapeRecognizer::morphVector()" << endl;

                        LTKReturnError(errorCode);
                    }
                }
                else
                {
                    //Move the codeVec away (No Match)
                    c_alpha = linearAlpha(iter,length,m_LVQInitialAlpha,c_alpha,-1);
                    errorCode = morphVector(m_trainSet.at(index), c_alpha, bestShapeSample);
                    if(errorCode != SUCCESS)
                    {
                        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                            " NNShapeRecognizer::morphVector()" << endl;

                        LTKReturnError(errorCode);
                    }
                }

                //Now update the prototypeSet with the morphed vector

                const vector<LTKShapeFeaturePtr>& tempFeatVec = (bestShapeSample).getFeatureVector();
                m_prototypeSet.at(codeVecIndex).setFeatureVector(tempFeatVec);
	}

	m_trainSet.clear();

	cout<<"\n Amount of LVQ Training Completed = "<<(double) 100<<" %\n\n Current Value of Alpha \t  = "<<c_alpha<<"\n\n\n";


    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::processPrototypeSetForLVQ()" << endl;
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: N. Sridhar Krishna
 * DATE			: 24-03-2006
 * NAME			: linearAlpha
 * DESCRIPTION	: this function is called from trainLVQ - linearly decreasing learning parameter in learing vector quantization
 * ARGUMENTS		:
 * RETURNS		: learning parameter (alpha)
 * NOTES			:
 * CHANGE HISTROY
 * Author Naveen Sundar G.	Date 11-Oct-2007	Description
 ******************************************************************************/
float NNShapeRecognizer:: linearAlpha(long iter, long length, double& initialAlpha, double lastAlpha,int correctDecision)
{

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::linearAlpha()" << endl;
	// return ( (initialAlpha * ( ( (double) (length - iter)) / (double) length )));
	// return (alpha *( (double ) 1/iter));

	//  Reference : http://www.cis.hut.fi/research/lvq_pak/lvq_doc.txt

	float currentAlpha;
	currentAlpha=lastAlpha/(1+correctDecision*lastAlpha);

	if (currentAlpha >initialAlpha)
		currentAlpha=initialAlpha;

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::linearAlpha()" << endl;
	
        return currentAlpha;

}

/**********************************************************************************
* AUTHOR		: N. Sridhar Krishna
* DATE			: 24-03-2006
* NAME			: morphVector
* DESCRIPTION	: This function does the reshaping of prototype vector (called from trainLVQ)
* ARGUMENTS		: The input parameters are the code vector, data vector (learning example in the context of LVQ), and alpha (learning parameter)
*				  @param bestcodeVec is the character which we are trying to morph
*						 the function modifies the character bestcodeVec
* RETURNS		: SUCCESS on successful training
* NOTES			:
* CHANGE HISTROY
* Author Naveen Sundar G. Date				Description
*************************************************************************************/
int NNShapeRecognizer::morphVector(const LTKShapeSample& dataShapeSample,
		double talpha, LTKShapeSample& bestShapeSample)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Enter NNShapeRecognizer::morphVector"<<endl;

	vector<LTKShapeFeaturePtr> bestFeatureVector = bestShapeSample.getFeatureVector();
	const vector<LTKShapeFeaturePtr>& dataFeatureVector = dataShapeSample.getFeatureVector();

	int index=0;
	int bestFVSize = bestFeatureVector.size();
	int dataFVSize = dataFeatureVector.size();

        int errorCode = SUCCESS;

	if(bestFVSize !=dataFVSize)
	{
		LTKReturnError(EMORPH_FVEC_SIZE_MISMATCH);
	}

	float temp1 = 0;



	for(index=0; index < bestFVSize ; ++index)
        {
            LTKShapeFeaturePtr temp1;
            LTKShapeFeaturePtr temp2;
            LTKShapeFeaturePtr temp3;


            errorCode = bestFeatureVector[index]->subtractFeature(dataFeatureVector[index],temp1);
            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                    " NNShapeRecognizer::morphVector()" << endl;

                LTKReturnError(errorCode);
            }

            errorCode = temp1->scaleFeature(talpha,temp2);
            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                    " NNShapeRecognizer::morphVector()" << endl;

                LTKReturnError(errorCode);
            }

            errorCode = bestFeatureVector[index]->addFeature(temp2,temp3);
            if(errorCode != SUCCESS)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                    " NNShapeRecognizer::morphVector()" << endl;

                LTKReturnError(errorCode);
            }

            bestFeatureVector[index] = temp3;

        }

        bestShapeSample.setFeatureVector(bestFeatureVector);

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exit NNShapeRecognizer::morphVector"<<endl;

	return SUCCESS;

}

/**********************************************************************************
 * AUTHOR               : N. Sridhar Krishna
 * DATE                 : 23-03-2006
 * NAME                 : trainRecognize (overloaded with 4 args)
 * DESCRIPTION          : This function does the recognition function required for training phase (called from trainLVQ)
 * ARGUMENTS            : The input parameter are the inFeatureVector, which is compared with the existing set of prototypes and then the matched code vector and along with its index (and also the shape id) is returned
 * @param inFeatureVector is the character which we are trying to recognise.
 * @param returnshapeID is the value of the matched character which is returned, codeCharacter is the matched prototype (code vector) vector, and codeVecIndex is the matched prototype (code vector) index
 * RETURNS              : SUCCESS on successful reading of the allocation statistics
 * NOTES                        :
 * CHANGE HISTROY
 * Author                       Date                            Description
 *************************************************************************************/
int NNShapeRecognizer::trainRecognize(LTKShapeSample& inShapeSample,
		LTKShapeSample& bestShapeSample,int& codeVecIndex)

{

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::trainRecognize()" << endl;
	//Variable to store the Euclidean distance.
	float localDistance = 0.0;

	//Iterator for prototypeSet
	vector <LTKShapeSample>::const_iterator prototypeSetIter = m_prototypeSet.begin();
	vector <LTKShapeSample>::const_iterator prototypeSetIterEnd = m_prototypeSet.end();

	//The top choice index
	int bestIndex = 0;

	//The top choice distance (FLT_MAX indicates the maximum value for float)
	float bestMinDist = FLT_MAX;

        int errorCode = SUCCESS;

	for(int j = 0; prototypeSetIter != prototypeSetIterEnd; ++prototypeSetIter, j++)
	{
		localDistance=0;
		if(LTKSTRCMP(m_LVQDistanceMeasure.c_str(), EUCLIDEAN_DISTANCE) == 0)
		{
			errorCode = computeEuclideanDistance(*prototypeSetIter,
					   				inShapeSample,
									localDistance);

                        if(errorCode != SUCCESS)
                        {
                            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                                " NNShapeRecognizer::trainRecognize()" << endl;

                            LTKReturnError(errorCode);
                        }
		}
		if(LTKSTRCMP(m_LVQDistanceMeasure.c_str(), DTW_DISTANCE) == 0)
		{
			errorCode = computeDTWDistance(*prototypeSetIter,
					   				inShapeSample,
									localDistance);

                        if(errorCode != SUCCESS)
                        {
                            LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
                                " NNShapeRecognizer::trainRecognize()" << endl;

                            LTKReturnError(errorCode);
                        }
		}

		//BestSofar Value for BSF computation using Euclidean distance
		if(bestMinDist > localDistance )
		{
			//bestMinDist is the Minimum Distance
			bestMinDist = localDistance;
			//bestIndex is the best match for the given character
			bestIndex = j;
		}
	}

	//Get the shape id of the best match from the prototypeSet
	bestShapeSample.setClassID((m_prototypeSet.at(bestIndex)).getClassID());

	//Get the Feature vector of the best match from the prototypeSet
	const vector<LTKShapeFeaturePtr>& tempFeatureVector =
							(m_prototypeSet.at(bestIndex)).getFeatureVector();
	bestShapeSample.setFeatureVector(tempFeatureVector);

	codeVecIndex = bestIndex ;

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::trainRecognize()" << endl;
	return SUCCESS;
}


/**********************************************************************************
 * AUTHOR		: Saravanan. R
 * DATE			: 25-01-2007
 * NAME			: deletePreprocessor
 * DESCRIPTION	: This method is used to deletes the PreProcessor instance
 * ARGUMENTS		: ptrPreprocInstance : Holds the pointer to the LTKPreprocessorInterface
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 *************************************************************************************/

//int NNShapeRecognizer::deletePreprocessor(LTKPreprocessorInterface *ptrPreprocInstance)
int NNShapeRecognizer::deletePreprocessor()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::deletePreprocessor()" << endl;

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
            " NNShapeRecognizer::deletePreprocessor()" << endl;
        LTKReturnError(returnStatus);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::deletePreprocessor()" << endl;

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 29-01-2007
 * NAME			: unloadPreprocessorDLL
 * DESCRIPTION	: This method is used to Unloads the preprocessor DLL.
 * ARGUMENTS		: none
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ****************************************************************************/
int NNShapeRecognizer::unloadPreprocessorDLL()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::unloadPreprocessorDLL()" << endl;


    //Check the preprocessor DLL was loaded already
    if(m_libHandler != NULL)
    {
        //Unload the DLL
		m_OSUtilPtr->unloadSharedLib(m_libHandler);
        m_libHandler = NULL;
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::unloadPreprocessorDLL()" << endl;

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Srinivasa Vithal, Ch
 * DATE			: 20-06-2008
 * NAME			: validatePreprocParameters
 * DESCRIPTION	: This method is used to validate the preproc parameters with
 *						mdt header values
 * ARGUMENTS		: none
 * RETURNS		: none
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description
 ****************************************************************************/
int NNShapeRecognizer::validatePreprocParameters(stringStringMap& headerSequence)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::validatePreprocParameters()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
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
				" NNShapeRecognizer::loadModelData()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
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
				" NNShapeRecognizer::loadModelData()" << endl;
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
				" NNShapeRecognizer::loadModelData()" << endl;
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
				" NNShapeRecognizer::loadModelData()" << endl;
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
            " NNShapeRecognizer::loadModelData()" << endl;
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
				" NNShapeRecognizer::loadModelData()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::validatePreprocParameters()" << endl;
	return SUCCESS;

}
/******************************************************************************
 * AUTHOR		: Saravanan
 * DATE			: 22-02-2007
 * NAME			: trainFromFeatureFile
 * DESCRIPTION	: This method will do the training by giving the Feature
 *				  file as input
 * ARGUMENTS	:
 * RETURNS		: none
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int NNShapeRecognizer::trainFromFeatureFile(const string& featureFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::trainFromFeatureFile()" << endl;

    //Count for the no. of samples read for a shape
    int sampleCount = 0;

    //Count of the no. of shapes read so far
    int shapeCount = 0;

    //ID for each shapes
    int shapeId = -1;

    //classId of the character
    int prevClassId = -1;

    //Flag to skip reading a newline in the list file, when a new class starts
    bool lastshapeIdFlag = false;

    //Flag is set when EOF is reached
    bool eofFlag = false;

    //Line from the list file
    string line = "";

    //Indicates the first class
    bool initClassFlag = false;

    //Output Stream for MDT file
    ofstream mdtFileHandle;

    //Input Stream for feature file
    ifstream featureFileHandle;

    LTKShapeSample shapeSampleFeatures;

    vector<LTKShapeSample> shapeSamplesVec;

    vector<LTKShapeSample> clusteredShapeSampleVec;


    //Opening the feature file for reading mode
    featureFileHandle.open(featureFilePath.c_str(), ios::in);

    //Throw an error if unable to open the training list file
    if(!featureFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EFEATURE_FILE_OPEN << " " <<
            getErrorMessage(EFEATURE_FILE_OPEN) <<
            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(EFEATURE_FILE_OPEN);

    }
    //Open the Model data file for writing mode
    if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::out);
    }
    else
    {
        mdtFileHandle.open(m_nnMDTFilePath.c_str(), ios::out|ios::binary);
    }

    //Return  error if unable to open the Model data file
    if(!mdtFileHandle)
    {
        featureFileHandle.close();
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            getErrorMessage(EMODEL_DATA_FILE_OPEN) <<
            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(EMODEL_DATA_FILE_OPEN);
    }

	//Reading feature file header
    getline(featureFileHandle, line, NEW_LINE_DELIMITER);
    stringStringMap headerSequence;
    int errorCode = SUCCESS;
    errorCode = m_shapeRecUtil.convertHeaderToStringStringMap(line, headerSequence);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(errorCode);
    }



    //Write the number of Shapes
    mdtFileHandle << m_numShapes << endl;

    //write Trace Dimension of input vector into the file
	 // mdtFileHandle << m_traceDimension << endl;


    while(!featureFileHandle.eof())
    {
        if( lastshapeIdFlag == false )
        {
            //Get a line from the feature file
            getline(featureFileHandle, line, NEW_LINE_DELIMITER);

            if( featureFileHandle.eof() )
            {
                eofFlag = true;
            }

            if((getShapeSampleFromString(line, shapeSampleFeatures) != SUCCESS) && (eofFlag == false) )
                continue;

            shapeId = shapeSampleFeatures.getClassID();

			if(eofFlag == false)
			{
				if(shapeId < 0)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
						"The NN Shape recognizer requires training file class Ids to be positive integers and listed in the increasing order" <<
						" NNShapeRecognizer::trainFromFeatureFile()" << endl;
					errorCode = EINVALID_SHAPEID;
					break;
				}
				else if(shapeId < prevClassId)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
						"Shape IDs in the train list file should be in the increasing order. Please use scripts/validateListFile.pl to generate list files." <<
						" NNShapeRecognizer::trainFromFeatureFile()" << endl;
					errorCode = EINVALID_ORDER_LISTFILE;
					break;
				}
			}

            if( initClassFlag == false )
            {
                initClassFlag = true;
                prevClassId=shapeId;
            }

        }
        else //Do not read next line during this iteration
        {
            //flag unset to read next line during the next iteration
            lastshapeIdFlag = false;
        }
        // Sample of the same class seen, keep pushing to the shapeSamplesVec
        if( shapeId == prevClassId )
        {
            shapeSamplesVec.push_back(shapeSampleFeatures);
            ++sampleCount;
            //All the samples are pushed to trainSet used only for trainLVQ
            //trainSet was NULL for Clustering and not NULL for LVQ
            if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_LVQ)
					== 0 && m_prototypeReductionFactor != 0)
				m_trainSet.push_back(shapeSampleFeatures);

            shapeSampleFeatures.clearShapeSampleFeatures();
        }
        // Sample of a new class seen, or end of feature file reached, train the recognizer on the samples of the previous class
        if( shapeId != prevClassId || eofFlag == true )
        {
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
                errorCode = performClustering(shapeSamplesVec, clusteredShapeSampleVec);

                if( errorCode != SUCCESS )
                {
                    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                        " NNShapeRecognizer::trainFromFeatureFile()" << endl;
                    LTKReturnError(errorCode);
                }


                if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_LVQ) == 0)
                {
                    //Push all the samples after clustering into prototypeSet
                    for( int i = 0; i < clusteredShapeSampleVec.size(); ++i )
                    {
                        m_prototypeSet.push_back(clusteredShapeSampleVec[i]);
                    }
                }
                else if(LTKSTRCMP(m_prototypeSelection.c_str(), PROTOTYPE_SELECTION_CLUSTERING) == 0)
                {
                    //Writing results to the MDT file
                    errorCode = appendPrototypesToMDTFile(clusteredShapeSampleVec, mdtFileHandle);
                    if( errorCode != SUCCESS )
                    {
                        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
                        LTKReturnError(errorCode);
                    }
                }

                //Clearing the shapeSampleVector and clusteredShapeSampleVector

                clusteredShapeSampleVec.clear();
                shapeSamplesVec.clear();
                //Resetting sampleCount for the next class
                sampleCount = 0;

                //Set the flag so that the already read line of next class in the list file is not lost
                lastshapeIdFlag = true;

                prevClassId = shapeId;
            }
        }
    }

    featureFileHandle.close();
    mdtFileHandle.close();

    if(!m_projectTypeDynamic && shapeCount != m_numShapes)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_SHAPES << " " <<
            getErrorMessage(EINVALID_NUM_OF_SHAPES) <<
            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(EINVALID_NUM_OF_SHAPES);
    }

    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NNShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::trainFromFeatureFile()" << endl;

    return SUCCESS;
}

/**************************************************************************
 * AUTHOR		: Balaji MNA
 * DATE			: 01-DEC-2008
 * NAME			: validatePreprocParameters
 * DESCRIPTION	: This method is used to update the preproc parameters for
 *				  featurefile
 * ARGUMENTS	: none
 * RETURNS		: none
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ****************************************************************************/
int NNShapeRecognizer::PreprocParametersForFeatureFile(stringStringMap& headerSequence)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NNShapeRecognizer::PreprocParametersForFeatureFile()" << endl;

	//preproc sequence
    headerSequence[PREPROC_SEQ] = "NA";
    //ResampTraceDimension
    headerSequence[TRACE_DIM] = "NA";
    // preserve aspect ratio
    headerSequence[PRESER_ASP_RATIO] = "NA";
	//NormPreserveRelativeYPosition
    headerSequence[PRESER_REL_Y_POS] = "NA";
	// NormPreserveAspectRatioThreshold
    headerSequence[ASP_RATIO_THRES] = "NA";
	// NormLineWidthThreshold
	headerSequence[DOT_SIZE_THRES] = "NA";
	// NormDotSizeThreshold
	headerSequence[DOT_THRES] = "NA";
	//ResampPointAllocation
	headerSequence[RESAMP_POINT_ALLOC] = "NA";
	//SmoothWindowSize
	headerSequence[SMOOTH_WIND_SIZE] = "NA";

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NNShapeRecognizer::PreprocParametersForFeatureFile()" << endl;
	return SUCCESS;

}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
* NAME			: adapt
* DESCRIPTION	: adapt recent recognized sample 
* ARGUMENTS		: shapeID : True shapeID of sample
* RETURNS		: Success : If sample was adapted successfully
*				  Failure : Returns Error Code
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int NNShapeRecognizer::adapt(int shapeId)
{
	try{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
				<< "Enter NNShapeRecognizer::adapt()"<<endl;

		//Validate shapeID
		map<int,int>::iterator m_shapeIDNumPrototypesMapIter;
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
				<< "Error during NNShapeRecognizer::adapt()"<<endl;
			LTKReturnError(nErrorCode);
		}

		//Clear Variables cached 
		m_neighborInfoVec.clear();
		m_vecRecoResult.clear();

	}
	catch(...)
	{	
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
				<< "Error during NNShapeRecognizer::adapt()"<<endl;
		return FAILURE;
	}

	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
			<< "Exit NNShapeRecognizer::adapt()"<<endl;
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 30-Aug-2007
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
int NNShapeRecognizer::adapt(const LTKTraceGroup& sampleTraceGroup, int shapeId )
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
			<< "Enter NNShapeRecognizer::Adapt()"<<endl;
	
	vector<int> vecSubSet;
	vector<LTKShapeRecoResult> vecRecoResult;
	LTKScreenContext objScreenContext;
	int nErrorCode;
	nErrorCode = recognize(
					sampleTraceGroup,
					objScreenContext,
					vecSubSet,
					CONF_THRESHOLD_FILTER_OFF,
					NN_DEF_RECO_NUM_CHOICES,
					vecRecoResult
					);

	if(nErrorCode !=0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
			<< "Error during call to recognize in NNShapeRecognizer::Adapt()"<<endl;
		LTKReturnError(nErrorCode);
	}

	nErrorCode = adapt(shapeId);
	if(nErrorCode !=0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) 
			<< "Error during NNShapeRecognizer::Adapt()"<<endl;
		LTKReturnError(nErrorCode);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_INFO) 
			<< "Exit NNShapeRecognizer::Adapt()"<<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Tarun Madan
* DATE			: 8-Oct-2007
* NAME			: deleteAdaptInstance
* DESCRIPTION	: delete AdaptInstance
* ARGUMENTS		: 
* RETURNS		: None
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int NNShapeRecognizer::deleteAdaptInstance()
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
