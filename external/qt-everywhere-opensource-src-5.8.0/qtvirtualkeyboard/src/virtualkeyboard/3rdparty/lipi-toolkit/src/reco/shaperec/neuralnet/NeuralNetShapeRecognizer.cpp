#include "LTKConfigFileReader.h"

#include "NeuralNetShapeRecognizer.h"

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

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: assignDefaultValues
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
void NeuralNetShapeRecognizer::assignDefaultValues()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::assignDefaultValues()" << endl;

    m_numShapes = 0;
    m_neuralnetCfgFilePath = "";
    m_neuralnetMDTFilePath = "";
    m_ptrPreproc = NULL;
    m_projectTypeDynamic=false;
    m_preProcSeqn=NN_DEF_PREPROC_SEQ;
    m_ptrFeatureExtractor=NULL;
    m_featureExtractorName=NN_DEF_FEATURE_EXTRACTOR;
	m_neuralnetNormalizationFactor=NEURALNET_DEF_NORMALIZE_FACTOR;
	m_neuralnetRandomNumberSeed=NEURALNET_DEF_RANDOM_NUMBER_SEED;
	m_neuralnetLearningRate=NEURALNET_DEF_LEARNING_RATE;
	m_neuralnetMomemtumRate=NEURALNET_DEF_MOMEMTUM_RATE;
	m_neuralnetTotalError=NEURALNET_DEF_TOTAL_ERROR;
	m_neuralnetIndividualError=NEURALNET_DEF_INDIVIDUAL_ERROR;
	m_neuralnetNumHiddenLayers=NEURALNET_DEF_HIDDEN_LAYERS_SIZE;
	m_layerOutputUnitVec.push_back(0); // for input layer
	for(int i = 0; i < m_neuralnetNumHiddenLayers; ++i)
	{
		m_layerOutputUnitVec.push_back(NEURALNET_DEF_HIDDEN_LAYERS_UNITS); // for hidden layer
	}
	m_layerOutputUnitVec.push_back(0); // for output layer
	m_layerOutputUnitVec.push_back(0);
	m_isNeuralnetWeightReestimate=false;
	m_neuralnetMaximumIteration=NEURALNET_DEF_MAX_ITR;
	m_isCreateTrainingSequence=true;
    m_rejectThreshold=NN_DEF_REJECT_THRESHOLD;
    m_deleteLTKLipiPreProcessor=NULL;
    m_MDTFileOpenMode = NN_MDT_OPEN_MODE_ASCII;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::assignDefaultValues()" << endl;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: NeuralNetShapeRecognizer
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
NeuralNetShapeRecognizer::NeuralNetShapeRecognizer(const LTKControlInfo& controlInfo):
m_OSUtilPtr(LTKOSUtilFactory::getInstance()),
m_libHandler(NULL),
m_libHandlerFE(NULL)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" << endl;

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


	    //Holds the value of Number of Shapes in string format
	    string strNumShapes = "";

	    string strProfileDirectory = m_lipiRootPath + PROJECTS_PATH_STRING +
	        strProjectName + PROFILE_PATH_STRING;

	    //Holds the path of the Project.cfg
	    string projectCFGPath = strProfileDirectory + PROJECT_CFG_STRING;

	    // Config file

	    m_neuralnetCfgFilePath = m_lipiRootPath + PROJECTS_PATH_STRING +
	        (tmpControlInfo.projectName) + PROFILE_PATH_STRING +
	        (tmpControlInfo.profileName) + SEPARATOR +
	        NEURALNET + CONFIGFILEEXT;


	    //Set the path for neuralnet.mdt
	    m_neuralnetMDTFilePath = strProfileDirectory + strProfileName + SEPARATOR + NEURALNET + DATFILEEXT;

	    //To find whether the project was dynamic or not andto read read number of shapes from project.cfg
	    int errorCode = m_shapeRecUtil.isProjectDynamic(projectCFGPath,
	            m_numShapes, strNumShapes, m_projectTypeDynamic);

	    if( errorCode != SUCCESS)
	    {
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }


	    //Set the NumShapes to the m_headerInfo
	    m_headerInfo[NUMSHAPES] = strNumShapes;

	    //Currently preproc cfg also present in NEURALNET
	    tmpControlInfo.cfgFileName = NEURALNET;
	    errorCode = initializePreprocessor(tmpControlInfo,
	            &m_ptrPreproc);

	    if( errorCode != SUCCESS)
	    {
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }

	    //Reading NEURALNET configuration file
	    errorCode = readClassifierConfig();

	    if( errorCode != SUCCESS)
	    {
			cout<<endl<<"Encountered error in readClassifierConfig"<<endl;
	        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << errorCode << " " <<
	            "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" <<endl;
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
	            "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" <<endl;
	        throw LTKException(errorCode);
	    }

	}
	catch(LTKException e)
	{

		deletePreprocessor();

	    //Unloading the feature Extractor instance
	    deleteFeatureExtractorInstance();

		delete m_OSUtilPtr;
		throw e;
	}
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::NeuralNetShapeRecognizer()" << endl;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: ~NeuralNetShapeRecognizer
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
NeuralNetShapeRecognizer::~NeuralNetShapeRecognizer()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::~NeuralNetShapeRecognizer()" << endl;

	int returnStatus = SUCCESS;

	//clear the traning set
	try{

		m_trainSet.clear();

		//clear all the vector nedded for traning
		m_delW.clear();

		m_previousDelW.clear();

		m_layerOutputUnitVec.clear();

		m_outputLayerContentVec.clear();

		m_targetOutputVec.clear();

		m_connectionWeightVec.clear();
	}
	catch(LTKException e)
	{
		delete m_OSUtilPtr;

		throw e;
	}

	returnStatus = deletePreprocessor();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << returnStatus << " " <<
            " NeuralNetShapeRecognizer::~NeuralNetShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }

    //Unloading the feature Extractor instance
    returnStatus = deleteFeatureExtractorInstance();
    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " <<  returnStatus << " " <<
            " NeuralNetShapeRecognizer::~NeuralNetShapeRecognizer()" << endl;
        throw LTKException(returnStatus);
    }

	delete m_OSUtilPtr;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::~NeuralNetShapeRecognizer()" << endl;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: readClassifierConfig
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::readClassifierConfig()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::readClassifierConfig()" << endl;

    string tempStringVar = "";
    int tempIntegerVar = 0;
    float tempFloatVar = 0.0;
    LTKConfigFileReader *shapeRecognizerProperties = NULL;
    int errorCode = FAILURE;

    try
    {
        shapeRecognizerProperties = new LTKConfigFileReader(m_neuralnetCfgFilePath);
    }
    catch(LTKException e)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO)<< "Info: " <<
            "Config file not found, using default values of the parameters" <<
            "NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

        delete shapeRecognizerProperties;

		return FAILURE;
    }

    //Pre-processing sequence
    errorCode = shapeRecognizerProperties->getConfigValue(PREPROCSEQUENCE, m_preProcSeqn);

    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_INFO) << "Info: " <<
            "Using default value of prerocessing sequence: "<< m_preProcSeqn <<
            " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

        m_preProcSeqn = NN_DEF_PREPROC_SEQ;
    }
	else
	{
		m_headerInfo[PREPROC_SEQ] = m_preProcSeqn;
	}

    if((errorCode = mapPreprocFunctions()) != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<" Error: " << errorCode <<
            " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

        delete shapeRecognizerProperties;

        LTKReturnError(errorCode);
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
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << REJECT_THRESHOLD <<
                " should be in the range (0-1)" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

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
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

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

	//Rendom number seed
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(RANDOM_NUMBER_SEED,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
            m_neuralnetRandomNumberSeed = atoi(tempStringVar.c_str());
            if(m_neuralnetRandomNumberSeed<=0)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << RANDOM_NUMBER_SEED <<
                    " should be zero or a positive integer" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << RANDOM_NUMBER_SEED <<
                " should be zero or a positive integer" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << RANDOM_NUMBER_SEED <<
            " : " << m_neuralnetRandomNumberSeed << endl;
    }

	//Normalised factor
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_NORMALISED_FACTOR,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
		if ( LTKStringUtil::isFloat(tempStringVar) )
        {
			//Writing normalised factor
			m_headerInfo[NORMALISED_FACTOR] = tempStringVar;

            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);

            if(tempFloatVar  > 0 )
            {
                m_neuralnetNormalizationFactor = tempFloatVar;

                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    NEURALNET_NORMALISED_FACTOR << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_NORMALISED_FACTOR <<
                    " should be a positive real number" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_NORMALISED_FACTOR <<
                " should be a positive real number" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_NORMALISED_FACTOR <<
            " : " << m_neuralnetNormalizationFactor << endl;
    }

	//Learning rate
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_LEARNING_RATE,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
		if ( LTKStringUtil::isFloat(tempStringVar) )
        {
			//Writing Learning rate
			m_headerInfo[LEARNING_RATE] = tempStringVar;

			tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);

            if(tempFloatVar  > 0.0 && tempFloatVar <= 1.0)
            {
                m_neuralnetLearningRate = tempFloatVar;

                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    NEURALNET_LEARNING_RATE << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_LEARNING_RATE <<
                    " should be in the range (0-1)" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_LEARNING_RATE <<
                " should be in the range (0-1)" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_LEARNING_RATE <<
            " : " << m_neuralnetLearningRate << endl;
    }

	//Momemtum rate
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_MOMEMTUM_RATE,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
		if ( LTKStringUtil::isFloat(tempStringVar) )
        {
			m_headerInfo[MOMEMTUM_RATE] = tempStringVar;

            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);

            if(tempFloatVar  > 0.0 && tempFloatVar <= 1.0)
            {
                m_neuralnetMomemtumRate = tempFloatVar;

                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    NEURALNET_MOMEMTUM_RATE << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_MOMEMTUM_RATE <<
                    " should be in the range (0-1)" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_MOMEMTUM_RATE <<
                " should be in the range (0-1)" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_MOMEMTUM_RATE <<
            " : " << m_neuralnetMomemtumRate << endl;
    }

	//Total error
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_TOTAL_ERROR,
                                                          tempStringVar);
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isFloat(tempStringVar) )
        {
            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);

            if(tempFloatVar  > 0 && tempFloatVar < 1)
            {
                m_neuralnetTotalError = tempFloatVar;

                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    NEURALNET_TOTAL_ERROR << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_TOTAL_ERROR <<
                    " should be in the range (0-1)" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_TOTAL_ERROR <<
                " should be in the range (0-1)" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_TOTAL_ERROR <<
            " : " << m_neuralnetTotalError << endl;
    }

	//individual error
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_INDIVIDUAL_ERROR,
                                                          tempStringVar);
    if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isFloat(tempStringVar) )
        {
            tempFloatVar = LTKStringUtil::convertStringToFloat(tempStringVar);

            if(tempFloatVar  > 0 && tempFloatVar < 1)
            {
                m_neuralnetIndividualError = tempFloatVar;

                LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
                    NEURALNET_INDIVIDUAL_ERROR << " = " <<tempStringVar <<endl;
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_INDIVIDUAL_ERROR <<
                    " should be in the range (0-1)" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_INDIVIDUAL_ERROR <<
                " should be in the range (0-1)" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_INDIVIDUAL_ERROR <<
            " : " << m_neuralnetIndividualError << endl;
    }

	//hidden layer
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_HIDDEN_LAYERS_SIZE,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
			//Writing number of hidden layer
			m_headerInfo[HIDDEN_LAYER] = tempStringVar;

            m_neuralnetNumHiddenLayers = atoi(tempStringVar.c_str());
            if(m_neuralnetNumHiddenLayers<=0)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_HIDDEN_LAYERS_SIZE <<
                    " should be a positive integer" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_HIDDEN_LAYERS_SIZE <<
                " should be a positive integer" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_HIDDEN_LAYERS_SIZE <<
            " : " << m_neuralnetNumHiddenLayers << endl;
    }

	//hidden layer unit
	tempStringVar = "";
    errorCode = shapeRecognizerProperties->getConfigValue(NEURALNET_HIDDEN_LAYERS_UNITS,
                                                          tempStringVar);
	if(errorCode == SUCCESS)
    {
		stringVector tokens;

		LTKStringUtil::tokenizeString(tempStringVar,  HIDDEN_LAYER_UNIT_DELIMITER,  tokens);

		if(tokens.size() ==  m_neuralnetNumHiddenLayers)
		{
			m_layerOutputUnitVec.clear();

			m_layerOutputUnitVec.push_back(0); // input layer

			for(int i = 0; i < m_neuralnetNumHiddenLayers; ++i)
			{
				if ( LTKStringUtil::isInteger(tokens[i]) )
				{
					m_layerOutputUnitVec.push_back(atoi(tokens[i].c_str())); // for hidden layer

					if(m_layerOutputUnitVec[i+1]<=0)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
							"Error: " << ECONFIG_FILE_RANGE << NEURALNET_HIDDEN_LAYERS_UNITS <<
							" should be a positive integer" <<
							" NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

						delete shapeRecognizerProperties;

						LTKReturnError(ECONFIG_FILE_RANGE);
					}
				}
				else
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
						"Error: " << ECONFIG_FILE_RANGE << NEURALNET_HIDDEN_LAYERS_UNITS <<
						" should be a positive integer" <<
						" NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

					delete shapeRecognizerProperties;

					LTKReturnError(ECONFIG_FILE_RANGE);
				}
			}// end for

			m_layerOutputUnitVec.push_back(0); // output layer

			m_layerOutputUnitVec.push_back(0); // extra

			tokens.clear();
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
				"Error: " << ECONFIG_FILE_RANGE << NEURALNET_HIDDEN_LAYERS_UNITS <<
				" should be a positive integer (number of unit should be same with number of hidden layer)" <<
				" NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

			delete shapeRecognizerProperties;

			LTKReturnError(ECONFIG_FILE_RANGE);
		}
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_HIDDEN_LAYERS_UNITS <<
            " : " << m_neuralnetNumHiddenLayers << endl;

		m_neuralnetNumHiddenLayers=NEURALNET_DEF_HIDDEN_LAYERS_SIZE;
    }

	//initialised weight from previously train weight
	tempStringVar = "";
    shapeRecognizerProperties->getConfigValue(NEURALNET_WEIGHT_REESTIMATION, tempStringVar);

    if(LTKSTRCMP(tempStringVar.c_str(), "true") ==0)
    {
        m_isNeuralnetWeightReestimate = true;

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
            <<"Confidence computation method: " << NEURALNET_WEIGHT_REESTIMATION << endl;
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_WEIGHT_REESTIMATION << " : " <<
            m_isNeuralnetWeightReestimate << endl;
    }

	//number of itaretion
	tempStringVar = "";
    shapeRecognizerProperties->getConfigValue(NEURALNET_TRAINING_ITERATION, tempStringVar);

	if(errorCode == SUCCESS)
    {
        if ( LTKStringUtil::isInteger(tempStringVar) )
        {
			m_neuralnetMaximumIteration = atoi(tempStringVar.c_str());

            if(m_neuralnetMaximumIteration<=0)
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                    "Error: " << ECONFIG_FILE_RANGE << NEURALNET_TRAINING_ITERATION <<
                    " should be a positive integer" <<
                    " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

                delete shapeRecognizerProperties;

                LTKReturnError(ECONFIG_FILE_RANGE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
                "Error: " << ECONFIG_FILE_RANGE << NEURALNET_TRAINING_ITERATION <<
                " should be a positive integer" <<
                " NeuralNetShapeRecognizer::readClassifierConfig()"<<endl;

            delete shapeRecognizerProperties;

            LTKReturnError(ECONFIG_FILE_RANGE);
        }
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_TRAINING_ITERATION <<
            " : " << m_neuralnetMaximumIteration << endl;
    }

	//prepare traning sequence
	tempStringVar = "";
    shapeRecognizerProperties->getConfigValue(NEURALNET_TRAINING_SEQUENCE, tempStringVar);

    if(LTKSTRCMP(tempStringVar.c_str(), "false") ==0)
    {
        m_isCreateTrainingSequence = false;

        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
            <<"Confidence computation method: " << NEURALNET_TRAINING_SEQUENCE << endl;
    }
    else
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
            "Using default value for " << NEURALNET_TRAINING_SEQUENCE << " : " <<
            m_isCreateTrainingSequence << endl;
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
        "NeuralNetShapeRecognizer::readClassifierConfig()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: mapPreprocFunctions
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::mapPreprocFunctions()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::mapPreprocFunctions()" << endl;

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
            " NeuralNetShapeRecognizer::mapPreprocFunctions()"<<endl;

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
                        " NeuralNetShapeRecognizer::mapPreprocFunctions()"<<endl;
                    LTKReturnError(EINVALID_PREPROC_SEQUENCE);
                }
            }
            else
            {
                LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                    "Wrong preprocessor sequence entry in cfg file  : " << module<<
                    " NeuralNetShapeRecognizer::mapPreprocFunctions()"<<endl;
                LTKReturnError(EINVALID_PREPROC_SEQUENCE);
            }
        }
        else
        {
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_PREPROC_SEQUENCE << " " <<
                "Wrong preprocessor sequence entry in cfg file  : "<<module<<
                " NeuralNetShapeRecognizer::mapPreprocFunctions()"<<endl;
            LTKReturnError(EINVALID_PREPROC_SEQUENCE);
        }
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::mapPreprocFunctions()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: initializePreprocessor
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::initializePreprocessor(const LTKControlInfo& controlInfo,
        LTKPreprocessorInterface** preprocInstance)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::initializePreprocessor()" << endl;

    FN_PTR_CREATELTKLIPIPREPROCESSOR createLTKLipiPreProcessor = NULL;
    int errorCode;

    // Load the DLL with path=preprocDLLPath
    void* functionHandle = NULL;

    int returnVal = m_OSUtilPtr->loadSharedLib(controlInfo.lipiLib, PREPROC, &m_libHandler);


	if(returnVal != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  ELOAD_PREPROC_DLL << " " <<
            getErrorMessage(ELOAD_PREPROC_DLL) <<
            " NeuralNetShapeRecognizer::initializePreprocessor()" << endl;
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
            " NeuralNetShapeRecognizer::initializePreprocessor()" << endl;

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
            " NeuralNetShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(EDLL_FUNC_ADDRESS_CREATE);
	}

    m_deleteLTKLipiPreProcessor = (FN_PTR_DELETELTKLIPIPREPROCESSOR)functionHandle;

    // Create preprocessor instance
    errorCode = createLTKLipiPreProcessor(controlInfo, preprocInstance);

    if(errorCode!=SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<  errorCode << " " <<
            " NeuralNetShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(errorCode);
    }

    // Could not create a LTKLipiPreProcessor
    if(*preprocInstance == NULL)
    {
        // Unload the DLL
        unloadPreprocessorDLL();

        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ECREATE_PREPROC << " " <<
            getErrorMessage(ECREATE_PREPROC) <<
            " NeuralNetShapeRecognizer::initializePreprocessor()" << endl;
        LTKReturnError(ECREATE_PREPROC);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::initializePreprocessor()" << endl;

    return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: unloadPreprocessorDLL
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::unloadPreprocessorDLL()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::unloadPreprocessorDLL()" << endl;


    //Check the preprocessor DLL was loaded already
    if(m_libHandler != NULL)
    {
        //Unload the DLL
		m_OSUtilPtr->unloadSharedLib(m_libHandler);
        m_libHandler = NULL;
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::unloadPreprocessorDLL()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: initializeFeatureExtractorInstance
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::initializeFeatureExtractorInstance(const LTKControlInfo& controlInfo)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"NeuralNetShapeRecognizer::initializeFeatureExtractorInstance()" << endl;


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
			" NeuralNetShapeRecognizer::initializeFeatureExtractorInstance()" << endl;
		LTKReturnError(errorCode);
	}
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
		"NeuralNetShapeRecognizer::initializeFeatureExtractorInstance()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: deleteFeatureExtractorInstance
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::deleteFeatureExtractorInstance()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

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
                " NeuralNetShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

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
        "NeuralNetShapeRecognizer::deleteFeatureExtractorInstance()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: deletePreprocessor
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::deletePreprocessor()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::deletePreprocessor()" << endl;

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
            " NeuralNetShapeRecognizer::deletePreprocessor()" << endl;
        LTKReturnError(returnStatus);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::deletePreprocessor()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: preprocess
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::preprocess(const LTKTraceGroup& inTraceGroup,
        LTKTraceGroup& outPreprocessedTraceGroup)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::preprocess()" << endl;

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
                        " NeuralNetShapeRecognizer::preprocess()" << endl;
                    LTKReturnError(errorCode);
                }

                local_inTraceGroup = outPreprocessedTraceGroup;
            }
            indx++;
        }
    }
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting " <<
		"NeuralNetShapeRecognizer::preprocess()" << endl;
    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: getTraceGroups
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::getTraceGroups(int shapeID, int numberOfTraceGroups,
        vector<LTKTraceGroup> &outTraceGroups)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Entering NeuralNetShapeRecognizer::getTraceGroups"
        <<endl;

	//base class function, to be implemented


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NeuralNetShapeRecognizer::getTraceGroups"
        <<endl;
    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: appendNeuralNetDetailsToMDTFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::appendNeuralNetDetailsToMDTFile(const double2DVector& resultVector,
														const bool isWeight,
														ofstream & mdtFileHandle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::appendNeuralNetDetailsToMDTFile()" << endl;

	int index =0;

	double2DVector::const_iterator resultRowIter = resultVector.begin();
	double2DVector::const_iterator resultRowIterEnd = resultVector.end();

	if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINVALID_FILE_HANDLE << " " <<
            "Invalid file handle for MDT file"<<
            " NNShapeRecognizer::appendNeuralNetDetailsToMDTFile()" << endl;
        LTKReturnError(EINVALID_FILE_HANDLE);
    }

	if( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_BINARY )
    {
		int numOfLayer = resultVector.size();

		mdtFileHandle.write((char *)(&numOfLayer), sizeof(int));
	}
	else
	{
		if(isWeight)
			mdtFileHandle << "<Weight>" << NEW_LINE_DELIMITER;
		else
			mdtFileHandle << "<De_W Previous>" << NEW_LINE_DELIMITER;
	}

	for(; resultRowIter != resultRowIterEnd; resultRowIter++)
	{
		doubleVector::const_iterator colItr = (*resultRowIter).begin();
		doubleVector::const_iterator colItrEnd = (*resultRowIter).end();

		int numOfNode = (*resultRowIter).size();

		if(numOfNode != 0 && m_MDTFileOpenMode == NN_MDT_OPEN_MODE_BINARY)
		{
				mdtFileHandle.write((char *)(&numOfNode), sizeof(int));
		}

		for(; colItr != colItrEnd; colItr++)
		{
			if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_BINARY )
			{
				float floatValue = (*colItr);
                mdtFileHandle.write((char *)(&floatValue), sizeof(float));
			}
			else
			{
				mdtFileHandle <<scientific <<fixed << (*colItr);

				if(index > 99)
				{
					mdtFileHandle << NEW_LINE_DELIMITER;

					index =0;
				}
				else
				{
					mdtFileHandle << CLASSID_FEATURES_DELIMITER;
					++index;
				}
			}
		}

		if(m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII)
		{
			mdtFileHandle <<NEW_LINE_DELIMITER;
		}

	}

	if( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
		if(isWeight)
			mdtFileHandle << "<End Weight>" << NEW_LINE_DELIMITER;
		else
			mdtFileHandle << "<End De_W Previous>" << NEW_LINE_DELIMITER;
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NeuralNetShapeRecognizer::appendNeuralNetDetailsToMDTFile()"
        <<endl;
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			:
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::loadModelData()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::loadModelData()" << endl;

	int numofShapes = 0;
     
	int errorCode = -1;

    // variable for shape Id
    int classId = -1;

	int layerIndex = 0;

	int nodeValueIndex;

    //Algorithm version
    string algoVersionReadFromADT = "";

    stringStringMap headerSequence;

    LTKCheckSumGenerate cheSumGen;

	if(errorCode = cheSumGen.readMDTHeader(m_neuralnetMDTFilePath,headerSequence))
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(errorCode);
    }

	// printing the headerseqn
	stringStringMap::const_iterator iter = headerSequence.begin();
	stringStringMap::const_iterator endIter = headerSequence.end();

	for(; iter != endIter; iter++)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Info: header seqn"<<
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
            " NeuralNetShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

    string feVersion = headerSequence[FE_VER];

    // comparing the mdt open mode read from cfg file with value in the mdt header
    string mdtOpenMode = headerSequence[MDT_FOPEN_MODE];

    if (LTKSTRCMP(m_MDTFileOpenMode.c_str(), mdtOpenMode.c_str()) != 0)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of NEURAL_NETMDTFileOpenMode parameter in config file ("<<
            m_MDTFileOpenMode <<") does not match with the value in MDT file ("<<
            mdtOpenMode<<")"<<
            " NeuralNetShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }


    // validating preproc parameters
    int iErrorCode = validatePreprocParameters(headerSequence);
    if (iErrorCode != SUCCESS )
    {
    	LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Values of NEURAL_NETMDTFileOpenMode parameter in config file does not match with "
            <<"the values in MDT file " << "NeuralNetShapeRecognizer::loadModelData()" << endl;

        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

	iErrorCode = validateNeuralnetArchitectureParameters(headerSequence);

	if (iErrorCode != SUCCESS )
    {
    	LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Values of NEURAL_NETMDTFileOpenMode parameter in config file does not match with "
            <<"the values in MDT file " << "NeuralNetShapeRecognizer::loadModelData()" << endl;

        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

    // Version comparison START
    algoVersionReadFromADT = headerSequence[RECVERSION].c_str();

    LTKVersionCompatibilityCheck verTempObj;
    string supportedMinVersion(SUPPORTED_MIN_VERSION);
    string currentVersionStr(m_currentVersion);

    bool compatibilityResults = verTempObj.checkCompatibility(supportedMinVersion,
            currentVersionStr,
            algoVersionReadFromADT);

    if(compatibilityResults == false)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EINCOMPATIBLE_VERSION << " " <<
            " Incompatible version"<<
            " NeuralNetShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(EINCOMPATIBLE_VERSION);
    }
    // Version comparison END

	//Input Stream for Model Data file
    ifstream mdtFileHandle;

    if (m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
    {
        mdtFileHandle.open(m_neuralnetMDTFilePath.c_str(), ios::in);
    }
    else
    {
        mdtFileHandle.open(m_neuralnetMDTFilePath.c_str(), ios::in | ios::binary);
    }

    //If error while opening, return an error
    if(!mdtFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
            " Unable to open model data file : " <<m_neuralnetMDTFilePath<<
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
    else
    {
		m_numShapes = numofShapes;
    }

	//set output layer unit
	if(m_layerOutputUnitVec[(m_layerOutputUnitVec.size() - 2)] != m_numShapes)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
            " Value of output unit parameter in config file ("<<m_numShapes<<
            ") does not match with the value in MDT file ("<<numofShapes<<")"<<
            " NNShapeRecognizer::loadModelData()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
	}

	// validating the header values

    string strValue = "";

	//testing for initialisation
	if(m_connectionWeightVec.size() == 0 || m_previousDelW.size() == 0)
	{
		for(int index = 0; index < (m_neuralnetNumHiddenLayers + 2); ++index)
		{
			doubleVector tempDoubleVec(((m_layerOutputUnitVec[index] + 1) * m_layerOutputUnitVec[index+1]));

			m_connectionWeightVec.push_back(tempDoubleVec);

			m_previousDelW.push_back(tempDoubleVec);

			tempDoubleVec.clear();
		}
	}

	int floatSize = atoi(headerSequence[SIZEOFFLOAT].c_str());

	int intSize = atoi(headerSequence[SIZEOFINT].c_str());

	if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
	{
		while(getline(mdtFileHandle, strValue, NEW_LINE_DELIMITER ))
		{
			if(LTKSTRCMP(strValue.c_str(),"<Weight>") == 0)
			{
				for (layerIndex = 0; layerIndex < m_neuralnetNumHiddenLayers + 1; layerIndex++)
				{
					for (nodeValueIndex =0; nodeValueIndex < (m_layerOutputUnitVec[layerIndex] + 1) * m_layerOutputUnitVec[layerIndex + 1]; nodeValueIndex++)
					{
						mdtFileHandle >> strValue;

                        float floatValue = LTKStringUtil::convertStringToFloat(strValue);

						m_connectionWeightVec[layerIndex][nodeValueIndex] = (double)floatValue;
					}
				}

			}
			else if(LTKSTRCMP(strValue.c_str(),"<De_W Previous>") == 0)
			{
				for (layerIndex = 0; layerIndex < m_neuralnetNumHiddenLayers + 1; layerIndex++)
				{
					for (nodeValueIndex = 0; nodeValueIndex < (m_layerOutputUnitVec[layerIndex] + 1) * m_layerOutputUnitVec[layerIndex + 1]; nodeValueIndex++)
					{
						mdtFileHandle >> strValue;

                        double floatValue = LTKStringUtil::convertStringToFloat(strValue);

						m_previousDelW[layerIndex][nodeValueIndex] = floatValue;
					}
				}
			}
		} // end while outer
	}
	else // for binary mode
	{
		int numOfLayer;

		while(!mdtFileHandle.eof())
		{
			mdtFileHandle.read((char*) &numOfLayer, intSize);

			if ( mdtFileHandle.fail() )
			{
				break;
			}

			if( (numOfLayer - 1) != (m_neuralnetNumHiddenLayers + 1))
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
					" Value of hidden layer parameter in config file ("<<m_neuralnetNumHiddenLayers<<
					") does not match with the value in MDT file ("<<(numOfLayer - 2)<<")"<<
					" NNShapeRecognizer::loadModelData()" << endl;

				LTKReturnError(ECONFIG_MDT_MISMATCH);
			}

			layerIndex = 0;

			for ( ; layerIndex < (numOfLayer - 1) ; layerIndex++)
			{
				nodeValueIndex = 0;

				int numberOfNode;

				mdtFileHandle.read((char*) &numberOfNode, intSize);

				cout<<numberOfNode << "::"<<endl;

				if(numberOfNode !=((m_layerOutputUnitVec[layerIndex] + 1) * m_layerOutputUnitVec[layerIndex + 1]))
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
						" Value of hidden node parameter in config file" <<
						" does not match with the value in MDT file" <<
						" NNShapeRecognizer::loadModelData()" << endl;

					LTKReturnError(ECONFIG_MDT_MISMATCH);
				}

				for(; nodeValueIndex < numberOfNode ; nodeValueIndex++)
				{
					float nodeValue = 0.0f;

					mdtFileHandle.read((char*) &nodeValue, floatSize);

					m_connectionWeightVec[layerIndex][nodeValueIndex] = nodeValue;

					if ( mdtFileHandle.fail() )
					{
						break;
					}
				}

			} //weight

			numOfLayer = 0;

			mdtFileHandle.read((char*) &numOfLayer, intSize);

			if ( mdtFileHandle.fail() )
			{
				break;
			}

			if((numOfLayer - 1) != (m_neuralnetNumHiddenLayers + 1))
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
					" Value of hidden layer parameter in config file ("<<m_neuralnetNumHiddenLayers<<
					") does not match with the value in MDT file ("<<(numOfLayer - 2)<<")"<<
					" NNShapeRecognizer::loadModelData()" << endl;

				LTKReturnError(ECONFIG_MDT_MISMATCH);
			}

			layerIndex = 0;

			for ( ; layerIndex < (numOfLayer - 1) ; layerIndex++)
			{
				int numberOfNode;
				mdtFileHandle.read((char*) &numberOfNode, intSize);

				if(numberOfNode !=((m_layerOutputUnitVec[layerIndex] + 1) * m_layerOutputUnitVec[layerIndex + 1]))
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ECONFIG_MDT_MISMATCH << " " <<
						" Value of hidden node parameter in config file" <<
						" does not match with the value in MDT file" <<
						" NNShapeRecognizer::loadModelData()" << endl;

					LTKReturnError(ECONFIG_MDT_MISMATCH);
				}

				nodeValueIndex = 0;
				for(; nodeValueIndex < numberOfNode ; nodeValueIndex++)
				{
					float nodeValue = 0.0f;

					mdtFileHandle.read((char*) &nodeValue, floatSize);

					m_previousDelW[layerIndex][nodeValueIndex] = nodeValue;

					if ( mdtFileHandle.fail() )
					{
						break;
					}
				}

			}
		}
	}

	mdtFileHandle.close();


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::loadModelData()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: validateNeuralnetArchitectureParameters
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters(stringStringMap& headerSequence)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;

	string headerValue = "";
	int headerValueInt   = 0;
    float headerValueFloat = 0.0f;


	if(LTKSTRCMP((headerSequence[HIDDEN_LAYER]).c_str(), "NA") != 0)
    {
		headerValueInt = atoi(headerSequence[HIDDEN_LAYER].c_str());

		if(headerValueInt !=  m_neuralnetNumHiddenLayers)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of hidden layer in config file ("<<
				m_neuralnetNumHiddenLayers<<") does not match with the value in MDT file ("<<
				headerValueInt <<")"<<
				" NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}

	if(LTKSTRCMP((headerSequence[LEARNING_RATE].c_str()),"NA") != 0)
	{
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[LEARNING_RATE].c_str());

		if(headerValueFloat != m_neuralnetLearningRate)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of larning rate in config file ("<<
				m_neuralnetLearningRate<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;
		}

	}

	if(LTKSTRCMP((headerSequence[MOMEMTUM_RATE].c_str()),"NA") != 0)
	{
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[MOMEMTUM_RATE].c_str());

		if(headerValueFloat != m_neuralnetMomemtumRate)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of momentum rate in config file ("<<
				m_neuralnetMomemtumRate<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;
		}

	}

	if(LTKSTRCMP((headerSequence[NORMALISED_FACTOR].c_str()),"NA") != 0)
	{
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[NORMALISED_FACTOR].c_str());

		if(headerValueFloat != m_neuralnetNormalizationFactor)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of normalised factor in config file ("<<
				m_neuralnetNormalizationFactor<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}

	}

	stringVector unitTokens;
	string mdtHLayerUnit = headerSequence[HIDDENLAYERSUNIT];

	LTKStringUtil::tokenizeString(mdtHLayerUnit,
						HIDDEN_LAYER_UNIT_DELIMITER,  unitTokens);

	if(unitTokens.size() != m_layerOutputUnitVec.size())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
            "Value of layer stracture does not match" <<
            " does not match with the value in MDT file"<<
            " NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;

        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }
	else
	{
		int s = m_layerOutputUnitVec.size();

		for(int i = 0 ; i < (s - 1) ; ++i)
		{
			if(i == 0)
			{
				//set input layer
				m_layerOutputUnitVec[i] = atoi(unitTokens[i].c_str());
			}
			else if( i <= m_neuralnetNumHiddenLayers)
			{
				if(m_layerOutputUnitVec[i] != atoi(unitTokens[i].c_str()))
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
						"Value of hidden node does not match" <<
						" does not match with the value in MDT file"<<
						" NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;

					LTKReturnError(ECONFIG_MDT_MISMATCH);
				}
			}
			else
			{
				m_layerOutputUnitVec[i] = atoi(unitTokens[i].c_str());
			}
		}
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::validateNeuralnetArchitectureParameters()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: validatePreprocParameters
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::validatePreprocParameters(stringStringMap& headerSequence)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
            " NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
				" NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
            " NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
            " NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
        LTKReturnError(ECONFIG_MDT_MISMATCH);
    }

	// NormPreserveAspectRatioThreshold
	tempFloatValue = m_ptrPreproc->getAspectRatioThreshold();
	if(LTKSTRCMP((headerSequence[ASP_RATIO_THRES]).c_str(), "NA") != 0)
    {
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[ASP_RATIO_THRES].c_str());

		if(headerValueFloat != tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcPreserveAspectRatioThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}

	// NormLineWidthThreshold
	if(LTKSTRCMP((headerSequence[DOT_SIZE_THRES]).c_str(), "NA") != 0)
    {
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[DOT_SIZE_THRES].c_str());
		tempFloatValue = m_ptrPreproc->getSizeThreshold();

		if(headerValueFloat !=  tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcNormLineWidthThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}

	// NormDotSizeThreshold
	if(LTKSTRCMP((headerSequence[DOT_THRES]).c_str(), "NA") != 0)
    {
		headerValueFloat = LTKStringUtil::convertStringToFloat(headerSequence[DOT_THRES].c_str());
		tempFloatValue = m_ptrPreproc->getDotThreshold();

		if(headerValueFloat != tempFloatValue)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<  ECONFIG_MDT_MISMATCH << " " <<
				"Value of preProcNormDotSizeThreshold in config file ("<<
				tempFloatValue<<") does not match with the value in MDT file ("<<
				headerValueFloat <<")"<<
				" NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
            " NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
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
				" NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
			LTKReturnError(ECONFIG_MDT_MISMATCH);
		}
	}
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::validatePreprocParameters()" << endl;
	return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: setDeviceContext
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::setDeviceContext(const LTKCaptureDevice& deviceInfo)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::setDeviceContext()" << endl;

    m_captureDevice = deviceInfo;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::setDeviceContext()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: unloadModelData
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::unloadModelData()
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::unloadModelData()" << endl;

    int returnStatus = SUCCESS;

    m_connectionWeightVec.clear();

	m_previousDelW.clear();

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::unloadModelData()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: writeNeuralNetDetailsToMDTFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::writeNeuralNetDetailsToMDTFile()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

	int returnStatus = SUCCESS;

	int index = 0;

	int maxIndex;

	double maxVal;


	//write the Neural net architecture i.e network and weight
	ofstream mdtFileHandle;

	double2DVector vecNetworkWeight;

	double2DVector vecNetworkDelW;

	double2DVector::iterator networkWeightIter;

	int connectionWeightSetSize = m_connectionWeightVec.size();

	if(connectionWeightSetSize == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EEMPTY_VECTOR << " " <<
			"Empty connection weights" <<
			" NNShapeRecognizer::writeNeuralNetDetailsToMDTFile()"<<endl;

		LTKReturnError(EEMPTY_VECTOR);
	}

	double2DVector::iterator networkDelWItr;

	int priviousDelWSetSize = m_previousDelW.size();

	if(priviousDelWSetSize == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EEMPTY_VECTOR << " " <<
			"Empty privious Del W" <<
			" NNShapeRecognizer::writeNeuralNetDetailsToMDTFile()"<<endl;

		LTKReturnError(EEMPTY_VECTOR);
	}


	//Open the Model data file for writing mode
	if ( m_MDTFileOpenMode == NN_MDT_OPEN_MODE_ASCII )
	{
		mdtFileHandle.open(m_neuralnetMDTFilePath.c_str(), ios::out);
	}
	else
	{
		mdtFileHandle.open(m_neuralnetMDTFilePath.c_str(),ios::out|ios::binary);
	}

	//Throw an error if unable to open the Model data file
	if(!mdtFileHandle)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< EMODEL_DATA_FILE_OPEN << " " <<
			getErrorMessage(EMODEL_DATA_FILE_OPEN)<<
			" NeuralNetShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

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

	networkWeightIter = m_connectionWeightVec.begin();

	int i=0;
	for (i=0;i<connectionWeightSetSize;i++)
	{
		vecNetworkWeight.push_back((*networkWeightIter));

		networkWeightIter++;
	}

	returnStatus = appendNeuralNetDetailsToMDTFile(vecNetworkWeight,true,mdtFileHandle);

	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< returnStatus << " " <<
           " NNShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

        LTKReturnError(returnStatus);
    }

	vecNetworkWeight.clear();

	networkDelWItr = m_previousDelW.begin();

	for (i=0;i<priviousDelWSetSize;i++)
	{
		vecNetworkDelW.push_back((*networkDelWItr));

		networkDelWItr++;
	}

	returnStatus = appendNeuralNetDetailsToMDTFile(vecNetworkDelW,false,mdtFileHandle);

	if(returnStatus != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< returnStatus << " " <<
           " NNShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

        LTKReturnError(returnStatus);
    }

	vecNetworkDelW.clear();

	mdtFileHandle.close();

	//Updating the Header Information
    updateHeaderWithAlgoInfo();

    //Adding header information	and checksum generation
    string strModelDataHeaderInfoFile = "";
    LTKCheckSumGenerate cheSumGen;

	returnStatus = cheSumGen.addHeaderInfo(
		    strModelDataHeaderInfoFile,
			m_neuralnetMDTFilePath,
			m_headerInfo
			);

	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  returnStatus << " " <<
           " NNShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

		LTKReturnError(returnStatus);
	}


    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::writeNeuralNetDetailsToMDTFile()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: train
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::train(const string& trainingInputFilePath,
        const string& mdtHeaderFilePath,
        const string &comment,const string &dataset,
        const string &trainFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::train()" << endl;

    int returnStatus = SUCCESS;

	if(comment.empty() != true)
    {
        m_headerInfo[COMMENT] = comment;
    }

    if(dataset.empty() != true)
    {
        m_headerInfo[DATASET] = dataset;
    }

	returnStatus = trainNetwork(trainingInputFilePath,
                mdtHeaderFilePath,
                trainFileType);

    if(returnStatus != SUCCESS)
    {
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
                getErrorMessage(returnStatus) <<
                " NNShapeRecognizer::train()" << endl;

		LTKReturnError(returnStatus);
    }

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::train()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: updateHeaderWithAlgoInfo
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
void NeuralNetShapeRecognizer::updateHeaderWithAlgoInfo()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;

	//Set the NumShapes to the m_headerInfo
	char strVal[80];
	//itoa function is not available in linux
	//itoa(m_numShapes,strVal,10);
	sprintf(strVal,"%d",m_numShapes);
	string strNumShapes(strVal);
	m_headerInfo[NUMSHAPES] = strNumShapes;

	ostringstream tempString;

	int size = m_layerOutputUnitVec.size();

	for(int i = 0; i < size; ++i)
	{
		tempString << m_layerOutputUnitVec[i] <<HIDDEN_LAYER_UNIT_DELIMITER;
	}

    string strhLayerUnit = tempString.str();

	m_headerInfo[HIDDENLAYERSUNIT] = strhLayerUnit;

    m_headerInfo[RECVERSION] = m_currentVersion;
    string algoName = NEURALNET;
    m_headerInfo[RECNAME] = algoName;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::updateHeaderWithAlgoInfo()" << endl;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: trainNetwork
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::trainNetwork(const string& trainingInputFilePath,
        const string &mdtHeaderFilePath,
        const string& inFileType)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::trainNetwork()" << endl;

	//Time at the beginning of Train Clustering
    m_OSUtilPtr->recordStartTime();

    int returnStatus = SUCCESS;

	///////////////////////////////////////
	// Calculating feature for all sample and all class
    if(LTKSTRCMP(inFileType.c_str(), INK_FILE) == 0)
    {
        //If the Input file is UNIPEN Ink file
        returnStatus = trainFromListFile(trainingInputFilePath);
        if(returnStatus != SUCCESS)
        {
            LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
                getErrorMessage(returnStatus) <<
                " NeuralNetShapeRecognizer::trainNetwork()" << endl;

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
                " NeuralNetShapeRecognizer::trainNetwork()" << endl;

            LTKReturnError(returnStatus);
        }

		PreprocParametersForFeatureFile(m_headerInfo);
    }

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Feature store successfully" <<endl;

	if(m_isCreateTrainingSequence)
	{
		//rearrabging the feature for traning of neural net
		returnStatus = prepareNeuralNetTrainingSequence();

		if(returnStatus != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
				getErrorMessage(returnStatus) <<
				" NeuralNetShapeRecognizer::trainNetwork()" << endl;

			LTKReturnError(returnStatus);
		}

		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
			"Success fully complete neural net traning sequence" <<endl;
	}

	//train the network
	returnStatus = prepareNetworkArchitecture();

	if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
            getErrorMessage(returnStatus) <<
            " NeuralNetShapeRecognizer::trainNetwork()" << endl;

        LTKReturnError(returnStatus);
    }

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Successfully train Network" <<endl;

	//write the weight
	returnStatus = writeNeuralNetDetailsToMDTFile();

	if( returnStatus != SUCCESS )
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<returnStatus << " " <<
			" NeuralNetShapeRecognizer::trainNetwork()" << endl;

		LTKReturnError(returnStatus);
	}

    if(returnStatus != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
            getErrorMessage(returnStatus) <<
            " NeuralNetShapeRecognizer::trainClustering()" << endl;

        LTKReturnError(returnStatus);
    }

	////////////////////////////////////////
    //Time at the end of Train Clustering
    m_OSUtilPtr->recordEndTime();

    string timeTaken = "";
    m_OSUtilPtr->diffTime(timeTaken);

    cout << "Time Taken  = " << timeTaken << endl;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::trainNetwork()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: trainFromListFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::trainFromListFile(const string& listFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::trainFromListFile()" << endl;

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

	vector<LTKShapeFeaturePtr> shapeFeature;

	//list file handle
    ifstream listFileHandle;

    //Opening the train list file for reading mode
    listFileHandle.open(listFilePath.c_str(), ios::in);

    //Throw an error if unable to open the training list file
    if(!listFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ETRAINLIST_FILE_OPEN << " " <<
            getErrorMessage(ETRAINLIST_FILE_OPEN)<<
            " NeuralNetShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(ETRAINLIST_FILE_OPEN);
    }

    int errorCode = SUCCESS;
	unsigned short numShapes = m_numShapes;
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
						" NeuralNetShapeRecognizer::trainFromListFile()" << endl;

					listFileHandle.close();

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
						"The NeuralNetShapeRecognizer requires training file class Ids to be positive integers and listed in the increasing order"<<
						" NeuralNetShapeRecognizer::trainFromListFile()" << endl;
					errorCode = EINVALID_SHAPEID;
					break;
				}
				else if(shapeId < prevClassId)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
						"Shape IDs in the train list file should be in the increasing order. Please use scripts/validateListFile.pl to generate list files." <<
						" NeuralNetShapeRecognizer::trainFromListFile()" << endl;
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
					" NeuralNetShapeRecognizer::trainFromListFile()" << endl;

				continue;
			}

			shapeSampleFeatures.setFeatureVector(shapeFeature);
			shapeSampleFeatures.setClassID(shapeId);

			++sampleCount;

			shapeFeature.clear();

			//All the samples are pushed to m_trainSet used for neural net training
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
			if( !m_projectTypeDynamic && shapeCount > numShapes )
			{
				errorCode = EINVALID_NUM_OF_SHAPES;
				break;
			}

			if( shapeCount > 0 && sampleCount > 0 )
			{
				//number of sample present in the traning shape
				m_sampleCountVec.push_back(sampleCount);

				//new shape is found
				//m_numShapes variable value is retreived from config file
				m_numShapes += 1;

				//Resetting sampleCount for the next class
				sampleCount = 0;

				//Set the flag so that the already read line of next class in the list file is not lost
				lastshapeIdFlag = true;

				prevClassId = shapeId;

			}
		}
	}//End of while

    //Closing the Train List file
    listFileHandle.close();

    if(!m_projectTypeDynamic && shapeCount != numShapes)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_SHAPES << " " <<
            getErrorMessage(EINVALID_NUM_OF_SHAPES)<<
            " NeuralNetShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(EINVALID_NUM_OF_SHAPES);
    }
	/*else
	{
		m_numShapes = shapeCount;
	}*/

    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::trainFromListFile()" << endl;
        LTKReturnError(errorCode);

    }

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::trainFromListFile()" << endl;

    return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: prepareNeuralNetTrainingSequence
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::prepareNeuralNetTrainingSequence()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::prepareNeuralNetTraningSequence()" << endl;

	int sizeOfTraningSet = m_trainSet.size();

	if(sizeOfTraningSet == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty traning set) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::prepareNeuralNetTraningSequence()" << endl;
        LTKReturnError(EEMPTY_VECTOR);
	}

	vector<LTKShapeSample> shapeSamplesVec;

	LTKShapeSample shapeSampleFeatures;

	intVector countVector;

	intVector initIndexVec;

	int qumulativeIndex = 0;

	int index = 0;

	// store the shape ID which contain the maximum traning sample
	int patternId = -1;

	int max = -1;

	bool isPrepareTraningSet = false;

	try{

		//if traning set contain unequal number of sample,
		// then it make the equal number of sample for the traning set
		for(index = 0; index < m_sampleCountVec.size(); ++index)
		{
			if(index == 0)
			{
				qumulativeIndex = (m_sampleCountVec[index] - 1);

				max = m_sampleCountVec[index];

				patternId = index;

				initIndexVec.push_back(index);
			}
			else
			{
				qumulativeIndex = (countVector[index - 1]  + m_sampleCountVec[index]);

				initIndexVec.push_back((countVector[index - 1] + 1));

				if(m_sampleCountVec[index] > max)
				{
					max = m_sampleCountVec[index];

					patternId = index;
				}
			}

			countVector.push_back(qumulativeIndex);
		}

		index = 0;

		// copy the whole traning set
		shapeSamplesVec = m_trainSet;

		m_trainSet.clear();

		while(!isPrepareTraningSet)
		{
			// currenly pointing the sample of a traning shape
			int currentIndex = initIndexVec[index];

			if(currentIndex <= countVector[index])
			{
				//point the next sample
				initIndexVec[index] += 1;

				int shapeId = shapeSamplesVec[currentIndex].getClassID();

				m_targetOutputVec.push_back(vector<double>());

				m_targetOutputVec[m_targetOutputVec.size() - 1] = doubleVector(m_numShapes);

				m_targetOutputVec[m_targetOutputVec.size() - 1][shapeId] = 1;

				vector<LTKShapeFeaturePtr> shapeFeature = shapeSamplesVec[currentIndex].getFeatureVector();

				//Normalised feature vector to prepare neural net traning feature lise between (-1 to 1)
				vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = shapeFeature.begin();
				vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = shapeFeature.end();

				for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
				{
					floatVector floatFeatureVector;

					(*shapeFeatureIter)->toFloatVector(floatFeatureVector);

					int vectorSize = floatFeatureVector.size();

					int i=0;
					for (i=0; i< vectorSize; i++)
					{
						float floatValue = floatFeatureVector[i];

						floatFeatureVector[i] = floatValue;
					}

					if(floatFeatureVector[(i - 1)] > 0.0)
					{
						//for penup and pendown
						floatFeatureVector[(i - 1)] = 1;
					}

					//Initialised the neuralnet traning feature
					(*shapeFeatureIter)->initialize(floatFeatureVector);

					floatFeatureVector.clear();

				}

				shapeSampleFeatures.setFeatureVector(shapeFeature);
				shapeSampleFeatures.setClassID(shapeId);

				m_trainSet.push_back(shapeSampleFeatures);

				//Initialised the output vector (output node) for nuralne net traning
				doubleVector tempVector(m_numShapes);
				m_outputLayerContentVec.push_back(tempVector);
				tempVector.clear();

				++index; // for next shape
			}
			else
			{
				//for putting duplicate copy for same traning
				if(index != patternId)
				{
					if(index == 0)
						initIndexVec[index] = 0;
					else
						initIndexVec[index] = (countVector[index - 1] + 1);
				}
			}

			// for back to the first class
			if(index == m_numShapes)
			{
				index = 0;

				if(initIndexVec[patternId] > countVector[patternId])
				{
					isPrepareTraningSet = true;
				}
			}
		}

	}//end try
	catch(LTKException e)
	{
		cout<<"Could not produce traning sequence." <<"\nPlease check the traning sequence."<<endl;

		shapeSamplesVec.clear();

		countVector.clear();

		initIndexVec.clear();

		m_trainSet.clear();

		throw e;
	}

	shapeSamplesVec.clear();

	countVector.clear();

	initIndexVec.clear();

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::prepareNeuralNetTraningSequence()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: trainFromFeatureFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::trainFromFeatureFile(const string& featureFilePath)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;

	//TIME NEEDED

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

    //Input Stream for feature file
    ifstream featureFileHandle;

    LTKShapeSample shapeSampleFeatures;

    //Opening the feature file for reading mode
    featureFileHandle.open(featureFilePath.c_str(), ios::in);

    //Throw an error if unable to open the training list file
    if(!featureFileHandle)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EFEATURE_FILE_OPEN << " " <<
            getErrorMessage(EFEATURE_FILE_OPEN) <<
            " NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(EFEATURE_FILE_OPEN);

    }

	//Reading feature file header
    getline(featureFileHandle, line, NEW_LINE_DELIMITER);
    stringStringMap headerSequence;
    int errorCode = SUCCESS;
    errorCode = m_shapeRecUtil.convertHeaderToStringStringMap(line, headerSequence);
    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(errorCode);
    }

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
						"The NeuralNet Shape recognizer requires training file class Ids to be positive integers and listed in the increasing order" <<
						" NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
					errorCode = EINVALID_SHAPEID;
					break;
				}
				else if(shapeId < prevClassId)
				{
					LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<<
						"Shape IDs in the train list file should be in the increasing order. Please use scripts/validateListFile.pl to generate list files." <<
						" NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
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
            //---------------------shapeSamplesVec.push_back(shapeSampleFeatures);
            ++sampleCount;
            //All the samples are pushed to trainSet for neuralnet traning
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
                //Clearing the shapeSampleVector and clusteredShapeSampleVector
                //---------------clusteredShapeSampleVec.clear();
                //----------------shapeSamplesVec.clear();

				//number of sample present in the traning shape
				m_sampleCountVec.push_back(sampleCount);

                //Resetting sampleCount for the next class
                sampleCount = 0;

                //Set the flag so that the already read line of next class in the list file is not lost
                lastshapeIdFlag = true;

                prevClassId = shapeId;
            }
        }
    }

    featureFileHandle.close();

    if(!m_projectTypeDynamic && shapeCount != m_numShapes)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_SHAPES << " " <<
            getErrorMessage(EINVALID_NUM_OF_SHAPES) <<
            " NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(EINVALID_NUM_OF_SHAPES);
    }
	{
		m_numShapes = shapeCount;
	}

    if(errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;
        LTKReturnError(errorCode);
    }

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::trainFromFeatureFile()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: getShapeSampleFromString
 * DESCRIPTION	: This method get the Shape Sample Feature from a given line
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::getShapeSampleFromString(const string& inString, LTKShapeSample& outShapeSample)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::getShapeSampleFromString()" << endl;


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
		    " NeuralNetShapeRecognizer::getShapeSampleFromString()" << endl;
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
		    " NeuralNetShapeRecognizer::getShapeSampleFromString()" << endl;
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
                " NeuralNetShapeRecognizer::getShapeSampleFromString()" << endl;
            LTKReturnError(EINVALID_INPUT_FORMAT);
        }
        shapeFeatureVector.push_back(shapeFeature);
    }

    //Set the feature vector and class id to the shape sample features
    outShapeSample.setFeatureVector(shapeFeatureVector);
    outShapeSample.setClassID(classId);

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::getShapeSampleFromString()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: PreprocParametersForFeatureFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::PreprocParametersForFeatureFile(stringStringMap& headerSequence)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::PreprocParametersForFeatureFile()" << endl;

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
        "NeuralNetShapeRecognizer::PreprocParametersForFeatureFile()" << endl;
	return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: getShapeFeatureFromInkFile
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::getShapeFeatureFromInkFile(const string& inkFilePath,
        vector<LTKShapeFeaturePtr>& shapeFeatureVec)
{

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::getShapeFeatureFromInkFile()" << endl;

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
            " NeuralNetShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(returnVal);
    }

    m_ptrPreproc->setCaptureDevice(captureDevice);
    m_ptrPreproc->setScreenContext(screenContext);

    preprocessedTraceGroup.emptyAllTraces();

    //Preprocessing to be done for the trace group that was read
	int errorCode = preprocess(inTraceGroup, preprocessedTraceGroup);
    if( errorCode != SUCCESS )
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }

	//Extract feature from the preprocessed trace group
    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
            shapeFeatureVec);

    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::getShapeFeatureFromInkFile()" << endl;
        LTKReturnError(errorCode);
    }

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::getShapeFeatureFromInkFile()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: computeConfidence
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::computeConfidence()
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::computeConfidence()" << endl;

	// Temporary vector to store the recognition results
    LTKShapeRecoResult outResult;

	int index = 0;

	//itaretor for the recognition result
	double2DVector::const_iterator outputLayerRowIter = m_outputLayerContentVec.begin();

	double2DVector::const_iterator outputLayerRowIterEnd = m_outputLayerContentVec.end();

	for(; outputLayerRowIter != outputLayerRowIterEnd; outputLayerRowIter++)
    {
		doubleVector::const_iterator colItr = (*outputLayerRowIter).begin();

		doubleVector::const_iterator colItrEnd = (*outputLayerRowIter).end();

		for(; colItr != colItrEnd; colItr++)
		{
			int classID = index++;

			double confidence = (*colItr);

			outResult.setShapeId(classID);

            outResult.setConfidence(confidence);

			m_vecRecoResult.push_back(outResult);
		}
	}

	//Sort the result vector in descending order of confidence
    sort(m_vecRecoResult.begin(), m_vecRecoResult.end(), sortResultByConfidence);

	 LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::computeConfidence()" << endl;

    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: sortResultByConfidence
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
bool NeuralNetShapeRecognizer::sortResultByConfidence(const LTKShapeRecoResult& x, const LTKShapeRecoResult& y)
{
    return (x.getConfidence()) > (y.getConfidence());
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: recognize
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::recognize(const vector<LTKShapeFeaturePtr>& shapeFeatureVector,
			const vector<int>& inSubSetOfClasses,
			float confThreshold,
			int  numChoices,
			vector<LTKShapeRecoResult>& outResultVector)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::recognize()" << endl;

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

	int index;

	//store the feature for network
	double2DVector outptr;

	//initialised the network parameter
	doubleVector tempVector(m_numShapes);

	m_outputLayerContentVec.push_back(tempVector);

	for( index = 0; index < (m_neuralnetNumHiddenLayers + 2); ++index)
	{
		doubleVector tempDoubleV((m_layerOutputUnitVec[index] + 1));

		outptr.push_back(tempDoubleV);

		tempDoubleV.clear();
	}

	for(index = 0; index < (m_neuralnetNumHiddenLayers + 1); ++index)
	{
		outptr[index][m_layerOutputUnitVec[index]] = 1.0;
	}

	//compute the forward algo for recognition performance
	int errorCode = feedForward(shapeFeatureVector,outptr,0);

	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::recognize()" << endl;

		LTKReturnError(errorCode);
	}

	errorCode = computeConfidence();

	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::recognize()" << endl;

		LTKReturnError(errorCode);
	}

	//copy the recognition result
	outResultVector = m_vecRecoResult;

	//clear the network parameter
	m_vecRecoResult.clear();
	m_outputLayerContentVec.clear();
	outptr.clear();

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::recognize()" << endl;

    return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: recognize
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::recognize(const LTKTraceGroup& traceGroup,
        const LTKScreenContext& screenContext,
        const vector<int>& inSubSetOfClasses,
        float confThreshold,
        int  numChoices,
        vector<LTKShapeRecoResult>& outResultVector)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::recognize()" << endl;

	if(traceGroup.containsAnyEmptyTrace())
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<EEMPTY_TRACE << " " <<
            " Input trace is empty"<<
            " NeuralNetShapeRecognizer::recognize()" << endl;

        LTKReturnError(EEMPTY_TRACE);
    }


    //Contains TraceGroup after Preprocessing is done
    LTKTraceGroup preprocessedTraceGroup;


    //Preprocess the traceGroup
	int errorCode = preprocess(traceGroup, preprocessedTraceGroup);
    if( errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            getErrorMessage(errorCode)<<
            " NeuralNetShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);
    }

    //Extract the shapeSample from preprocessedTraceGroup
    if(!m_ptrFeatureExtractor)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< ENULL_POINTER << " " <<
            " m_ptrFeatureExtractor is NULL"<<
            " NeuralNetShapeRecognizer::recognize()" << endl;
        LTKReturnError(ENULL_POINTER);
    }

    vector<LTKShapeFeaturePtr> shapeFeatureVec;
    errorCode = m_ptrFeatureExtractor->extractFeatures(preprocessedTraceGroup,
            shapeFeatureVec);

    if (errorCode != SUCCESS)
    {
        LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " NeuralNetShapeRecognizer::recognize()" << endl;

        LTKReturnError(errorCode);
    }

	// call recognize with featureVector

	if(recognize( shapeFeatureVec, inSubSetOfClasses, confThreshold,
			numChoices, outResultVector) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            getErrorMessage(errorCode)<<
            " NeuralNetShapeRecognizer::recognize()" << endl;
        LTKReturnError(errorCode);

	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::recognize()" << endl;

    return SUCCESS;

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: constractNeuralnetLayeredStructure
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()" << endl;

	int sizeOfTraningSet = m_trainSet.size();
	if(sizeOfTraningSet == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty traning set) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	int outputLayerIndex;

	//Store how many feature extracted from a sample (i.e the number of input node)
	int inputNodes = 0;

	vector<LTKShapeFeaturePtr> shapeFeature = m_trainSet[0].getFeatureVector();
	vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = shapeFeature.begin();
    vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = shapeFeature.end();

	//itaretor through all the feature vector
    for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
    {
		int dimention = (*shapeFeatureIter)->getFeatureDimension();

		inputNodes += dimention;
	}

	if(inputNodes <= 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_INPUT_NODE << "(Input node must be grater then zero) " <<
            getErrorMessage(EINVALID_NUM_OF_INPUT_NODE)<<
            " NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()" << endl;

        LTKReturnError(EINVALID_NUM_OF_INPUT_NODE);
	}

	 // input node
	m_layerOutputUnitVec[0] = inputNodes;

	outputLayerIndex = (m_layerOutputUnitVec.size() - 2);

	if(m_numShapes <= 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NUM_OF_OUTPUT_NODE << "(Output node must be grater than zero) " <<
            getErrorMessage(EINVALID_NUM_OF_OUTPUT_NODE)<<
            " NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()" << endl;

        LTKReturnError(EINVALID_NUM_OF_OUTPUT_NODE);
	}

	// output node
	m_layerOutputUnitVec[outputLayerIndex] = m_numShapes;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::constractNeuralnetLayeredStructure()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: prepareNetworkArchitecture
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::prepareNetworkArchitecture()
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::prepareNetworkArchitecture" << endl;

	int index;

	int returnStatus = SUCCESS;

	//Temporary vector for network parameter
	double2DVector outptr,errptr,target;

	//Store the error value of the network
	doubleVector ep;

	//Configur the node and layer of the network
	returnStatus = constractNeuralnetLayeredStructure();

	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  returnStatus << " " <<
           " NNShapeRecognizer::prepareNetworkArchitecture()" << endl;

		LTKReturnError(returnStatus);
	}

	returnStatus = initialiseNetwork(outptr,errptr);

	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  returnStatus << " " <<
           " NNShapeRecognizer::prepareNetworkArchitecture()" << endl;

		LTKReturnError(returnStatus);
	}

	//Backpropogate function for creating the network
	returnStatus = adjustWeightByErrorBackpropagation(outptr,errptr,ep);

	if(returnStatus != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<<  returnStatus << " " <<
           " NNShapeRecognizer::prepareNetworkArchitecture()" << endl;

		LTKReturnError(returnStatus);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting NeuralNetShapeRecognizer::prepareNetworkArchitecture" <<endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: initialiseNetwork
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::initialiseNetwork(double2DVector& outptr,double2DVector& errptr)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::initialiseNetwork()" << endl;

	//Chack the total number of layer is correctly specified
	int numLayer = (m_layerOutputUnitVec.size());

	if((m_neuralnetNumHiddenLayers + 3) != numLayer) // 3 because input layer + output layer + one extra
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EINVALID_NETWORK_LAYER << "(Network layer does not match) " <<
            getErrorMessage(EINVALID_NETWORK_LAYER)<<
            " NeuralNetShapeRecognizer::initialiseNetwork()" << endl;

        LTKReturnError(EINVALID_NETWORK_LAYER);
	}

	int index;

	//initialised all the parameter which are need
	for(index = 0; index < (m_neuralnetNumHiddenLayers + 2); ++index)
	{
		doubleVector tempDoubleVec(((m_layerOutputUnitVec[index] + 1) * m_layerOutputUnitVec[index+1]));

		m_connectionWeightVec.push_back(tempDoubleVec);

		m_delW.push_back(tempDoubleVec);

		m_previousDelW.push_back(tempDoubleVec);

		tempDoubleVec.clear();

		doubleVector tempDoubleV((m_layerOutputUnitVec[index] + 1));

		outptr.push_back(tempDoubleV);

		errptr.push_back(tempDoubleV);

		tempDoubleV.clear();
	}

	for(index = 0; index < (m_neuralnetNumHiddenLayers + 1); ++index)
	{
		outptr[index][m_layerOutputUnitVec[index]] = 1.0;
	}

	if(!m_isNeuralnetWeightReestimate)
	{
		//For randomly initialised the weight of the network
		srand(m_neuralnetRandomNumberSeed);

		for(index = 0; index < (m_neuralnetNumHiddenLayers + 2); ++index)
		{
			for(int i = 0; i < ((m_layerOutputUnitVec[index] + 1) * m_layerOutputUnitVec[index+1]); ++i)
			{
				m_connectionWeightVec[index][i] = (double) ((double)rand()/((double)RAND_MAX) - 0.5);
				m_delW[index][i] = 0.0;
				m_previousDelW[index][i] = 0.0;
			}
		}
	}
	else
	{
		//Initialised neuralnet from previously train weight
		cout<<"Loading initial weight and acrhitecture from previously train data"<<endl;
		int returnStatus = loadModelData();

		if(returnStatus != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Error: " <<
				getErrorMessage(returnStatus) <<
				" NeuralNetShapeRecognizer::initialiseNetwork()" << endl;

			LTKReturnError(returnStatus);
		}
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting NeuralNetShapeRecognizer::initialiseNetwork()" <<endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: feedForward
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::feedForward(const vector<LTKShapeFeaturePtr>& shapeFeature,double2DVector& outptr,const int& currentIndex)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entaring " <<
        "NeuralNetShapeRecognizer::feedForward" << endl;

	if(shapeFeature.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty traning set) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::feedForward()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	if(m_layerOutputUnitVec.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty network layer) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::feedForward()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	if(m_connectionWeightVec.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty network weights) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::feedForward()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	double normalizationFactor = m_neuralnetNormalizationFactor;

	if(normalizationFactor <= 0.0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENON_POSITIVE_NUM <<"(Normalised factor should be posative)" <<
			getErrorMessage(ENON_POSITIVE_NUM)<<
			" NeuralNetShapeRecognizer::feedForward()" << endl;

		LTKReturnError(ENON_POSITIVE_NUM);
	}

	int index;

	int offset;

	int lInddex; //Layer index

	int nodeIndex; // node index

	double net;

	//input level output calculation
	vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIter = shapeFeature.begin();
    vector<LTKShapeFeaturePtr>::const_iterator shapeFeatureIterEnd = shapeFeature.end();

	nodeIndex = 0;

	//	assign content to input layer
	//itaretor through all the feature vector
    for(; shapeFeatureIter != shapeFeatureIterEnd; ++shapeFeatureIter)
    {
		floatVector floatFeatureVector;

		(*shapeFeatureIter)->toFloatVector(floatFeatureVector);

		int vectorSize = floatFeatureVector.size();

		for (int i=0; i< vectorSize; i++)
		{
			double floatValue = floatFeatureVector[i];

			// Normalised the feature, so that the feature value lies between 0 to 1
			floatValue /= normalizationFactor;

			outptr[0][nodeIndex++] = floatValue;
		}
	}

	//	assign output(activation) value
	//	to each neuron usng sigmoid func
    //hidden & output layer output calculation
	for (lInddex = 1; lInddex < m_neuralnetNumHiddenLayers + 2; lInddex++) // For each layer
	{
		// For each neuron in current layer
		for (nodeIndex = 0; nodeIndex < m_layerOutputUnitVec[lInddex]; nodeIndex++)
		{
			net=0.0;

			// For input from each neuron in preceeding layer
			for (index = 0; index < m_layerOutputUnitVec[lInddex - 1]+1; index++)
			{
				offset = (m_layerOutputUnitVec[lInddex - 1]+1) * nodeIndex + index;

				// Apply weight to inputs and add to net
				net += (m_connectionWeightVec[lInddex - 1][offset] * outptr[lInddex - 1][index]);
			}

			// Apply sigmoid function
			outptr[lInddex][nodeIndex] = calculateSigmoid(net);
		}

	}

	//	returns currentIndex'th output of the net
	for (nodeIndex = 0; nodeIndex < m_layerOutputUnitVec[m_neuralnetNumHiddenLayers + 1]; nodeIndex++)
	{
		m_outputLayerContentVec[currentIndex][nodeIndex] = outptr[m_neuralnetNumHiddenLayers + 1][nodeIndex];
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::feedForward()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: introspective
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::introspective(const doubleVector& ep, const double currentError, const int& currentItr, int& outConvergeStatus)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entaring " <<
        "NeuralNetShapeRecognizer::introspective" << endl;

	if(ep.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty individual error set) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::introspective()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	if(currentError < 0.0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENEGATIVE_NUM << "(Current error can't be nagative) " <<
            getErrorMessage(ENEGATIVE_NUM)<<
            " NeuralNetShapeRecognizer::introspective()" << endl;

        LTKReturnError(ENEGATIVE_NUM);
	}

	if(currentItr < 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< ENEGATIVE_NUM << "(Itaretion can't be nagative) " <<
            getErrorMessage(ENEGATIVE_NUM)<<
            " NeuralNetShapeRecognizer::introspective()" << endl;

        LTKReturnError(ENEGATIVE_NUM);
	}

	bool isConverge;

	int index;

	int nsnew;

	int traningSetSize  = m_trainSet.size();

	// reached max. iteration ?
	if (currentItr >= m_neuralnetMaximumIteration)
	{
		cout<<"Successfully complete traning (Maximum iteration reached)"<<endl;

		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
			"Successfully complete traning (Maximum iteration reached) : " <<
			currentItr << endl;

		outConvergeStatus = (EXIT_FAILURE);
	}
	else
	{
		// error for each pattern small enough?
		isConverge = true;

		for (index = 0; (index < traningSetSize) && (isConverge); index++)
		{
			if (ep[index] >= m_neuralnetIndividualError)
				isConverge = false;
		}

		if(isConverge)
		{
			cout<<"Successfully complete traning (individual error suficently small) : "<<endl;

			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
				"Successfully complete traning (Individual error suficently small) : " << endl;

			outConvergeStatus = (EXIT_SUCCESSFULLY);
		}
		else
		{
			// system total error small enough?
			if (currentError <= m_neuralnetTotalError)
			{
				cout<<"Successfully complete traning (Total error suficently small) : "<<endl;

				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<
					"Successfully complete traning (Total error suficently small) : " << endl;

				outConvergeStatus = (EXIT_SUCCESSFULLY);
			}
			else
			{
				//Continue traning itaretion
				outConvergeStatus = (CONTINUE);
			}
		}
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::introspective" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: adjustWeightByErrorBackpropagation
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::adjustWeightByErrorBackpropagation(double2DVector& outptr,double2DVector& errptr,doubleVector& ep)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entaring " <<
        "NeuralNetShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

	if(outptr.size() == 0 || errptr.size() == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	int totalNumberOfSample = m_trainSet.size();

	if(totalNumberOfSample == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR) << "Error: "<< EEMPTY_VECTOR << "(Empty traning set) " <<
            getErrorMessage(EEMPTY_VECTOR)<<
            " NeuralNetShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

        LTKReturnError(EEMPTY_VECTOR);
	}

	int outResult;

	int  nIndex;	// node index

	int offset;		// offfset index of the vector

	int layerIndex;	//Layer index

	int nodeIndex; // node or neuron index

	int sampleIndex;

    int nsold = 0;

    int itaretorIndex = 0; // itaretorIndex

	double actualOutput;

	double currentError;

	ep = doubleVector( totalNumberOfSample ); //errror per sample // individual error

	cout<<"After preparing traning sequence" <<
			"(made all shape same number of traning sample" <<
			" as the highest number of sample present in orginal traning list) :"<<
			totalNumberOfSample << endl;

	/////////////////////////////////////////////////////
	outResult = CONTINUE;

	do
	{
        currentError = 0.0;
        //for each pattern
        for(sampleIndex = 0; sampleIndex < totalNumberOfSample; sampleIndex++)
		{

			vector<LTKShapeFeaturePtr> shapeFeature = m_trainSet[sampleIndex].getFeatureVector();
            //bottom_up calculation
			//	update output values for each neuron
            int errorCode = feedForward(shapeFeature,outptr,sampleIndex);

			if(errorCode != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                   " NNShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

				LTKReturnError(errorCode);
			}

			shapeFeature.clear();

            //top_down error propagation output_level error
			//	find error for output layer
            for(layerIndex = 0; layerIndex < m_layerOutputUnitVec[ (m_neuralnetNumHiddenLayers + 1) ]; layerIndex++)
			{
               actualOutput= outptr[ (m_neuralnetNumHiddenLayers+1)][layerIndex];
               errptr[(m_neuralnetNumHiddenLayers + 1)][layerIndex] = ((m_targetOutputVec[sampleIndex][layerIndex] - actualOutput) *
																			(1 - actualOutput) * actualOutput);
            }

            // hidden & input layer errors
			for(layerIndex = (m_neuralnetNumHiddenLayers+1); layerIndex >= 1; layerIndex--)
			{
				for(nodeIndex = 0; nodeIndex < m_layerOutputUnitVec[layerIndex - 1]+1; nodeIndex++)
				{
					errptr[layerIndex-1][nodeIndex] = 0.0;
					for(nIndex = 0; nIndex < m_layerOutputUnitVec[layerIndex]; nIndex++)
					{
						offset = (m_layerOutputUnitVec[layerIndex-1]+1) * nIndex + nodeIndex;

						m_delW[layerIndex-1][offset] = m_neuralnetLearningRate * (errptr[layerIndex][nIndex]) *
										(outptr[layerIndex - 1][nodeIndex]) + m_neuralnetMomemtumRate *
										(m_previousDelW[layerIndex - 1][offset]);

						errptr[layerIndex-1][nodeIndex] += (errptr[layerIndex][nIndex]) *
										(m_connectionWeightVec[layerIndex - 1][offset]);
					}

					(errptr[layerIndex - 1][nodeIndex]) = (errptr[layerIndex - 1][nodeIndex]) *
										(1- (outptr[layerIndex - 1][nodeIndex])) *
										(outptr[layerIndex - 1][nodeIndex]);
				}
			}

            // connection weight changes
			//	adjust weights usng steepest descent
			for(layerIndex = 1 ; layerIndex < (m_neuralnetNumHiddenLayers + 2); layerIndex++)
			{
				for(nodeIndex = 0; nodeIndex < m_layerOutputUnitVec[layerIndex]; nodeIndex++)
				{
					for(nIndex = 0; nIndex < m_layerOutputUnitVec[layerIndex-1]+1; nIndex++)
					{
						offset= (m_layerOutputUnitVec[layerIndex - 1] + 1) * nodeIndex + nIndex;

						(m_connectionWeightVec[layerIndex - 1][offset]) += (m_delW[layerIndex - 1][offset]);
					}
				}
			}

            //current modification amount saved
			for(layerIndex = 1; layerIndex < m_neuralnetNumHiddenLayers + 2; layerIndex++)
			{
				for(nodeIndex = 0; nodeIndex < m_layerOutputUnitVec[layerIndex]; nodeIndex++)
				{
					for(nIndex = 0; nIndex < m_layerOutputUnitVec[layerIndex - 1] + 1; nIndex++)
					{
						offset = (m_layerOutputUnitVec[layerIndex - 1] + 1) * nodeIndex + nIndex;

						(m_previousDelW[layerIndex - 1][offset]) = (m_delW[layerIndex - 1][offset]);
					}
				}
			}

			//update individula error for find mean square error
			ep[sampleIndex] = 0.0;

			for(layerIndex = 0; layerIndex < m_layerOutputUnitVec[m_neuralnetNumHiddenLayers+1]; layerIndex++)
			{

				ep[sampleIndex] += fabs((m_targetOutputVec[sampleIndex][layerIndex] -
										(outptr[m_neuralnetNumHiddenLayers+1][layerIndex])));

			}

			currentError +=ep[sampleIndex]  * ep[sampleIndex];

			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<sampleIndex <<" ->"  <<currentError <<endl;

		}

		//mean square error
		currentError =0.5 * currentError / (double)( totalNumberOfSample * m_numShapes );

		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"itaretion = "<<itaretorIndex<<"||"<<"Mean square error = "<<currentError<<endl;

		cout <<"Itaretion = " << itaretorIndex <<"||"
				<<"Mean square error = " << currentError << endl;

		itaretorIndex++;

        // check condition for terminating learning
		int errorCode = introspective(ep,currentError,itaretorIndex,outResult);
		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                   " NNShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

			LTKReturnError(errorCode);
		}

	} while (outResult == CONTINUE);

	//update output with changed weights
	for (sampleIndex = 0; sampleIndex < totalNumberOfSample; sampleIndex++)
	{
		vector<LTKShapeFeaturePtr> shapeFeature = m_trainSet[sampleIndex].getFeatureVector();

		int errorCode = feedForward(shapeFeature,outptr,sampleIndex);

		if(errorCode != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                   " NNShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

			LTKReturnError(errorCode);
		}

		shapeFeature.clear();
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "NeuralNetShapeRecognizer::adjustWeightByErrorBackpropagation()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: calculateSigmoid
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
double NeuralNetShapeRecognizer ::calculateSigmoid(double inNet)
{
		return (double)(1/(1+exp(-inNet)));
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: addClass
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::addClass(const LTKTraceGroup& sampleTraceGroup, int& shapeID)
{
    // Should be moved to nnInternal
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "NeuralNetShapeRecognizer::addClass()" << endl;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
        <<"Exiting NeuralNetShapeRecognizer::addClass"<<endl;
    return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: addSample
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::addSample(const LTKTraceGroup& sampleTraceGroup, int shapeID)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
		"NeuralNetShapeRecognizer::addSample()" << endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)
		<<"Exiting NeuralNetShapeRecognizer::addSample"<<endl;
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			:
 * NAME			: deleteClass
 * DESCRIPTION	:
 * ARGUMENTS	:
 * RETURNS		: int:
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int NeuralNetShapeRecognizer::deleteClass(int shapeID)
{
    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering NeuralNetShapeRecognizer::deleteClass" <<endl;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting NeuralNetShapeRecognizer::deleteClass" <<endl;

    return SUCCESS;
}
