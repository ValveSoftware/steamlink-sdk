#ifndef __NEURALNETSHAPERECOGNIZER_H
#define __NEURALNETSHAPERECOGNIZER_H

/**            Include files       */
#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKMacros.h"
#include "LTKShapeRecognizer.h"
#include "LTKShapeRecoUtil.h"
#include "LTKShapeSample.h"
#include "LTKCheckSumGenerate.h"

/**            Forward declaration of classes          */
class LTKTraceGroup;
class LTKPreprocessorInterface;
class LTKShapeSample;
class LTKShapeFeatureExtractor;
class LTKShapeFeature;

#define SUPPORTED_MIN_VERSION "3.0.0"

typedef int (*FN_PTR_CREATELTKLIPIPREPROCESSOR)(const LTKControlInfo& , LTKPreprocessorInterface** );
typedef int (*FN_PTR_DELETELTKLIPIPREPROCESSOR)(LTKPreprocessorInterface* );


/**
 * @ingroup NeuralNetShapeRecognizer.h
 * @brief The Header file for the NeuralNetShapeRecognizer
 * @class NeuralNetShapeRecognizer
 *<p> Fully connected feedforward multilayer perceptron 
 *		backpropogation	algorithm used for its training.
 *</p>
 */

class NeuralNetShapeRecognizer: public LTKShapeRecognizer
{

    /** @name private data members */
        //@{
    private:

        FN_PTR_DELETELTKLIPIPREPROCESSOR m_deleteLTKLipiPreProcessor;
        //Function pointer for deleteLTKLipiPreProcessor

        // preproc lib handle
        void *m_libHandler;

        // feature extractor lib handle
        void *m_libHandlerFE;

        unsigned short m_numShapes;
        /**< @brief Number of shapes
         *     <p>
         *     It Defines the number of shapes to be recognized
         *
         *     DEFAULT: 0
         *
         *     Note: If the project is dynamic, then this numShapes is set to 0
         *            If the project is not dynamic, then the numShapes is read from project configuration file
         *     </p>
         */

        string m_featureExtractorName;
        /**< @brief The Feature Extractor
         * <p>
         *
         *     DEFAULT:PointFloat
         *
         * </p>
         */

        bool m_projectTypeDynamic;
        /**< @brief Project Dynamic
         *     <p>
         *     if projectTypeDynamic    = true, then the project is dynamic i.e., the numShapes can take any number of value(s)
         *                    = false, then the project is not dynamic i.e., the numShapes take value from project.cfg file
         *
         *     DEFAULT: false
         *     </p>
         */

        LTKPreprocessorInterface *m_ptrPreproc;
        /**< @brief Pointer to preprocessor instance
         *     <p>
         *     Instance which is used to call the preprocessing methods before recognition
         *
         *     DEFAULT: NULL
         *     </p>
         */

        string m_neuralnetCfgFilePath;
        /**< @brief Full path of NeuralNet configuration file
         *     <p>
         *     Assigned value in the NeuralNetShapeRecognizer::initialize function
         *     </p>
         */

        string m_neuralnetMDTFilePath;
        /**< @brief Full path of Model data file
         *     <p>
         *     Assigned value in the NeuralNetShapeRecognizer::initialize function
         *     </p>
         */

        stringStringMap m_headerInfo;
        /**< @brief Header Information
         *     <p>
         *     </p>
         */

        LTKShapeRecoUtil m_shapeRecUtil;
        /**< @brief Pointer to LTKShapeRecoUtil class
         *     <p>
         *     Instance used to call Shape Recognizer Utility functions
         *
         *     DEFAULT: NULL
         */

        string m_lipiRootPath;
        /**< @brief Path of the Lipi Root
         *     <p>
         *     DEFAULT: LipiEngine::getLipiPath()
         *     </p>
         */

        string m_lipiLibPath;
        /**< @brief Path of the Lipi Libraries
         *     <p>
         *     DEFAULT: LipiEngine::getLipiPath()
         *     </p>
         */

        LTKShapeFeatureExtractor *m_ptrFeatureExtractor;
        /**< @brief Pointer to LTKShapeFeatureExtractor class
         *     <p>
         *     DEFAULT: NULL
         *     </p>
         */

        string m_preProcSeqn;
        /**< @brief Preprocessor Sequence
         *     <p>
         *     This string holds the sequence of preprocessor methods
         *     </p>
         */

        vector<stringStringPair> m_preprocSequence;
		/**< @brief Preprocessor Sequence vector
         *     <p>
         *     This vector of string holds the sequence of preprocessor methods
         *     </p>
         */

        vector<LTKShapeRecoResult> m_vecRecoResult;
        /**< @brief Store Recognize results
         *     <p>
         *			This vector hold shape ID and its confidance value
         *     </p>
         */

		int m_neuralnetRandomNumberSeed;
		/**< @brief Store seed value for random number generator 
		 *
		 *	<p>
		 *		This integer variable hold the seed value of random number generator for initialization of network weights
		 *
		 *		DEFAULT : NEURALNET_DEF_RANDOM_NUMBER_SEED	
		 *	</p>
		 */

		float m_neuralnetNormalizationFactor;
		/**< @brief Store the normalisation factor to normalise the input feature component value between (-1 to +1)
		 *	It is used during genaration of training / testing feature vector
		 *	<p>
		 *
		 *		DEFAULT : NEURALNET_DEF_NORMALIZE_FACTOR
		 *	</p>
		 */

		float m_neuralnetLearningRate;
		/**< @brief Learning rate of backpropagation algorithm 
		 *
		 *	<p>
		 *
		 *		DEFAULT : NEURALNET_DEF_LEARNING_RATE
		 *	</p>
		 */

		float m_neuralnetMomemtumRate;
		/**< @brief Momentum rate of backpropagation algorithm
		 *
		 *	<p>
		 *
		 *		DEFAULT : NEURALNET_DEF_MOMEMTUM_RATE
		 *	</p>
		 */

		double m_neuralnetTotalError;
		/**< @brief   Store the threshold value of the system error
		 *				(i.e total error of network response during an epoch)
		 *
		 *	<p>
		 *		Used for training convergence criterion
		 *
		 *		DEFAULT : NEURALNET_DEF_TOTAL_ERROR
		 *	</p>
		 */

		double m_neuralnetIndividualError;
		/**< @brief Store the threshold value of the network response error per
		 *				inidividual sample
		 *
		 *	<p>
		 *		Used for training convergence criterion
		 *
		 *		DEFAULT :  NEURALNET_DEF_INDIVIDUAL_ERROR
		 *	</p>
		 */

		int m_neuralnetNumHiddenLayers;
		/**< @brief Number of hidden layers of the neural net 
		 *
		 *	<p>
		 *
		 *		DEFAULT : NEURALNET_DEF_HIDDEN_LAYERS_SIZE
		 *	</p>
		 */

		int m_neuralnetMaximumIteration;
		/**< @brief Number of iteration 
		 *
		 *	<p>
		 *		Used for upper bound of traning iteration
		 *
		 *		DEFAULT : NEURALNET_DEF_MAX_ITR
		 *	</p>
		 */

		bool m_isCreateTrainingSequence;
		/**< @brief If true generate input sequence of feature vectors for training 
		 *
		 *	<p>
		 *
		 *		DEFAULT : true
		 *	</p>
		 */

		double2DVector m_connectionWeightVec;
		/**< @brief Store network connection weight
         *     <p>
         *
         *     </p>
         */

		double2DVector m_delW;
		/**< @brief Store error derivatives with respect to network weights
         *     <p>
         *
         *     </p>
         */

		double2DVector m_previousDelW;
		/**< @brief  Store values of m_delW during last but one iteration
         *     <p>
         *
         *     </p>
         */

		double2DVector m_outputLayerContentVec;
		/**< @brief Store values of output layer node
         *     <p>
         *
         *     </p>
         */

		double2DVector m_targetOutputVec;
		/**< @brief Store values of target output vactor components for each training sample
         *     <p>
         *
         *     </p>
         */

		intVector m_layerOutputUnitVec;
		/**< @brief Stores numbere of nodes at different layers of the network
         *     <p>
         *
         *     </p>
         */
		
		bool m_isNeuralnetWeightReestimate;
		/**< @brief If true the neural net is initialised using a previously trained network information
		 *		this information is obtained from neuralnet.mdt file
         *     <p>
         *
         *     </p>
         */

        float m_rejectThreshold;
        /**< @brief Threshold on the confidence to declare a test sample rejected
         *     <p>
         *			DEFAULT : 0.01
         *     </p>
         */

		intVector m_sampleCountVec;
		/**< @brief Number of training samples present in each category
		*		<p>
		*
		*		</p>
		*/

		LTKCaptureDevice m_captureDevice;

		LTKOSUtil*  m_OSUtilPtr;
		/**< @brief Pointer to LTKOSUtil interface
         *     <p>
         *
         *     </p>
         */

        vector<LTKShapeSample> m_trainSet;
		/**< @brief Store ShapeSampleFeatures for all training samples
         *				Used during neuralnet training
         *     <p>
         *
         *			DEFAULT : 0.01
         *     </p>
         */
		
		string m_MDTFileOpenMode;
		/**< @brief Store the neuralnet.mdt file open mode (Ascii or Binary)
         *     <p>
         *			Used during open neuralnet.mdt file
		 *
         *			DEFAULT : Ascii
         *     </p>
         */

        //@}

        string m_currentVersion;

    public:

        /** @name Constructors and Destructor */
        //@{

        NeuralNetShapeRecognizer(const LTKControlInfo& controlInfo);

        /**
         * Destructor
         */
        ~NeuralNetShapeRecognizer();

        //@}

        /**
         * This method initializes the NeuralNet shape recognizer
         * <p>
         * Semantics
         *   - Set the project name in NeuralNetShapeRecognizer::headerInfo with the parameter passed <br>
         *          m_headerInfo[PROJNAME] = strProjectName;
         *
         *   -  Initialize NeuralNetShapeRecognizer::m_neuralnetCfgFilePath <br>
         *          m_neuralnetCfgFilePath = NeuralNetShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
         *                          strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
         *                          LTKInc::SEPARATOR + LTKInc::NEURALNET + LTKInc::CONFIGFILEEXT;
         *
         *   -  Initializes NeuralNetShapeRecognizer::m_neuralnetMDTFilePath <br>
         *          m_neuralnetMDTFilePath = NeuralNetShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
         *                            strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
         *                            LTKInc::SEPARATOR + LTKInc::NEURALNET + LTKInc::DATFILEEXT;
         *
         *   - Initializes NeuralNetShapeRecognizer::m_projectTypeDynamic with the value returned from LTKShapeRecoUtil::isProjectDynamic
         *
         *   - Initialize the preprocessor using LTKShapeRecoUtil::initializePreprocessor and assign
         *      default values for
         *           -# Normalised size
         *           -# Threshold size
         *           -# Aspect ratio
         *           -# Dot threshold
         *
         *   - Initialize the instance variables of neuralnet recognizer with the values given in classifier config file
         *
         * </p>
         * @param strProjectName : string : Holds the name of the Project
         * @param strProfileName : string : Holds the name of the Profile
         *
         * @return int : LTKInc::SUCCESS          if initialization done successfully
         *                    errorValues if initialization has some errors
         *
         * @exception LTKErrorList::ECONFIG_FILE_OPEN            Could not open project.cfg
         * @exception LTKErrorList::EINVALID_NUM_OF_SHAPES       Negative value for number of shapes
         * @exception LTKErrorList::ELOAD_PREPROC_DLL            Could not load preprocessor DLL
         * @exception LTKErrorList::EDLL_FUNC_ADDRESS_CREATE     Could not map createPreprocInst
         * @exception LTKErrorList::EDLL_FUNC_ADDRESS_DELETE     Could not map destroyPreprocInst
         */

        /**
         * This method calls the train method of the NeuralNet classifier
         *
         */
        int train(const string& trainingInputFilePath,
                const string& mdtHeaderFilePath,
                const string &comment,const string &dataset,
                const string &trainFileType=INK_FILE) ;

        /**
         * This method loads the Training Data of the NeuralNet classifier
         * @param
         * @return LTKInc::SUCCESS : if the model data was loaded successfully
         * @exception
         */
        int loadModelData();

        /**
         * This method unloads all the training data
         * @param none
         * @return LTKInc::SUCCESS : if the model data was unloaded successfully
         * @exception none
         */
        int unloadModelData();

        /**
         * This method sets the device context for the recognition
         *
         * @param deviceInfo parameter to be set
         * @return
         * @exception
         */
        int setDeviceContext(const LTKCaptureDevice& deviceInfo);

        /**
         * Populates a vector of LTKShapeRecoResult consisting of top classes with their confidences
         *
         * Semantics
         *
         *     - Validate the input arguments
         *     - Extract the features from traceGroup
         *     - Compute the confidences of the classes, call computeConfidence()
         *     - Check if the first element of resultVector has confidence less than m_rejectThreshold, if so,
         empty the resultVector (reject the sample), log and return.
         *     - If the confThreshold value was specified by the user (not equal to -1),
         delete the entries from resultVector with confidence values less than confThreshold
         *     - If the numChoices value was specified by the user (not equal to -1),
         update the resultVector with top numChoices entries, delete other values
         *
         * @param traceGroup The co-ordinates of the shape which is to be recognized
         * @param screenContext Contains information about the input sample whether it is written inside a preprinted box
         *                 or unconstrained  continuous writing
         * @param subSetOfClasses A subset of the entire class space which is to be used for
         *                        recognizing the input shape.
         * @param confThreshold Classes with confidence below this threshold are not returned,
         *               valid range of confThreshold: (0,1)
         * @param numOfChoices Number of top choices to be returned in the result structure
         * @param resultVector The result of recognition
         *
         * @return SUCCESS: resultVector populated successfully
         *         FAILURE: return ErrorCode
         * @exception none
         */
        int recognize(const LTKTraceGroup& traceGroup,
                const LTKScreenContext& screenContext,
                const vector<int>& subSetOfClasses,
                float confThreshold,
                int  numChoices,
                vector<LTKShapeRecoResult>& outResultVector);


          /* Overloaded the above function to take vector<LTKShapeFeaturePtr> as
           * input
           */
         int recognize(const vector<LTKShapeFeaturePtr>& shapeFeatureVec,
               const vector<int>& subSetOfClasses,
               float confThreshold,
               int  numChoices,
               vector<LTKShapeRecoResult>& resultVector);

        /**
         * This method add Class
         *
         * Semantics
         *
         *
         * @param sampleTraceGroup : LTKTraceGroup : Holds TraceGroup of sample to add
         * @param shapeID          : int : Holds shapeID of new Class
         *
         * @return SUCCESS:Shape Class added successfully
         *         FAILURE: return ErrorCode
         * @exception none
         */

        int addClass(const LTKTraceGroup& sampleTraceGroup, int& shapeID);

          /**
          * This method add Sample Class for adapt
          *
          * Semantics
          *
          *
          * @param sampleTraceGroup : LTKTraceGroup : Holds TraceGroup of sample to Add
          * @param shapeID         : int : Holds shapeID of new Class
          *
          * @return SUCCESS:Shape Class added successfully
          *         FAILURE: return ErrorCode
          * @exception none
          */
          int addSample(const LTKTraceGroup& sampleTraceGroup, int shapeID);

        /**
         * This method delete Class
         *
         * Semantics
         *
         * @param shapeID          : int : Holds shapeID of Shape to be deleted
         *
         * @return SUCCESS: Shape Class deleted successfully
         *         FAILURE: return ErrorCode
         * @exception none
         */
        int deleteClass(int shapeID);

        /**
         * This method converts features to TraceGroup
         *
         * Semantics
         *
         *
         * @param shapeID          : int : Holds shapeID
         * @param numberOfTraceGroups : int : Maximum number of Trace Groups to populate
         * @param outTraceGroups : vector<LTKTraceGroup> : TraceGroup
         *
         * @return SUCCESS: 
         *         FAILURE:
         * @exception none
         */
        int getTraceGroups(int shapeID, int numberOfTraceGroups, vector<LTKTraceGroup> &outTraceGroups);

          
    private:
        /**
         * This function is the method for generation of network architecture,
		 *		creation of training samples and learning of network weights
         *
         *
         * Semantics
         *
         *     - Note the start time for computation of execution time
         *
         *     - Call train method depending on the inFileType
         *          - NeuralNetShapeRecognizer::trainFromListFile() if inFileType = LTKMacros::INK_FILE
         *          - NeuralNetShapeRecognizer::trainFromFeatureFile() if inFileType = LTKMacros ::FEATURE_FILE
         *
		 *	   - Call prepareNeuralNetTrainingSequence method depending on the m_isCreateTratningSequence
		 *			- NeuralNetShapeRecognizer::prepareNeuralNetTrainingSequence() if m_isCreateTrainingSequence = true
		 *
		 *	   - Call NeuralNetShapeRecognizer::prepareNetworkArchitecture()
		 *
         *     - Update the headerInfo with algorithm version and name using NeuralNetShapeRecognizer::updateHeaderWithAlgoInfo() method
         *
         *     - Calculate the checksum.
         *
		 *	   - Call NeuralNetShapeRecognizer::writePrototypeSetToMDTFile()
		 *
         *     - Note the termination time for execution time computation
         *
         *
         * @param inputFilePath :string : Path of trainListFile / featureFile
         * @param strModelDataHeaderInfoFile : string : Holds the Header information of Model Data File
         * @param inFileType : string : Possible values ink / featureFile
         *
         * @return LTKInc::SUCCESS : if the training done successfully
         * @return errorCode : if it contains some error training fails
         */
        int trainNetwork(const string& trainingInputFilePath,
                const string& mdtHeaderFilePath,
                const string& trainFileType);


        /**
         * This method maps between the module names and respective function names from the cfg file
         *
         * Semantics
         *
         *     - Read the Preprocess Sequence from the neuralnet.cfg file
         *
         *     - Split the sequence into tokens with delimiter LTKMacros::DELEMITER_SEQUENCE using LTKStringUtil::tokenizeString
         *
         *     - Split each token with delimiter LTKMacrosDELEMITER_FUNC using LTKStringUtil::tokenizeString
         *          to get the Module name and Function name
         *
         *     - Store the Module name and the Function name into a structure
         *
         *
         * @param none
         * @return LTKInc::SUCCESS      : if functions are successfully mapped,
         * @return errorCodes : if contains any error
         * @exception none
         */
        int mapPreprocFunctions();

        /**
         * This method will assign default values to the members
         *
         * Semantics
         *
         *     - Assign Default values to all the data members
         *
         *
         * @param  none
         *
         * @return none
         */
        void assignDefaultValues();

        /** Reads the NeuralNet.cfg and initializes the instance variable of the classifier with the user defined
         * values.
         *
         * Semantics
         *
         *     - Open the NeuralNet.cfg using LTKConfigFileReader
         *
         *     - In case of file open failure (NeuralNet.cfg), default values of the classifier parameters are used
         *
         *     - The valid values of the classifier parameters are cached into the class data members
         *        LTKConfigFileReader::getConfigValue is used to get the value for a key defined in the config file
         *
         *     - Exception is thrown if the user has specified an invalid value for a parameter
         *
         *
         * @param  none
         * @return SUCCESS   : If the Config file is read successfully
         * @return errorCode : If it contains some error
         * @exception LTKErrorList::ECONFIG_FILE_RANGE           The config file variable is not within the correct range
         */
        int readClassifierConfig();

        /**
         * This method creates a custom feature extractor instance and stores it's address in
         * 
		 * Semantics
         *
         *
         *     - Intialize the NeuralNetShapeRecognizer::m_ptrFeatureExtractor with address of the feature extractor instance created
         *        using LTKShapeFeatureExtractorFactory::createFeatureExtractor
         *
         * @param none
         *
         * @return 0 on LTKInc::SUCCESS and 1 on LTKInc::FAILURE
         *
         * @exception none
         */
        int initializeFeatureExtractorInstance(const LTKControlInfo& controlInfo);

        /**
         * This method trains the classifier from the file containing train list whose path is passed as a paramater
         *
         * Semantics
         *
         *     - Open the trainListFile for reading
         *
         *     - Open the mdt file for writing
         *
         *     - Write header information to the mdt file
         *          - NeuralNetShapeRecognizer::m_numShapes
         *          - NeuralNetShapeRecognizer::m_traceDimension
         *          - NeuralNetShapeRecognizer::m_flexibilityIndex
         *
         *     - Get a valid line from the train list file
         *          - Skip commented lines
         *          - Skip lines where number_of_tokens != 2
         *          - Throw error LTKErrorList::EINITSHAPE_NONZERO, if the first shape in the list file is not zero
         *          - Throw error LTKErrorList::EINVALID_ORDER_LISTFILE if the shapes are not in sequential order
         *
         *     - For every valid line get the ShapeSample from the ink file using NeuralNetShapeRecognizer::getShapeSampleFromInkFile
         *          - Read ink from UNIPEN  ink file
         *          - Skip if the trace group is empty
         *          - Pre process the trace group read from the ink file
         *          - Extract features
         *
         *     - Push all the ShapeSamples corresponding to a shape into a vector of ShapeSample ShapeSamplesVec
         *
         *
         * @param listFilePath : string : Holds the path for train list file
         *
         * @return none
         *
         * @exception LTKErrorList::EFILE_OPEN_ERROR        : Error in Opening a file (may be mdt file or list file)
         * @exception LTKErrorList::EINVALID_NUM_OF_SHAPES : Invalid value for number of shapes
         * @exception LTKErrorList::EINVALID_ORDER_LISTFILE: Invalid order of shapeId in List file
         * @exception LTKErrorList::EINITSHAPE_NONZERO      : Initial shapeId must not be zero
         */
        int trainFromListFile(const string& listFilePath);

        /**
         * This method trains the classifier from the feature file whose path is passed as paramater
         *
         * Semantics
         *
         *
         * @param featureFilePath : string : Holds the path of Feature file
         *
         * @return none
         */
        int trainFromFeatureFile(const string& featureFilePath);
		
		/**
         * This method is used to store the preprocess parameter for header of the .mdt file 
         *
         * Semantics
         *
         *
         * @param headerSequence : stringStringMap : Holds header value of .mdt file
         *
         * @return none
         */
        int PreprocParametersForFeatureFile(stringStringMap& headerSequence);

        /**
         * This method returns the ShapeSample by using the ink file path as input
         *
         * Semantics
         *
         *     - Call the LTKShapeRecoUtil::readInkFromFile() method (Utility Method) to read the ink file
         *        By reading this file, an inTraceGroup is generated
         *
         *     - Preprocess the inTraceGroup and get the preprocessed trace group
         *        LTKTraceGroup preprocessedTraceGroup
         *
         *     - Extract features from the preprocessed trace group to get the ShapeSamples.
         *
         *
         * @param path        : string : The path for Ink file
         * @param ShapeSample : ShapeSample : The ShapeSample generated after feature extraction
         *
         * @return SUCCESS : If the ShapeSample is generated successfully
         * @return FAILURE : Empty traces group detected for current shape
         *
         * @exception LTKErrorList::EINKFILE_EMPTY          : Ink file is empty
         * @exception LTKErrorList::EINK_FILE_OPEN          : Unable to open unipen ink file
         * @exception LTKErrorList::EINKFILE_CORRUPTED      : Incorrect or corrupted unipen ink file
         * @exception LTKErrorList::EEMPTY_TRACE            : Number of points in the trace is zero
         * @exception LTKErrorList::EEMPTY_TRACE_GROUP      : Number of traces in the trace group is zero
         */
        int getShapeFeatureFromInkFile(const string& inkFilePath,
                vector<LTKShapeFeaturePtr>& shapeFeatureVec);

        /**
         * This method will Update the Header information for the MDT file
         *
         * Semantics
         *
         *     - Copy the version number to a string
         *
         *     - Update the version info and algoName to NeuralNetShapeRecognizer::m_headerInfo, which specifies the
         *        header information for MDT file
         *
         *
         * @param none
         *
         * @return none

         * @exception none
         */
        void updateHeaderWithAlgoInfo();

		/**
         * This method do preprocessing for input trace group
         *
         * Semantics
         *
         * @param inTraceGroup : LTKTraceGroup : Store unipen ink file
				  outPreprocessedTraceGroup : LTKTraceGroup : Store preprocess ink of input ink
         * @return 
         * @exception 
         */
        int preprocess (const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outPreprocessedTraceGroup);

        /**
         * This method write training results to the mdt file
         *
         * Semantics
         *
         *     - If mdt file open mode is ASCII
		 *			- Write the number of Shape
		 *			- Write the connection weight
		 *			- Write the derivative of connection weight
         *
         *     - If mdt file open mode is BINARY
         *			- Write the number of Shape
		 *			- Write the connection weight
		 *			- Write the connection weight vector size
		 *			- Write the derivative of connection weight
         *
         * @param none
         * @return 
         * @exception 
         */
        int writeNeuralNetDetailsToMDTFile();

		/**
         * This method will writes training results to the mdt file
         *
         * Semantics
         *
         *     - If mdt file open mode is ASCII
		 *			- Write the number of Shape
		 *			- Write the connection weight
		 *			- Write the derivative of connection weight
         *
         *     - If mdt file open mode is BINARY
         *			- Write the number of Shape
		 *			- Write the connection weight
		 *			- Write the connection weight and derivative of connection weight vectors size
		 *			- Write the derivative of connection weight
         *
         *
         * @param resultVector          : double2DVector : A vector of weight or weight derivative created as a result of training
         *           mdtFileHandle      : ofstream       : Specifies the outut stream
		 *			 isWeight			: bool			 : If true then resultVector contain weight, if false then resultVector contain weight derivative
         *
         * @return none
         *
         * @exception none
         */
        int appendNeuralNetDetailsToMDTFile(const double2DVector& resultVector, const bool isWeight, ofstream & mdtFileHandle);

        int deleteFeatureExtractorInstance();
        
        /**
         * This method computes the confidences for underlying classes of a test sample
         *
         * Semantics
         *
         *     - Compute the confidence based on the values of m_outputLayerContentVec (output layer value of each node)
         *     - Populate the resultVector
         *     - Sort the resultVector
         *
         * @param none
         *
         * @return SUCCESS: resultVector populated
         *         FAILURE: return ErrorCode
         * @exception none
         */

        int computeConfidence();

        /**
         * The comparison function object of STL's sort() method, overloaded for class LTKShapeRecoResult, used to sort the vector of LTKShapeRecoResult based on the member variable confidence
         *
         * Semantics
         *
         *     - Compare the values of two objects
         *     - Return true or false
         *     -
         *
         * @param x : LTKShapeRecoResult : First object for comparison
         * @param y : LTKShapeRecoResult : Second object for comparison
         *
         * @return true: If x.confidence > y.confidence
         *         false: If x.confidence <= y.confidence
         * @exception none
         */
        static bool sortResultByConfidence(const LTKShapeRecoResult& x, const LTKShapeRecoResult& y);

        /** This method is used to initialize the PreProcessor
         *
         * Semantics
         *
         *     - Load the preprocessor DLL using LTKLoadDLL().
         *
         *     - Get the proc address for creating and deleting the preprocessor instance.
         *
         *     - Create preprocessor instance.
         *
         *     - Start the logging for the preprocessor module.
         *
         * @param  preprocDLLPath : string : Holds the Path of the Preprocessor DLL,
         * @param  errorStatus    : int    : Holds SUCCESS or Error Values, if occurs
         * @return preprocessor instance
         *
         * @exception    ELOAD_PREPROC_DLL             Could not load preprocessor DLL
         * @exception    EDLL_FUNC_ADDRESS_CREATE Could not map createPreprocInst
         * @exception    EDLL_FUNC_ADDRESS_DELETE Could not map destroyPreprocInst
         */
        int initializePreprocessor(const LTKControlInfo& controlInfo,
                LTKPreprocessorInterface** preprocInstance);


        /** This method is used to delete the PreProcessor instance
         *
         * Semantics
         *
         *     - Call deleteLTKPreprocInst from the preproc.dll.
         *
         *     - Unload the preprocessor DLL.
         *
         * @param  ptrPreprocInstance : Holds the pointer to the LTKPreprocessorInterface
         * @return none
         * @exception none
         */

        int deletePreprocessor();

        /** This method is used to unload the preprocessor DLL.
         *
         * Semantics
         *
         *     - If m_libHandler != NULL, unload the DLL
         *          LTKUnloadDLL(m_libHandler);
         *          m_libHandler = NULL;
         *
         * @param none
         * @return none
         * @exception none
         */
        int unloadPreprocessorDLL();

        /** This method is used to check the preprocess parameter are specified correcty in cfg file 
         *
         * Semantics
         *
         *
         * @param headerSequence : stringStringMap : 
         * @return 
         * @exception 
         */
		int validatePreprocParameters(stringStringMap& headerSequence);

		/**
		*  MLP is trained by backpropagation algorithm. Training samples (feature vector representation) from underlying
		*  classes are presented in an alternative sequence. Error for each such pattern is calculated at the output layer
		*  Error is propagated backward for top-to-bottom layer weight adjustments
		*
		* Semantics
		*
		*		   - Call feedForward method for bottom-to-top propagation of training feature
		*
        *          - Calculate the error at output layer for each nuron
		*
		*		   - Calculate weight adjustment amount for each sample presented
		*
		* @param : outptr : double2DVector : Hold the conteint of each node after forward propagation of a sample
		*		   errptr : double2DVector : Hold intermediate error values used for computation of error derivatives
        *									  at hidden & output layer
		*		   ep	  : doubleVector   : Hold output layer absolute error for individual samples
		* 
		* @return SUCCESS: successfully complete
        *         FAILURE: return ErrorCode
		*
		* @exception	LTKErrorList::EEMPTY_VECTOR		Vector which are used in this method is empty
		*/
		int adjustWeightByErrorBackpropagation(double2DVector& outptr,double2DVector& errptr,doubleVector& ep);
		
		/** This method is used to arrange training samples in an alternate sequence required for effective application of backpropagation algorithm 
		 *
         * Semantics
         *
		 *	   - If the number of training samples for each class is unequal, then it repeats  samples of those classes 
		 *			which have fewer samples in the training set
		 *
         *     - If m_trainSet is empty it returns error and stop training process.
         *
         * @param none
         * @return SUCCESS: successfully complete
         *         FAILURE: return ErrorCode
         * @exception 
         */
		int prepareNeuralNetTrainingSequence();
		
		/** This method is used to prepare architecture of neural network
         *
         * Semantics
         *
         *     - Call constractNeuralnetLayeredStructure method
		 *			- For creating input, hidden, and output layer structure
		 *
		 *	   - Call initialiseNetwork method.
		 *
		 *	   - Call adjustWeightByErrorBackpropagation method.
		 *			- For neuralnet training
         *
         * @param none
         * @return SUCCESS: successfully complete
         *         FAILURE: return ErrorCode
         * @exception none
         */
		int prepareNetworkArchitecture();
		
		/** This method is used to initialise neuralnet parameters
         *
         * Semantics
         *
         *     - Check specified network architecture during reestimation of network weights
		 *
		 *	   - Generally the network weights are initialised by random numbers
		 *
		 *	   - During reestimation of network (weights, prevDelW) neuralnet parameters are initialised by previously
		 *		  train network
		 *	   - Call loadModelData method
		 *
         *
         * @param 
         * @return SUCCESS: successfully complete
         *         FAILURE: return ErrorCode
		 *
         * @exception	LTKErrorList::EINVALID_NETWORK_LAYER	Network layer are not specified correctly
         */
		int initialiseNetwork(double2DVector& outptr,double2DVector& errptr);
		
		/**
		*  Feedforward input training sample
		*  Bottom up calculation of response at each node of network for a specific input pattern
		*
		* Semantics
		*
		*		   - It checks whether necessary vectors are empty
		*
        *          - Normalise the featurte vector so that each feature component should be in the range 0 to 1
		*
        *          - Calculate hidden and output layer node activation values using 
		*				calculateSigmoid method
		*
        *          - Call NeuralNetShapeRecognizer::calculateSigmoid() method for sigmoid units, the output varies 
		*				continuously but not linearly as the input changes <br>
		*				Sigmoid units bear a greater resemblance to real neurones than do linear or threshold units <br>
		*				Also, sigmoidal transfer function being derivable, it is
		*				useful for calculation of the error derivatives in backpropagation algorithm
        *          
		* @param :	shapeFeature	: vector<LTKShapeFeaturePtr> : Hold feature representation of input sample
					outptr : double2DVector : Hold the output layer content
		*			currentIndex : const int : Hold index of current sample
		* 
		* @return SUCCESS: successfully complete
        *         FAILURE: return ErrorCode
		*
		* @exception	LTKErrorList::EEMPTY_VECTOR		Vector which is used in this method is empty
		*				LTKErrorList::ENON_POSITIVE_NUM	Normalised factor should be positive
		*/
		int feedForward(const vector<LTKShapeFeaturePtr>& shapeFeature,double2DVector& outptr,const int& currentIndex);
		
		/** This method is used to take dissection for terminating training process if network converge or maximum itaretion reach
         *
         * Semantics
         *
         *     - Check if individual error set for each neuron are empty
		 *
		 *	   - Check if current error is negative
		 *
		 *	   - Check if training iteration is negative
		 *
		 *	   - If criteria for convergence or maximum training iteration is reached
		 *			- terminate else training process continue
         *
         * @param	ep : const doubleVector : Hold output layer absolute error for individual neuron
		 *			currentError : const double : Hold the current error
		 *			currentItr : const int	: Itaretion number
		 *			outConvergeStatus : int : Status of iterative learning after each iteration
		 *
         * @return SUCCESS: successfully complete
         *         FAILURE: return ErrorCode
		 *
         * @exception	LTKErrorList::EEMPTY_VECTOR		Empty individual error set
		 * @exception	LTKErrorList::ENEGATIVE_NUM		Current error can't be nagative
		 * @exception	LTKErrorList::ENEGATIVE_NUM		Itaretion can't be nagative
         */
		int introspective(const doubleVector& ep, const double currentError,const int& currentItr, int& outConvergeStatus);
		
		/** This method is used to construct the layered structure of network. 
         *
         * Semantics
         *
         *     - It initialise input layer and output layer nodes
         *
         * @param none
         * @return SUCCESS: successfully complete
		 *
         * @exception	LTKErrorList::EEMPTY_VECTOR		 If the feature set is empty
		 *				LTKErrorList::EINVALID_NUM_OF_INPUT_NODE	If input layer nodes are not correctly specified
		 *				LTKErrorList::EINVALID_NUM_OF_OUTPUT_NODE	If output layer nodes are not correctly specified
         */
		int constractNeuralnetLayeredStructure();
		
		/** This method is used to check if the network architecture parameters are specified correctly or not
         *
         * Semantics
         *
         *     - Check network cofig parameters
         *
         * @param none
         * @return SUCCESS: successfully complete
		 *         FAILURE: return ErrorCode
         * @exception
         */
		int validateNeuralnetArchitectureParameters(stringStringMap& headerSequence);
		
		/** This method is used to calculate sigmoid value of input value to a node 
         *
         * Semantics
         *
         * @param 
         * @return 
         * @exception
         */
		double calculateSigmoid(double inNet);

		int getShapeSampleFromString(const string& inString, LTKShapeSample& outShapeSample);
};


#endif
