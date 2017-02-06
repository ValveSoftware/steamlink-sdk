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
* $LastChangedDate: 2011-08-23 13:28:15 +0530 (Tue, 23 Aug 2011) $
* $Revision: 857 $
* $Author: jitender $
*
************************************************************************/
/*********************************************************************************************
* FILE DESCR: Definitions for ActiveDTW Shape Recognition module
*
* CONTENTS:
*
* AUTHOR: S Anand
*
*
* DATE:3-MAR-2009
* CHANGE HISTORY:
* Author       Date            Description of change
***********************************************************************************************/


#ifndef __ACTIVEDTWSHAPERECOGNIZER_H
#define __ACTIVEDTWSHAPERECOGNIZER_H

#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKMacros.h"
#include "LTKShapeRecognizer.h"
#include "LTKShapeRecoUtil.h"
#include "LTKShapeSample.h"
#include "LTKCheckSumGenerate.h"
#include "LTKDynamicTimeWarping.h"
#include "ActiveDTWShapeModel.h"
#include "ActiveDTWAdapt.h"

class LTKTraceGroup;
class LTKPreprocessorInterface;
class LTKShapeSample;
class LTKShapeFeatureExtractor;
class LTKShapeFeature;
class LTKAdapt;

#define SIMILARITY(distance) (1 / (distance + EPS ))
#define SUPPORTED_MIN_VERSION "3.0.0"

#define CLUSTER 0
#define SINGLETON 1

#define INVALID_SHAPEID -2
#define NEW_SHAPEID -2

//#ifdef _INTERNAL
//#endif

typedef int (*FN_PTR_LOCAL_DISTANCE)(LTKShapeFeaturePtr, LTKShapeFeaturePtr,float&);
typedef int (*FN_PTR_CREATELTKLIPIPREPROCESSOR)(const LTKControlInfo& , LTKPreprocessorInterface** );
typedef int (*FN_PTR_DELETELTKLIPIPREPROCESSOR)(LTKPreprocessorInterface* );

typedef vector<LTKShapeFeaturePtr> shapeFeature;
typedef vector<shapeFeature> shapeMatrix;


/**
* @ingroup ActiveDTWShapeRecognizer.h
* @brief The Header file for the ActiveDTWShapeRecognizer
* @class ActiveDTWShapeRecognizer
*<p> <p>
*/

class ActiveDTWShapeRecognizer: public LTKShapeRecognizer
{

public:
     //#ifdef _INTERNAL
     friend class LTKAdapt;
     int adapt(int shapeID );
     int adapt(const LTKTraceGroup& sampleTraceGroup, int shapeID );

     /**
     * This function does the recognition function required for training phase (called from trainLVQ)
     * The input parameter are the incharacter, which is compared with the existing
     * set of prototypes and then the matched code vector and along with its index (and also the shape id) is returned
     * @param incharacter is the character which we are trying to recognise.
     * @param returnshapeID is the value of the matched character which is returned, codeCharacter is the matched prototype (code vector) vector, and codeVecIndex is the matched prototype (code vector) index
     */
     int trainRecognize(LTKShapeSample& inShapeSample, LTKShapeSample& bestShapeSample, int& codeVecIndex);

     int writePrototypeShapesToMDTFile();

     int addClass(const LTKTraceGroup& sampleTraceGroup, int& shapeID);

     int deleteClass(int shapeID);

     int readInternalClassifierConfig();

private:

     int deleteAdaptInstance();



     //    #endif


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
     *    DEFAULT: 0
     *
     *    Note: If the project is dynamic, then this numShapes was set to 0
     *           If the project is not dynamic, then the numShapes was read from project configuration file
     *    </p>
     */


     string m_prototypeSelection;
     /**< @brief The Prototype Selection
     *    <p>
     *    if Prototype Selection = clustering, the training method used was clustering
     *
     *    DEFAULT: LTKPreprocDefaults::NN_DEF_PROTOTYPESELECTION
     *    Possible values are "clustering"
     *    </p>
     */

     int m_prototypeReductionFactor;
     /**< @brief The prototype Reduction factor
     *    <p>
     *    if PrototypeReductionFactor   = 0 every training sample is cluster on its own
     *                                            = 100 all training samples are represented by one prototype
     *                                       = 80 then all samples are represented by 20% of the training samples
     *
     *    DEFAULT: LTKPreprocDefaults::NN_DEF_PROTOTYPEREDUCTIONFACTOR
     *    RANGE:    0 TO 100
     *    </p>
     */

     int m_numClusters;
     /**< @brief The number of clusters
     *    <p>
     *    if NumClusters = k, then k clusters are found from the training samples
     *
     *
     *
     *    DEFAULT: There is no default as this and prototype reduction factor are dependent
     *    RANGE:
     *    </p>
     */


     float m_percentEigenEnergy;
     /**< @brief The percent of Eigen Energy
     *    <p>
     *    if PercentEigenEnergy = 90 then those eigen values contributing to 90% of the Eigen Energy,
     *                              and their corresponding eigen vectors are retained
     *
     *
     *    DEFAULT: LTKClassifierDefaults::ACTIVEDTW_DEF_PERCENTEIGENENERGY
     *    RANGE: 1-100
     *    </p>
     */

     int m_eigenSpreadValue;
     /**< @brief The eigen values spread range
     *    <p>
     *    if EigenSpreadValue = 16 then deformation parameters for computing the optimal deformation,
     *                            can lie in the range [-4*Eigen Value,4*Eigen Value]
     *
     *
     *    DEFAULT: LTKClassifierDefaults::ACTIVEDTW_DEF_EIGENSPREADVALUE
     *    RANGE: greater than 0
     *    </p>
     */

     int m_minClusterSize;
     /**< @brief The minimum cluster size
     *    <p>
     *    It specifies the minimum number of samples required to form a cluster
     *
     *
     *    DEFAULT: LTKPreprocDefaults::ADAPT_DEF_MIN_NUMBER_SAMPLES_PER_CLASS
     *    RANGE: >= 2
     *    </p>
     */

     bool m_useSingleton;
     /**< @brief Use Singletons
     *    <p>
     *    It specifies if singletons should be considered during the recognition process
     *      If Use Singleton is true, singletons will be considered during the recognition process,
     *      else they will be ignored
     *
     *    DEFAULT: LTKClassifierDefaults::ACTIVEDTW_DEF_USESINGLETON
     *    RANGE: True or False
     *    </p>
     */


     int m_nearestNeighbors;
     /**< @brief Nearest Neighbors
     *    <p>
     *
     *    DEFAULT: LTKClassifierDefaults::NN_DEF_NEARESTNEIGHBORS
     *    </p>
     */


     float m_dtwBanding;
     /**< @brief  DTW Banding
     *    <p>
     *
     *    DEFAULT: LTKClassifierDefaults::NN_DEF_BANDING
     *    </p>
     */

     int m_dtwEuclideanFilter;
     /**< @brief DTW Euclidean Filter
     *    <p>
     *
     *    DEFAULT: LTKClassifierDefaults::ACTIVEDTW_DEF_DTWEUCLIDEANFILTER
     *    </p>
     */

     string m_featureExtractorName;
     /**< @brief The Feature Extractor
     * <p>
     *
     *    DEFAULT:
     *
     * </p>
     */

     bool m_projectTypeDynamic;
     /**< @brief Project Dynamic
     *    <p>
     *    if projectTypeDynamic    = true, then the project is dynamic ie, the numShapes can take any number of value
     *                   = false, then the project is not dynamic ie, the numShape can take value specified in project.cfg file
     *
     *    DEFAULT: false
     *    </p>
     */

     LTKPreprocessorInterface *m_ptrPreproc;
     /**< @brief Pointer to preprocessor instance
     *    <p>
     *    Instance which is used to call the preprocessing methods before recognition
     *
     *    DEFAULT: NULL
     *    </p>
     */

     string m_activedtwCfgFilePath;
     /**< @brief Full path of ActiveDTW configuration file
     *    <p>
     *    Assigned value in the ActiveDTWShapeRecognizer::initialize function
     *    </p>
     */

     string m_activedtwMDTFilePath;
     /**< @brief Full path of Model data file
     *    <p>
     *    Assigned value in the ActiveDTWShapeRecognizer::initialize function
     *    </p>
     */

     stringStringMap m_headerInfo;
     /**< @brief Header Information
     *    <p>
     *    </p>
     */

     LTKShapeRecoUtil m_shapeRecUtil;
     /**< @brief Pointer to LTKShapeRecoUtil class
     *    <p>
     *    Instance which used to call Shape Recognizer Utility functions
     *
     *    DEFAULT: NULL
     */

     string m_lipiRootPath;
     /**< @brief Path of the Lipi Root
     *    <p>
     *    DEFAULT: LipiEngine::getLipiPath()
     *    </p>
     */

     string m_lipiLibPath;
     /**< @brief Path of the Lipi Libraries
     *    <p>
     *    DEFAULT: LipiEngine::getLipiLibPath()
     *    </p>
     */

     LTKShapeFeatureExtractor *m_ptrFeatureExtractor;
     /**< @brief Pointer to LTKShapeFeatureExtractor class
     *    <p>
     *    DEFAULT: NULL
     *    </p>
     */

     string m_preProcSeqn;
     /**< @brief Preprocessor Sequence
     *    <p>
     *    This string will holds what sequence the preprocessor methods to be executed
     *    </p>
     */

     LTKCaptureDevice m_captureDevice;

     /** Structure to store information required for recognition and adaptation **/
     struct NeighborInfo
     {
          int typeId;
          int sampleId;
          int classId;
          double distance;
     };
     /**< @brief Structure to store information required for recognition and adaptation
     *    <p>
     *    TypeId: Specifies whether it is a singleton (defined by 1) or a cluster (defined by 0)
     *      SampleId: Specifies which singleton or cluster sample. The range of values depend on
     *                the number of clusters or singletons (>= 0)
     *      ClassId: Specifies the class id
     *      Distance: The distance between the test sample and singleton or
     *                the distance between the test sample and optimal deformation (cluster case)
     *                The range of values is >= 0
     *
     *    DEFAULT: NULL
     *      Range:  Depends on Individual fields
     *    </p>
     */

     /**Vector to store the distances from each class to test sample**/
     vector <struct NeighborInfo> m_neighborInfoVec;
     /**< @brief Neighbor Information Vector
     *    <p>
     *    This vector contains the distance information between the test samples and
     *      prototype data. It is a vector of structures NeighborInfo
     *    </p>
     */

     /** contains all the prototype data from load Model data **/
     vector<ActiveDTWShapeModel> m_prototypeShapes;
     /**< @brief Prototype Shapes
     *    <p>
     *    This is a vector of ActiveDTWShapeModels
     *      This populated via the loadModelData function
     *    </p>
     */

     /** Sorts dtw distances **/
     static bool sortDist(const NeighborInfo& x, const NeighborInfo& y);
     /**
     * This method compares distances
     *
     * Semantics
     *
     *     - Compare distances
     *
     * @param NeighborInfo(X)
     * @param NeighborInfo(Y)
     *
     * @return TRUE:  if X.distance lesser than Y.distance
     *         FALSE: if X.distance greater than Y.distance
     * @exception none
     */

     vector<stringStringPair> m_preprocSequence;

     intIntMap m_shapeIDNumPrototypesMap;
     /**< @brief Map of shapeID and Number of Samples per shape
     *    <p>
     *
     *    </p>
     */

     int m_prototypeSetModifyCount;
     /**< @brief
     *    <p>
     *    Used to count number of modifications done to m_prototypeShapes.
     *   Write to MDT after m_prototypeModifyCntCFG such modifications or at Exit.
     *    </p>
     */

     int m_MDTUpdateFreq;
     /**< @brief Update MDT after a specified number of modifications to m_prototypeSet
     *    <p>
     *    Specified in ActiveDTW.cfg
     *
     *    </p>
     */

     shapeFeature m_cachedShapeFeature;
     /**< @brief Store shapeFeature of the last inTraceGroup to Recognize
     *   Used during subsequent call to Adapt
     *    <p>
     *
     *
     *    </p>
     */

     float m_rejectThreshold;
     /**< @brief Threshold on the confidence to reject a test sample
     *    <p>
     *
     *    </p>
     */

     bool m_adaptivekNN;
     /**< @brief Adaptive kNN method to compute confidence
     *    <p>
     *    If m_adaptivekNN = true, the adaptive kNN method is used for confidence computation
     *                 false, NN or kNN method is used, based on the value of m_nearestNeighbors
     *    </p>
     */

     string m_currentVersion;

     string m_MDTFileOpenMode;
     /**< @brief File modes of the mdt file
     *    <p>
     *    If m_adaptivekNN = ascii, the mdt file is written in ascii mode
     *                 binary, the mdt file will be written in binary mode
     *      Default: LTKPreprocDefaults::NN_MDT_OPEN_MODE_ASCII
     *    </p>
     */

     //dtw obj for computing the dtw distance between features
     DynamicTimeWarping<LTKShapeFeaturePtr, float> m_dtwObj;
     /**< @brief Dynamic Time Warping Object
     *    <p>
     *    This object aids in calculating the dtw distance between two LTKShapeFeaturePtrs
     *      and the distance is in float
     *    </p>
     */

     //store the recognitioresults
     vector<LTKShapeRecoResult> m_vecRecoResult;
     /**< @brief Vector of LTKShapeRecoResult
     *    <p>
     *    This vector is used to store the confidence values computed using computeConfidence
     *    </p>
     */

     //minimum numberOfSamples to form cluster


    public:

        /** @name Constructors and Destructor */

          /**
          * Constructor
          */
        ActiveDTWShapeRecognizer(const LTKControlInfo& controlInfo);

        /**
          * Destructor
          */
        ~ActiveDTWShapeRecognizer();

        //@}

        /**
          * This method initializes the ActiveDTW shape recognizer
          * <p>
          * Semantics
          *   - Set the project name in ActiveDTWShapeRecognizer::headerInfo with the parameter passed.<br>
          *         m_headerInfo[PROJNAME] = strProjectName;
          *
          *   -  Initialize ActiveDTWShapeRecognizer::m_activedtwCfgFilePath <br>
          *         m_activedtwCfgFilePath = ActiveDTWShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
          *                         strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
          *                         LTKInc::SEPARATOR + LTKInc::ActiveDTW + LTKInc::CONFIGFILEEXT;
          *
          *   -  Initializes ActiveDTWShapeRecognizer::m_activedtwMDTFilePath <br>
          *         m_activedtwMDTFilePath = ActiveDTWShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
          *                           strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
          *                           LTKInc::SEPARATOR + LTKInc::ActiveDTW + LTKInc::DATFILEEXT;
          *
          *   - Initializes ActiveDTWShapeRecognizer::m_projectTypeDynamic with the value returned from LTKShapeRecoUtil::isProjectDynamic
          *
          *   - Initialize the preprocessor using LTKShapeRecoUtil::initializePreprocessor and assign
          *      default values for
          *           -# Normalised size
          *           -# Threshold size
          *           -# Aspect ratio
          *           -# Dot threshold
          *
          *   - Initialize the recognizers instance variables with the values given in classifier config file.
          *
          * </p>
          * @param strProjectName : string : Holds the name of the Project
          * @param strProfileName : string : Holds the name of the Profile
          *
          * @return int : LTKInc::SUCCESS         if initialization done successfully
          *                   errorValues if initialization has some errors
          *
          * @exception LTKErrorList::ECONFIG_FILE_OPEN           Could not open project.cfg
          * @exception LTKErrorList::EINVALID_NUM_OF_SHAPES      Negative value for number of shapes
          * @exception LTKErrorList::ELOAD_PREPROC_DLL           Could not load preprocessor DLL
          * @exception LTKErrorList::EDLL_FUNC_ADDRESS_CREATE    Could not map createPreprocInst
          * @exception LTKErrorList::EDLL_FUNC_ADDRESS_DELETE    Could not map destroyPreprocInst
          */

        /**
          * This method calls the train method of the ActiveDTW classifier.
          *
          */
        int train(const string& trainingInputFilePath,
               const string& mdtHeaderFilePath,
               const string &comment,const string &dataset,
               const string &trainFileType=INK_FILE) ;

               /**
               * This method loads the Training Data of the ActiveDTW classifier.
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
          * @param deviceInfo The parameter to be set
          * @return
          * @exception
          */
        int setDeviceContext(const LTKCaptureDevice& deviceInfo);

        /**
          * Populates a vector of LTKShapeRecoResult consisting of top classes with their confidences.
          *
          * Semantics
          *
          *    - Validate the input arguments
          *    - Extract the features from traceGroup
          *      - Compute the optimal deformation vectors for each class
          *    - If the dtwEuclidean filter is on:
          *          --Then for Clusters
          *          -- Calculate the euclidean distances between the test samples and the optimal deformations
          *         Accordingly populate the distFilter vector
          *          --Then for singletons
          *          --Calculate the euclidean distances between the test samples and the singletonVectors
          *            Accordingly populate the distFilter vector
          *    - Compute the dtw distances between the test sample and the optimal deformations and singletonVectors
          -- If the dtwEuclidean filter is on then calculate the dtw distances only
          to those deformations and singletonVectors recommended by the dtw euclidean filter
          *      - Populate the distIndexPairVector with distances and shapeID
          *    - Sort the distIndexPairVector based on the distances in ascending order
          *    - Compute the confidences of the classes appearing in distIndexPairVector, call computeConfidence()
          *    - Check if the first element of resultVector has confidence less than m_rejectThreshold, if so,
          empty the resultVector (reject the sample), log and return.
          *    - If the confThreshold value was specified by the user (not equal to -1),
          delete the entries from resultVector with confidence values less than confThreshold.
          *    - If the numChoices value was specified by the user (not equal to -1),
          update the resultVector with top numChoices entries, delete the other values.
          *
          * @param traceGroup The co-ordinates of the shape which is to be recognized
          * @param screenContext Contains information about the input field like whether it is boxed input
          *                 or continuous writing
          * @param subSetOfClasses A subset of the entire class space which is to be used for
          *                        recognizing the input shape.
          * @param confThreshold Classes with confidence below this threshold are not returned,
          *              valid range of confThreshold: (0,1)
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
               const vector<int>& inSubSetOfClasses,
               float confThreshold,
               int  numChoices,
               vector<LTKShapeRecoResult>& outResultVector);

               /**
               * In case of:
               *          1. Singleton Vectors: This method converts them to traceGroups
               *          2. Clusters: This method converts the cluster means to traceGroup
               * Semantics
               *
               *    - Check if shapeID is valid, if not return error code
               *    - Check if project is Dynamic, if not return ErrorCode
               *    - Update PrototypeSet
               *    - Update MDTFile
               *
               * @param shapeID         : int : Holds shapeID
               * @param numberOfTraceGroups : int : Maximum number of Trace Groups to populate
               * @param outTraceGroups : vector<LTKTraceGroup> : TraceGroup
               *
               * @return SUCCESS: TraceGroup is populated successfully
               *         FAILURE: return ErrorCode
               * @exception none
          */
        int getTraceGroups(int shapeID, int numberOfTraceGroups, vector<LTKTraceGroup> &outTraceGroups);



    private:
     /**
     * This function is the train method using Clustering prototype selection technique.
     *
     *
     * Semantics
     *
     *    - Note the start time for time calculations.
     *
     *    - Create an instance of the feature extractor using ActiveDTWShapeRecognizer::initializeFeatureExtractorInstance() method
     *
     *    - Call train method depending on the inFileType
     *         - ActiveDTWShapeRecognizer::trainFromListFile() if inFileType = LTKMacros::INK_FILE
     *         - ActiveDTWShapeRecognizer::trainFromFeatureFile() if inFileType = LTKMacros ::FEATURE_FILE
     *
     *    - Update the headerInfo with algorithm version and name using ActiveDTWShapeRecognizer::updateHeaderWithAlgoInfo() method
     *
     *    - Calculate the checksum.
     *
     *    - Note the finish time for time calculations.
     *
     *
     * @param inputFilePath :string : Path of trainListFile / featureFile
     * @param strModelDataHeaderInfoFile : string : Holds the Header information of Model Data File
     * @param inFileType : string : Possible values ink / featureFile
     *
     * @return LTKInc::SUCCESS     : if the training done successfully
     * @return errorCode : if it contains some errors
          */
        int trainClustering(const string& trainingInputFilePath,
               const string& mdtHeaderFilePath,
               const string& trainFileType);


               /**
               * This method do the map between the module name and function names from the cfg file
               *
               * Semantics
               *
               *    - Read the Preprocess Sequence from the nn.cfg
               *
               *    - Split the sequence into tokens with delimiter LTKMacros::DELEMITER_SEQUENCE using LTKStringUtil::tokenizeString
               *
               *    - Split each token with delimiter LTKMacrosDELEMITER_FUNC using LTKStringUtil::tokenizeString
               *         to get the Module name and Function name
               *
               *    - Store the Module name and the Function name into a structure
               *
               *
               * @param none
               * @return LTKInc::SUCCESS          : if functions are successfully mapped,
               * @return errorCodes     : if contains any errors
               * @exception none
          */
        int mapPreprocFunctions();

        /**
          * This method will assign default values to the members
          *
          * Semantics
          *
          *    - Assign Default values to all the data members
          *
          *
          * @param  none
          *
          * @return none
          */
        void assignDefaultValues();

        /** Reads the ActiveDTW.cfg and initializes the instance variable of the classifier with the user defined
          * values.
          *
          * Semantics
          *
          *    - Open the activedtw.cfg using LTKConfigFileReader
          *
          *    - Incase of file open failure (activedtw.cfg), default values of the classifier parameters are used.
          *
          *    - The valid values of the classifier parameters are cached in to the class data members.
          *        LTKConfigFileReader::getConfigValue is used to get the value fora key defined in the config file
          *
          *    - Exception is thrown if the user has specified an invalid valid for a parameter
          *
          *
          * @param  none
          * @return SUCCESS   : If the Config file read successfully
          * @return errorCode : If it contains some errors
          * @exception LTKErrorList::ECONFIG_FILE_RANGE               The config file variable is not within the correct range
          */
        int readClassifierConfig();

        /**
          * This function serves as wrapper function to the Dtw distance computation function
          * (for use by clustering prototype selection)
          * @param train This is an input parameter and corresponds to the training character.
          * @param test This is an input parameter and corresponds to the testing character.
          */
        int computeDTWDistance(const LTKShapeSample& inFirstShapeSampleFeatures,
               const LTKShapeSample& inSecondShapeSampleFeatures,
               float& outDTWDistance);

               /**
               * computes the dtw distance between two shape feature vectors
          **/
          int computeDTWDistance(const vector<LTKShapeFeaturePtr>& inFirstFeatureVector,
               const vector<LTKShapeFeaturePtr>& inSecondFeatureVector,
               float& outDTWDistance);
               /**
               * This function serves as wrapper function to the Dtw distance computation function
               * (for use by recognize function)
               * @param train This is an input parameter and corresponds to the training character.
               * @param test This is an input parameter and corresponds to the testing character.
          */


        /**
          *    Computes the euclidean distance between two shape Features
          **/
          int computeEuclideanDistance(const shapeFeature& inFirstFeature,
               const shapeFeature& inSecondFeature,
               float& outEuclideanDistance);
               /**
               * This function is used to compute the Euclidean distance between two shape Features
               * (for use by recognize when dtwEuclidean filter is on)
               * @param train This is an input parameter and corresponds to the training character.
               * @param test This is an input parameter and corresponds to the testing character.
          */





        /**
          * This method creates a custom feature extractor instance and stores it's address in
          * ActiveDTWShapeRecognizer::m_ltkFE. The local distance function pointer is also initialized.
          *
          * Semantics
          *
          *
          *    - Intialize the ActiveDTWShapeRecognizer::m_ptrFeatureExtractor with address of the feature extractor instance created
          *        using LTKShapeFeatureExtractorFactory::createFeatureExtractor
          *
          *    - Cache the address of  LTKShapeFeatureExtractor::getLocalDistance() in an instance variable
          *
          * @param none
          *
          * @return 0 on LTKInc::SUCCESS and 1 on LTKInc::FAILURE
          *
          * @exception none
          */
        int initializeFeatureExtractorInstance(const LTKControlInfo& controlInfo);

        /**
          * This method trains the classifier from the train list file whose path is passed as paramater.
          *
          * Semantics
          *
          *    - Open the trainListFile for reading.
          *
          *    - Open the mdt file for writing.
          *
          *    - Write header information to the mdt file
          *         - ActiveDTWShapeRecognizer::m_numShapes
          *         - ActiveDTWShapeRecognizer::m_traceDimension
          *         - ActiveDTWShapeRecognizer::m_flexibilityIndex
          *
          *    - Get a valid line from the train list file
          *         - Skip commented lines
          *         - Skip lines where number_of_tokens != 2
          *         - Throw error LTKErrorList::EINITSHAPE_NONZERO, if the first shape in the list file is not zero
          *         - Throw error LTKErrorList::EINVALID_ORDER_LISTFILE if the shapes are not in sequential order
          *
          *    - For every valid line get the ShapeSample from the ink file using ActiveDTWShapeRecognizer::getShapeSampleFromInkFile
          *         - Read ink from UNIPEN  ink file
          *         - Skip if the trace group is empty
          *         - Pre process the trace group read from the ink file
          *         - Extract features
          *
          *    - Push all the ShapeSamples corresponding to a shape into a vector of ShapeSample ShapeSamplesVec.
          *
          *    - When all the ShapeSamples corresponding to a Shape have been collected, cluster them using ActiveDTWShapeRecognizer::performClustering
          *
          *    - performClustering results in vector of clustered ShapeSamples.
          *
          *      - computeCovarianceMatrix of clusters
          *
          *      - computeEigenVectorsForLargeDimension for the covariance matrices of the clusters
          *
          *      - construct shape models using cluster models and singletons
          *
          *    - Append these clustered vector<ActiveDTWShapeModel> to the mdt file.
          *
          *
          * @param listFilePath : string : Holds the path for train list file
          * @param trainSet : ShapeSampleVector: Holds the ShapeSample for all shapes, used for LVQ only
          *
          * @return none
          *
          * @exception LTKErrorList::EFILE_OPEN_ERROR       : Error in Opening a file (may be mdt file or list file)
          * @exception LTKErrorList::EINVALID_NUM_OF_SHAPES : Invalid value for number of shapes
          * @exception LTKErrorList::EINVALID_ORDER_LISTFILE: Invalid order of shapeId in List file
          * @exception LTKErrorList::EINITSHAPE_NONZERO          : Initial shapeId must not be zero
          * @exception LTKErrorList::EEMPTY_EIGENVECTORS         : Number of eigen vectors must be a positive number
          * @exception LTKErrorList::EINVALID_NUM_OF_EIGENVECTORS     : Number of eigen vector must be a positive number
          */

          int trainFromListFile(const string& listFilePath);

        /**
          * This method will get the ShapeSample by giving the ink file path as input
          *
          * Semantics
          *
          *    - Call the LTKShapeRecoUtil::readInkFromFile() method (Utility Method) to read the ink file
          *       By reading this file, an inTraceGroup was generated
          *
          *    - Preprocess the inTraceGroup and get the preprocessed trace group
          *       LTKTraceGroup preprocessedTraceGroup
          *
          *    - Extract features from the preprocessed trace group to get the ShapeSamples.
          *
          *
          * @param path       : string : The path for Ink file
          * @param ShapeSample : ShapeSample : The ShapeSample generated after feature extraction
          *
          * @return SUCCESS : If the ShapeSample was got successfully
          * @return FAILURE : Empty traces group detected for current shape
          *
          * @exception LTKErrorList::EINKFILE_EMPTY              : Ink file is empty
          * @exception LTKErrorList::EINK_FILE_OPEN              : Unable to open unipen ink file
          * @exception LTKErrorList::EINKFILE_CORRUPTED          : Incorrect or corrupted unipen ink file.
          * @exception LTKErrorList::EEMPTY_TRACE           : Number of points in the trace is zero
          * @exception LTKErrorList::EEMPTY_TRACE_GROUP          : Number of traces in the trace group is zero
          */
        int getShapeFeatureFromInkFile(const string& inkFilePath,
               vector<LTKShapeFeaturePtr>& shapeFeatureVec);


               /**
               * This method will do Custering for the given ShapeSamples
               *
               * Semantics
               *
               *     - If the ActiveDTWShapeRecognizer::m_prototypeReductionFactor is -1 means Automatic clustering could be done
               *
               *     - If the ActiveDTWShapeRecognizer::m_prototypeReductionFactor is 0 means No clustering was needed
               *
               *     - Otherwise clustering is needed based on the value of ActiveDTWShapeRecognizer::m_prototypeReductionFactor
               *
               *
               *
               * @param ShapeSamplesVec : ShapeSampleVector : Holds all the ShapeSample for a single class
               * @param resultVector          : int2DVector : Vector of indices of samples belonging to a cluster
               *
               * @return SUCCESS ; On successfully performing clustering
               * @return ErrorCode ; On some error
               * @exception none
          */
          int performClustering(const vector<LTKShapeSample>& shapeSamplesVec,
               int2DVector& outputVector);

               /**
               * This method computes the covariance matrix and the mean for a given feature matrix
               *
               * Semantics
               *
               *     - Computes the mean of the feature matrix
               *
               *     - Computes the mean corrected data
               *
               *     - Computes the covariance matrix
               *
               *
               *
               * @param featureMatrix : double2DVector : Holds all the features of a cluster
               * @param covarianceMatrix : double2DVector : covariance matrix of the cluster
               * @param meanFeature : doubleVector : mean feature of the cluster
               *
               * @return SUCCESS ; On successfully computing the covariance matrix
               * @return ErrorCode ; On some error
               * @exception LTKErrorList:: EEMPTY_FEATUREMATRIX, Feature matrix is empty
          */
          int computeCovarianceMatrix(double2DVector& featureMatrix,
               double2DVector& covarianceMatrix,doubleVector& meanFeature);

               /**
               * compute the eigen vectors
               * for a covariance Matrix
               * @inparam covarianceMatrix,rank,nrot --> no of rotations
               * @outparam eigenValueVec, eigenVectorMatrix
          **/
          int computeEigenVectors(double2DVector &covarianceMatrix,const int rank,
               doubleVector &eigenValueVec, double2DVector &eigenVectorMatrix, int& nrot);


               /**
               * This method will Update the Header information for the MDT file
               *
               * Semantics
               *
               *    - Copy the version number to a string
               *
               *    - Update the version info and algoName to ActiveDTWShapeRecognizer::m_headerInfo, which specifies the
               *       header information for MDT file
               *
               *
               * @param none
               *
               * @return none

                * @exception none
          */
        void updateHeaderWithAlgoInfo();

        int preprocess (const LTKTraceGroup& inTraceGroup, LTKTraceGroup& outPreprocessedTraceGroup);


          /**
          * This append the shape model data to the mdt file
          *
          * Semantics
          *
          *     - Append cluster models
          *
          *     - Append singleton vectors
          *
          *
          *
          * @param shapeModel : ActiveDTWShapeModel : Holds clusters and singletons of a shape
          * @param mdtFileHandle : ofstream : file handle for the mdt file
          *
          * @return SUCCESS ; On successfully appending the data to mdt file
          * @return ErrorCode:
          * @exception LTKErrorList:: EINVALID_FILE_HANDLE, unable to open mdt file
          */
          int appendShapeModelToMDTFile(const ActiveDTWShapeModel& shapeModel,ofstream& mdtFileHandle);

          /**
          *  find optimal deformation parameters using bound constrained optimization
        *  here any third part library can be called
        *  we will solve the equation
        *  Min f(x)
          *  Subject to:  lb <= x <= ub
          *  where lb -- lower bound  ub --- upper bound
        **/

          /**
          * This solves the optimization problem and finds the deformation parameters
          * It constructs the optimal deformation, the sample in the cluster closest to the test sample
          * Semantics
          *
          *     - Solve the optimization problem
          *        Min f(x)
          *          Subject to:  lb <= x <= ub
          *          where lb -- lower bound  ub --- upper bound
          *
          *
          *
          * @param eigenValues : doubleVector : eigen values of a cluster
          * @param eigenVector : double2DVector : eigen vectorsof a cluster
          * @param clusterMean : doubleVector : mean of the cluster
          * @param testSample : doubleVector : test sample
          * @param deformationParameters : doubleVector : parameters required to construct the optimal deformation
          *
          * @return SUCCESS ; On successfully appending the data to mdt file
          * @return ErrorCode:
          * @exception LTKErrorList:: EEMPTY_EIGENVALUES, eigen values are empty
          * @exception LTKErrorList:: EEMPTY_EIGENVECTORS, eigen vectors are empty
          * @exception LTKErrorList:: EEMPTY_CLUSTERMEAN, cluster mean is empty
          * @exception LTKErrorList:: ENUM_EIGVALUES_NOTEQUALTO_NUM_EIGVECTORS, number of eigen value is not equal to the number of eigen vectors
          * @exception LTKErrorList:: EEMPTY_EIGENVECTORS, eigen vectors are empty
          */
          int findOptimalDeformation(doubleVector& deformationParameters,doubleVector& eigenValues, double2DVector& eigenVector,
               doubleVector& clusterMean, doubleVector& testSample);

          static void getDistance(const LTKShapeFeaturePtr& f1,const LTKShapeFeaturePtr& f2, float& distance);


          /**
          * This method computes the confidences of test sample belonging to various classes
          *
          * Semantics
          *
          *    - Compute the confidence based on the values of m_nearestNeighbors and m_adaptiveKNN
          *    - Populate the resultVector
          *    - Sort the resultVector
          *    -
          *
          * @param distIndexPairVector : vector<struct NeighborInfo>: Holds the samples, classIDs and distances to the test sample
          * @param resultVector    : vector<LTKShapeRecoResult> : Holds the classIDs and the respective confidences
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
          *    - Check if the first object's confidence value is greater than the second object's confidence value
          *    - Return true or false
          *    -
          *
          * @param x : LTKShapeRecoResult : First object for comparison
          * @param y : LTKShapeRecoResult : Second object for comparison
          *
          * @return true: If x.confidence > y.confidence
          *         false: If x.confidence <= y.confidence
          * @exception none
          */
          static bool sortResultByConfidence(const LTKShapeRecoResult& x, const LTKShapeRecoResult& y);

          static bool compareMap( const map<int, int>::value_type& lhs, const map<int, int>::value_type& rhs );

        int mapFeatureExtractor();

        int deleteFeatureExtractorInstance();
        /**
          * This method extracts shape features from given TraceGroup
          *
          * Semantics
          *
          *    - PreProcess tracegroup
          *    - Extract Features
          *
          * @param inTraceGroup    : LTKTraceGroup : Holds TraceGroup of sample
          *
          * @return SUCCESS:  if shapeFeatures is populated successfully
          *         FAILURE:  return ErrorCode
          * @exception none
          */

        int extractFeatVecFromTraceGroup(const LTKTraceGroup& traceGroup,
               vector<LTKShapeFeaturePtr>& featureVec);


               /** This method is used to initialize the PreProcessor
               *
               * Semantics
               *
               *    - Load the preprocessor DLL using LTKLoadDLL().
               *
               *    - Get the proc address for creating and deleting the preprocessor instance.
               *
               *    - Create preprocessor instance.
               *
               *    - Start the logging for the preprocessor module.
               *
               * @param  preprocDLLPath : string : Holds the Path of the Preprocessor DLL,
               * @param  errorStatus    : int    : Holds SUCCESS or Error Values, if occurs
               * @return preprocessor instance
               *
               * @exception   ELOAD_PREPROC_DLL             Could not load preprocessor DLL
               * @exception   EDLL_FUNC_ADDRESS_CREATE Could not map createPreprocInst
               * @exception   EDLL_FUNC_ADDRESS_DELETE Could not map destroyPreprocInst
          */
        int initializePreprocessor(const LTKControlInfo& controlInfo,
               LTKPreprocessorInterface** preprocInstance);

               /** This method is used to deletes the PreProcessor instance
               *
               * Semantics
               *
               *    - Call deleteLTKPreprocInst from the preproc.dll.
               *
               *    - Unload the preprocessor DLL.
               *
               * @param  ptrPreprocInstance : Holds the pointer to the LTKPreprocessorInterface
               * @return none
               * @exception none
          */

        int deletePreprocessor();

        /** This method is used to Unloads the preprocessor DLL.
          *
          * Semantics
          *
          *    - If m_libHandler != NULL, unload the DLL
          *         LTKUnloadDLL(m_libHandler);
          *         m_libHandler = NULL;
          *
          * @param none
          * @return none
          * @exception none
          */
        int unloadPreprocessorDLL();

          /**< @brief Pointer to LTKOSUtil interface
          *    <p>
          *
          *    </p>
          */
        LTKOSUtil*  m_OSUtilPtr;

          int validatePreprocParameters(stringStringMap& headerSequence);

          /**
          * Computes the eigen values and eigen vectors of the larger covariance matrix using the
          * a smaller covariance matrix
          *
          *    Semantics
          *
          *     - Compute the smaller covariance matrix, using meanCorrectedData(Transpose)*meanCorrectedData(Transpose)
          *
          *     - Compute the eigen vectors of the smaller covariance matrix
          *
          *     - Determine the number of eigen vectors, depending on the eigen energy to be retained
          *
          *     - Compute the eigen vectors of the larger covariance matrix
          *
          *     - Normalizing the eigen vectors
          *
          *
          *
          * @param meanCorrectedData : double2DVector : mean corrected data
          * @param covarianceMatrix : double2DVector : covariance matrix of the corresponding mean corrected data
          * @param eigenValues : doubleVector : output selected eigen values
          * @param eigenVector : double2DVectorr : output selected eigen vectors
          *
          * @return SUCCESS ; On successfully computing eigen values and eigen vectors
          * @return ErrorCode:
          * @exception LTKErrorList:: EEMPTY_MEANCORRECTEDDATA, empty mean corrected data
          * @exception LTKErrorList:: EEMPTY_COVARIANCEMATRIX, empty covariance matrix
          */
          int computeEigenVectorsForLargeDimension(double2DVector& meanCorrectedData,double2DVector& covarianceMatrix,
               double2DVector& eigenVector,doubleVector& eigenValues);

               /**
               * This converts the double vector to a feature vector
               * It constructs the optimal deformation, the sample in the cluster closest to the test sample
               *
               *
               * @param featureVec : doubleVector : input double feature vector
               * @param shapeFeatureVec : vector<LTkShapeFeaturePtr> : output feature vector
               *
               * @return SUCCESS ; On successfully conversion
               * @return ErrorCode:
               * @exception LTKErrorList:: EINVALID_INPUT_FORMAT
          */
          int convertDoubleToFeatureVector(vector<LTKShapeFeaturePtr>& shapeFeatureVec,doubleVector& featureVec);


};

#endif
