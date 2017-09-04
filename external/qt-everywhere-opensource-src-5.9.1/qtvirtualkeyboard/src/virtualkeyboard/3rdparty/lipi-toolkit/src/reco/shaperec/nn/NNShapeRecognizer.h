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
 * $LastChangedDate: 2011-08-23 13:31:44 +0530 (Tue, 23 Aug 2011) $
 * $Revision: 865 $
 * $Author: jitender $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for NN Shape Recognition module
 *
 * CONTENTS:
 *
 * AUTHOR:     Saravanan R.
 *
 * DATE:       January 23, 2007
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/

#ifndef __NNSHAPERECOGNIZER_H
#define __NNSHAPERECOGNIZER_H

/**            Include files       */
#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKTrace.h"
#include "LTKMacros.h"
#include "LTKShapeRecognizer.h"
#include "LTKShapeRecoUtil.h"
#include "LTKShapeSample.h"
#include "LTKCheckSumGenerate.h"
#include "LTKDynamicTimeWarping.h"

/**            Forward declaration of classes          */
class LTKTraceGroup;
class LTKPreprocessorInterface;
class LTKShapeSample;
class LTKShapeFeatureExtractor;
class LTKShapeFeature;

#define SIMILARITY(distance) (1 / (distance + EPS ))
#define SUPPORTED_MIN_VERSION "3.0.0"

class LTKAdapt;

typedef int (*FN_PTR_LOCAL_DISTANCE)(LTKShapeFeaturePtr, LTKShapeFeaturePtr,float&);
typedef int (*FN_PTR_CREATELTKLIPIPREPROCESSOR)(const LTKControlInfo& , LTKPreprocessorInterface** );
typedef int (*FN_PTR_DELETELTKLIPIPREPROCESSOR)(LTKPreprocessorInterface* );


/**
 * @ingroup NNShapeRecognizer.h
 * @brief The Header file for the NNShapeRecognizer
 * @class NNShapeRecognizer
 *<p> <p>
 */

class NNShapeRecognizer: public LTKShapeRecognizer
{

    public:
        friend class LTKAdapt;
        int adapt(int shapeID );
        int adapt(const LTKTraceGroup& sampleTraceGroup, int shapeID );

    private:

        int deleteAdaptInstance();


        /** @name private data members */
        //@{
    private:

        FN_PTR_DELETELTKLIPIPREPROCESSOR m_deleteLTKLipiPreProcessor;
        //Function pointer for deleteLTKLipiPreProcessor

        // preproc lin handle
        void *m_libHandler;

        // feature extractor lin handle
        void *m_libHandlerFE;

        unsigned short m_numShapes;
        /**< @brief Number of shapes
         *     <p>
         *     It Defines the number of shapes to be recognized
         *
         *     DEFAULT: 0
         *
         *     Note: If the project is dynamic, then this numShapes was set to 0
         *            If the project is not dynamic, then the numShapes was read from project configuration file
         *     </p>
         */

        string m_prototypeSelection;
        /**< @brief The Prototype Selection
         *     <p>
         *     if Prototype Selection = clustering, the training method used was clustering
         *                             = lvq,           the training method used was LVQ
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_PROTOTYPESELECTION
         *     Possible values are "clustering" and "lvq"
         *     </p>
         */

        int m_prototypeReductionFactor;
        /**< @brief The prototype Reduction factor
         *     <p>
         *     if PrototypeReductionFactor   = 0 every training sample is cluster on its own
         *                                             = 100 all training samples are represented by one prototype
         *                                        = 80 then all samples are represented by 20% of the training samples
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_PROTOTYPEREDUCTIONFACTOR
         *     RANGE:    0 TO 100
         *     </p>
         */

        int m_numClusters;
        /**< @brief The number of clusters
         *     <p>
         *     if NumClusters = k, then k clusters are found from the training samples
         *
         *
         *
         *     DEFAULT: There is no default as this and prototype reduction factor are dependent
         *     RANGE:
         *     </p>
         */


        string m_prototypeDistance;
        /**< @brief The Prototype Distance
         * <p>
         * if PrototypeDistance  =  eu, then the distance between the samples can be calculated using the Euclidean distance method
         *                       =  dtw, then the distance between the samples can be calculated using the DTW method
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_PROTOTYPEDISTANCE
         *     Possible values are LTKMacros::DTW_DISTANCE and LTKMacros::EUCLIDEAN_DISTANCE.
         * </p>
         */

        int m_nearestNeighbors;
        /**< @brief Nearest Neighbors
         *     <p>
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_NEARESTNEIGHBORS
         *     </p>
         */


//        int m_dtwBanding;
        float m_dtwBanding;
        /**< @brief  DTW Banding
         *     <p>
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_BANDING
         *     </p>
         */

        int m_dtwEuclideanFilter;
        /**< @brief DTW Euclidean Filter
         *     <p>
         *
         *     DEFAULT: LTKPreprocDefaults::NN_DEF_DTWEUCLIDEANFILTER
         *     </p>
         */

        string m_featureExtractorName;
        /**< @brief The Feature Extractor
         * <p>
         *
         *     DEFAULT:
         *
         * </p>
         */

        bool m_projectTypeDynamic;
        /**< @brief Project Dynamic
         *     <p>
         *     if projectTypeDynamic    = true, then the project is dynamic ie, the numShapes can take any number of value
         *                    = false, then the project is not dynamic ie, the numShape can take value specified in project.cfg file
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

        string m_nnCfgFilePath;
        /**< @brief Full path of NN configuration file
         *     <p>
         *     Assigned value in the NNShapeRecognizer::initialize function
         *     </p>
         */

        string m_nnMDTFilePath;
        /**< @brief Full path of Model data file
         *     <p>
         *     Assigned value in the NNShapeRecognizer::initialize function
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
         *     Instance which used to call Shape Recognizer Utility functions
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
         *     DEFAULT: LipiEngine::getLipiLibPath()
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
         *     This string will holds what sequence the preprocessor methods to be executed
         *     </p>
         */

        vector<LTKShapeSample> m_prototypeSet;
        /**< @brief Prototype Set for LVQ
         *     <p>
         *     It contains the Vector of Clustered ShapeSamples
         *     </p>
         */

        LTKCaptureDevice m_captureDevice;

        struct NeighborInfo
        {
            int classId;
            float distance;
            int prototypeSetIndex;
        };

        /*
           struct MapModFunc
           {
           string moduleName;
           string funcName;
           };
         */

        vector<stringStringPair> m_preprocSequence;

        intIntMap m_shapeIDNumPrototypesMap;
        /**< @brief Map of shapeID and Number of Samples per shape
         *     <p>
         *
         *     </p>
         */

        int m_prototypeSetModifyCount;
        /**< @brief
         *     <p>
         *     Used to count number of modifications done to m_prototypeSet.
         *   Write to MDT after m_prototypeModifyCntCFG such modifications or at Exit.
         *     </p>
         */

        int m_MDTUpdateFreq;
        /**< @brief Update MDT after a specified number of modifications to m_prototypeSet
         *     <p>
         *     Specified in NN.cfg
         *
         *     </p>
         */

        //Cache Parameters used by Adapt
        vector<LTKShapeRecoResult> m_vecRecoResult;
        /**< @brief Store Recognize results
         *      used by subsequent call to Adapt
         *     <p>
         *
         *
         *     </p>
         */


        vector <struct NeighborInfo> m_neighborInfoVec;
        /**< @brief Vector to store the distances of test sample to each of the samples in prototypeSet,
         *    classIDs and indices within the prototypeset
         *   Used during subsequent call to Adapt
         *     <p>
         *
         *
         *     </p>
         */

        LTKShapeSample m_cachedShapeSampleFeatures;
        /**< @brief Store ShapeSampleFeatures of the last inTraceGroup to Recognize
         *   Used during subsequent call to Adapt
         *     <p>
         *
         *
         *     </p>
         */

        float m_rejectThreshold;
        /**< @brief Threshold on the confidence to reject a test sample
         *     <p>
         *
         *     </p>
         */

        bool m_adaptivekNN;
        /**< @brief Adaptive kNN method to compute confidence
         *     <p>
         *     If m_adaptivekNN = true, the adaptive kNN method is used for confidence computation
         *                  false, NN or kNN method is used, based on the value of m_nearestNeighbors
         *     </p>
         */

        //@}

        string m_currentVersion;

        string m_MDTFileOpenMode;

     DynamicTimeWarping<LTKShapeFeaturePtr, float> m_dtwObj;

    public:

        /** @name Constructors and Destructor */
        //@{

        NNShapeRecognizer(const LTKControlInfo& controlInfo);

        /**
         * Destructor
         */
        ~NNShapeRecognizer();

        //@}

        /**
         * This method initializes the NN shape recognizer
         * <p>
         * Semantics
         *   - Set the project name in NNShapeRecognizer::headerInfo with the parameter passed.<br>
         *          m_headerInfo[PROJNAME] = strProjectName;
         *
         *   -  Initialize NNShapeRecognizer::m_nnCfgFilePath <br>
         *          m_nnCfgFilePath = NNShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
         *                          strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
         *                          LTKInc::SEPARATOR + LTKInc::NN + LTKInc::CONFIGFILEEXT;
         *
         *   -  Initializes NNShapeRecognizer::m_nnMDTFilePath <br>
         *          m_nnMDTFilePath = NNShapeRecognizer::m_lipiRootPath + LTKMacros::PROJECTS_PATH_STRING +
         *                            strProjectName + LTKMacros::PROFILE_PATH_STRING + strProfileName +
         *                            LTKInc::SEPARATOR + LTKInc::NN + LTKInc::DATFILEEXT;
         *
         *   - Initializes NNShapeRecognizer::m_projectTypeDynamic with the value returned from LTKShapeRecoUtil::isProjectDynamic
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
         * This method calls the train method of the NN classifier.
         *
         */
        int train(const string& trainingInputFilePath,
                const string& mdtHeaderFilePath,
                const string &comment,const string &dataset,
                const string &trainFileType=INK_FILE) ;

        /**
         * This method loads the Training Data of the NN classifier.
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
         *     - Validate the input arguments
         *     - Extract the features from traceGroup
         *     - If the distance method (m_prototypeDistance) is Euclidean (EUCLIDEAN_DISTANCE),
         *          populate the distIndexPairVector with the distance of the test sample to all the
         samples in the prototype set
         *     - If the distance method is DTW, compute the DTW distance of the test sample to the
         samples in the prototype set which passed the Euclidean filter. Populate the
         distIndexPairVector
         *     - Sort the distIndexPairVector based on the distances in ascending order
         *     - Compute the confidences of the classes appearing in distIndexPairVector, call computeConfidence()
         *     - Check if the first element of resultVector has confidence less than m_rejectThreshold, if so,
         empty the resultVector (reject the sample), log and return.
         *     - If the confThreshold value was specified by the user (not equal to -1),
         delete the entries from resultVector with confidence values less than confThreshold.
         *     - If the numChoices value was specified by the user (not equal to -1),
         update the resultVector with top numChoices entries, delete the other values.
         *
         * @param traceGroup The co-ordinates of the shape which is to be recognized
         * @param screenContext Contains information about the input field like whether it is boxed input
         *                 or continuous writing
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
         *     - Check if project is Dynamic, if not return ErrorCode
         *     - Calculate Features
         *     - Update PrototypeSet
         *     - Update MDTFile
         *     - return shapeID of new class added
         *
         * @param sampleTraceGroup : LTKTraceGroup : Holds TraceGroup of sample to Add
         * @param shapeID          : int : Holds shapeID of new Class
         *                         shapeID = m_prototypeSet.at(prototypeSetSize-1).getClassID()+1
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
          *    - Check if project is Dynamic, if not return ErrorCode
          *    - Calculate Features
          *    - Update PrototypeSet
          *    - Update MDTFile
          *    - return shapeID of new class added
          *
          * @param sampleTraceGroup : LTKTraceGroup : Holds TraceGroup of sample to Add
          * @param shapeID         : int : Holds shapeID of new Class
          *                         shapeID = m_prototypeSet.at(prototypeSetSize-1).getClassID()+1
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
         *     - Check if shapeID is valid, if not return error code
         *     - Check if project is Dynamic, if not return ErrorCode
         *     - Update PrototypeSet
         *     - Update MDTFile
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
         *     - Check if shapeID is valid, if not return error code
         *     - Check if project is Dynamic, if not return ErrorCode
         *     - Update PrototypeSet
         *     - Update MDTFile
         *
         * @param shapeID          : int : Holds shapeID
         * @param numberOfTraceGroups : int : Maximum number of Trace Groups to populate
         * @param outTraceGroups : vector<LTKTraceGroup> : TraceGroup
         *
         * @return SUCCESS: TraceGroup is populated successfully
         *         FAILURE: return ErrorCode
         * @exception none
         */
        int getTraceGroups(int shapeID, int numberOfTraceGroups, vector<LTKTraceGroup> &outTraceGroups);

          /**
         * This function does the recognition function required for training phase (called from trainLVQ)
         * The input parameter are the incharacter, which is compared with the existing
         * set of prototypes and then the matched code vector and along with its index (and also the shape id) is returned
         * @param incharacter is the character which we are trying to recognise.
         * @param returnshapeID is the value of the matched character which is returned, codeCharacter is the matched prototype (code vector) vector, and codeVecIndex is the matched prototype (code vector) index
         */
        int trainRecognize(LTKShapeSample& inShapeSample, LTKShapeSample& bestShapeSample, int& codeVecIndex);

    private:
        /**
         * This function is the train method using Clustering prototype selection technique.
         *
         *
         * Semantics
         *
         *     - Note the start time for time calculations.
         *
         *     - Create an instance of the feature extractor using NNShapeRecognizer::initializeFeatureExtractorInstance() method
         *
         *     - Call train method depending on the inFileType
         *          - NNShapeRecognizer::trainFromListFile() if inFileType = LTKMacros::INK_FILE
         *          - NNShapeRecognizer::trainFromFeatureFile() if inFileType = LTKMacros ::FEATURE_FILE
         *
         *     - Update the headerInfo with algorithm version and name using NNShapeRecognizer::updateHeaderWithAlgoInfo() method
         *
         *     - Calculate the checksum.
         *
         *     - Note the finish time for time calculations.
         *
         *
         * @param inputFilePath :string : Path of trainListFile / featureFile
         * @param strModelDataHeaderInfoFile : string : Holds the Header information of Model Data File
         * @param inFileType : string : Possible values ink / featureFile
         *
         * @return LTKInc::SUCCESS : if the training done successfully
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
         *     - Read the Preprocess Sequence from the nn.cfg
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
         * @return errorCodes : if contains any errors
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

        /** Reads the NN.cfg and initializes the instance variable of the classifier with the user defined
         * values.
         *
         * Semantics
         *
         *     - Open the nn.cfg using LTKConfigFileReader
         *
         *     - Incase of file open failure (nn.cfg), default values of the classifier parameters are used.
         *
         *     - The valid values of the classifier parameters are cached in to the class data members.
         *        LTKConfigFileReader::getConfigValue is used to get the value fora key defined in the config file
         *
         *     - Exception is thrown if the user has specified an invalid valid for a parameter
         *
         *
         * @param  none
         * @return SUCCESS   : If the Config file read successfully
         * @return errorCode : If it contains some errors
         * @exception LTKErrorList::ECONFIG_FILE_RANGE           The config file variable is not within the correct range
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
         * This function computes the eucildean distance between the two points.
         * @param train X and Y coordinate of the first point.
         * @param test X and Y coordinate of the second point.
         */

        /*int computeEuclideanDistance(const LTKShapeSample& inFirstShapeSampleFeatures,
                const LTKShapeSample& inSecondShapeSampleFeatures,
                float& outEuclideanDistance);*/

         int computeEuclideanDistance(const LTKShapeSample& inFirstShapeSampleFeatures,
                                     const LTKShapeSample& inSecondShapeSampleFeatures,
                                     float& outEuclideanDistance);

     int calculateMedian(const int2DVector& clusteringResult,
          const float2DVector& distanceMatrix, vector<int>& outMedianIndexVec);



        /**
         * This method creates a custom feature extractor instance and stores it's address in
         * NNShapeRecognizer::m_ltkFE. The local distance function pointer is also initialized.
         *
         * Semantics
         *
         *
         *     - Intialize the NNShapeRecognizer::m_ptrFeatureExtractor with address of the feature extractor instance created
         *        using LTKShapeFeatureExtractorFactory::createFeatureExtractor
         *
         *     - Cache the address of  LTKShapeFeatureExtractor::getLocalDistance() in an instance variable
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
         *     - Open the trainListFile for reading.
         *
         *     - Open the mdt file for writing.
         *
         *     - Write header information to the mdt file
         *          - NNShapeRecognizer::m_numShapes
         *          - NNShapeRecognizer::m_traceDimension
         *          - NNShapeRecognizer::m_flexibilityIndex
         *
         *     - Get a valid line from the train list file
         *          - Skip commented lines
         *          - Skip lines where number_of_tokens != 2
         *          - Throw error LTKErrorList::EINITSHAPE_NONZERO, if the first shape in the list file is not zero
         *          - Throw error LTKErrorList::EINVALID_ORDER_LISTFILE if the shapes are not in sequential order
         *
         *     - For every valid line get the ShapeSample from the ink file using NNShapeRecognizer::getShapeSampleFromInkFile
         *          - Read ink from UNIPEN  ink file
         *          - Skip if the trace group is empty
         *          - Pre process the trace group read from the ink file
         *          - Extract features
         *
         *     - Push all the ShapeSamples corresponding to a shape into a vector of ShapeSample ShapeSamplesVec.
         *
         *     - When all the ShapeSamples corresponding to a Shape have been collected, cluster them using NNShapeRecognizer::performClustering
         *
         *     - performClustering results in vector of clustered ShapeSamples.
         *
         *     - Append these clustered vector<ShapeSample> to the mdt file.
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

        int PreprocParametersForFeatureFile(stringStringMap& headerSequence);

        /**
         * This method will get the ShapeSample by giving the ink file path as input
         *
         * Semantics
         *
         *     - Call the LTKShapeRecoUtil::readInkFromFile() method (Utility Method) to read the ink file
         *        By reading this file, an inTraceGroup was generated
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
         * @return SUCCESS : If the ShapeSample was got successfully
         * @return FAILURE : Empty traces group detected for current shape
         *
         * @exception LTKErrorList::EINKFILE_EMPTY               : Ink file is empty
         * @exception LTKErrorList::EINK_FILE_OPEN               : Unable to open unipen ink file
         * @exception LTKErrorList::EINKFILE_CORRUPTED      : Incorrect or corrupted unipen ink file.
         * @exception LTKErrorList::EEMPTY_TRACE            : Number of points in the trace is zero
         * @exception LTKErrorList::EEMPTY_TRACE_GROUP      : Number of traces in the trace group is zero
         */
        int getShapeFeatureFromInkFile(const string& inkFilePath,
                vector<LTKShapeFeaturePtr>& shapeFeatureVec);

        /**
         * This method will do Custering for the given ShapeSamples
         *
         * Semantics
         *
         *     - If the NNShapeRecognizer::m_prototypeReductionFactor is -1 means Automatic clustering could be done
         *
         *     - If the NNShapeRecognizer::m_prototypeReductionFactor is 0 means No clustering was needed
         *
         *     - Otherwise clustering is needed based on the value of NNShapeRecognizer::m_prototypeReductionFactor
         *
         *     - Calculate Median if NNShapeRecognizer::m_prototypeReductionFactor is not equal to zero
         *
         *
         * @param ShapeSamplesVec : ShapeSampleVector : Holds all the ShapeSample for a single class
         * @param resultVector          : ShapeSampleVector : Holds all the ShapeSample after clustering
         * @param sampleCount : int                     : Holds the number of shapes for a sample
         *
         * @return none
         * @exception none
         */
        int performClustering(const vector<LTKShapeSample> & shapeSamplesVec,
                vector<LTKShapeSample>& outClusteredShapeSampleVec);

        /**
         * This method will Update the Header information for the MDT file
         *
         * Semantics
         *
         *     - Copy the version number to a string
         *
         *     - Update the version info and algoName to NNShapeRecognizer::m_headerInfo, which specifies the
         *        header information for MDT file
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
         * This method will writes training results to the mdt file
         *
         * Semantics
         *
         *     - If the feature representation was float then
         *          - Iterate through the shape model
         *          - Write the feature Dimension
         *          - Write the feature vector size
         *          - Write all the feature vector
         *          - Write the class ID
         *
         *     - If the feature representation was custom then
         *          - Iterate through the shape model
         *          - Write the feature Size
         *          - Call the writeFeatureVector() to write all the feature vector
         *          - Write the class ID
         *
         *
         * @param resultVector          : ShapeSampleVector : A vector of ShapeSamples created as a result of training
         *           mdtFileHandle      : ofstream            : Specifies the outut stream
         *
         * @return none
         *
         * @exception none
         */

        int appendPrototypesToMDTFile(const vector<LTKShapeSample>& prototypeVec, ofstream & mdtFileHandle);

        static bool sortDist(const NeighborInfo& x, const NeighborInfo& y);

        static void getDistance(const LTKShapeFeaturePtr& f1,const LTKShapeFeaturePtr& f2, float& distance);

        int getShapeSampleFromString(const string& inString, LTKShapeSample& outShapeSample);

        int mapFeatureExtractor();

        int deleteFeatureExtractorInstance();
        /**
         * This method extracts shape features from given TraceGroup
         *
         * Semantics
         *
         *     - PreProcess tracegroup
         *     - Extract Features
         *
         * @param inTraceGroup     : LTKTraceGroup : Holds TraceGroup of sample
         *
         * @return SUCCESS:  if shapeFeatures is populated successfully
         *         FAILURE:  return ErrorCode
         * @exception none
         */

        int extractFeatVecFromTraceGroup(const LTKTraceGroup& traceGroup,
                vector<LTKShapeFeaturePtr>& featureVec);

        /**
         * This method create MDTFile
         *
         * Semantics
         *
         *
         * @param None
         *
         * @return None
         *
         * @exception none
         */
        int writePrototypeSetToMDTFile();

        /**
         * This method adds Sample To Prototype
         *
         * Semantics
         *
         *     - Add data in ascending order to ShapeID
         *     -
         *
         * @param shapeSampleFeatures   : LTKShapeSample : Holds features of sample to be added to PrototypeSet
         *
         * @return SUCCESS:  if shapeSampleFeatures is populated successfully
         *         FAILURE:  return ErrorCode
         * @exception none
         */
        int insertSampleToPrototypeSet(const LTKShapeSample &shapeSampleFeatures);

        /**
         * This method computes the confidences of test sample belonging to various classes
         *
         * Semantics
         *
         *     - Compute the confidence based on the values of m_nearestNeighbors and m_adaptiveKNN
         *     - Populate the resultVector
         *     - Sort the resultVector
         *     -
         *
         * @param distIndexPairVector : vector<struct NeighborInfo>: Holds the samples, classIDs and distances to the test sample
         * @param resultVector     : vector<LTKShapeRecoResult> : Holds the classIDs and the respective confidences
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
         *     - Check if the first object's confidence value is greater than the second object's confidence value
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

        /**
         * The comparison function object of STL's max_element() method, overloaded for the map<int, int>, used to retrieve the maximum of the value field (second element) of map
         *
         * Semantics
         *
         *     - Check if the first object's second value is greater than the second object's second value
         *     - Return true or false
         *     -
         *
         * @param lhs : map<int, int>::value_type : First object for comparison
         * @param rhs : map<int, int>::value_type : Second object for comparison
         *
         * @return true: If lhs.second > rhs.second
         *         false: If lhs.second <= rhs.second
         * @exception none
         */

        static bool compareMap( const map<int, int>::value_type& lhs, const map<int, int>::value_type& rhs );

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


        /** This method is used to deletes the PreProcessor instance
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

        /** This method is used to Unloads the preprocessor DLL.
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

          /**
         * This function is the train method using LVQ
         *
         * Semantics
         *
         *     - Note the start time for time calculations.
         *
         *     - Create an instance of the feature extractor using NNShapeRecognizer::initializeFeatureExtractorInstance() method
         *
         *     - Call train method depending on the inFileType
         *          -    NNShapeRecognizer::trainFromListFile if inFileType() = LTKMacros::INK_FILE
         *          -    tNNShapeRecognizer::rainFromFeatureFile if inFileType() = LTKMacros::FEATURE_FILE
         *
         *     NOTE :
         *          The NNShapeRecognizer::trainFromListFile populates the following data structures
         *
         *          -    NNShapeRecognizer::m_prototypeSet : Vector of clustered ShapeSample and
         *
         *     - Process the prototype set using NNShapeRecognizer::processPrototypeSetForLVQ()
         *
         *     - Update the headerInfo with algorithm version and name using NNShapeRecognizer::updateHeaderWithAlgoInfo()
         *
         *     - Calculate the checksum.
         *
         *     - Note the finish time for time calculations.
         *
         *
         * @param inputFilePath :string : Path of trainListFile / featureFile
         * @param strModelDataHeaderInfoFile : string : Holds the Header information of Model Data File
         * @param inFileType : string : Possible values ink / featureFile
         *
         * @return LTKInc::SUCCESS : if the training done successfully
         * @return errorCode : if it contains some errors
         */
        int trainLVQ(const string& inputFilePath,
                const string& mdtHeaderFilePath,
                const string& inFileType);

          /**
         * This function is used to compute the learning parameter that is used in Learning Vector Quantization  (called from trainLVQ)
         * The input parameters are the iteration number, number of iterations, and the start value of the learning parameter (alpha)
         * the function returns the value of the learning parameter (linearly decreasing)
         */
        float linearAlpha(long iter, long length, double& initialAlpha,
                double lastAlpha,int correctDecision);

        /**
         * This function does the reshaping of prototype vector (called from trainLVQ)
         * The input parameters are the code vector, data vector (learning example in the context of LVQ), and alpha (learning parameter)
         * @param bestcodeVec is the character which we are trying to morph
         * the function modifies the character bestcodeVec
         */
        int morphVector(const LTKShapeSample& dataShapeSample,
                double talpha, LTKShapeSample& bestShapeSample);

        int processPrototypeSetForLVQ();

        int validatePreprocParameters(stringStringMap& headerSequence);


           /**< @brief LVQ Iteration Scale
         *     <p>
         *
         *     DEFAULT:  LTKPreprocDefaults::NN_DEF_LVQITERATIONSCALE
         *     </p>
         */
        int m_LVQIterationScale;

        /**< @brief LVQ Initial Alpha
         *     <p>
         *
         *     DEFAULT:  LTKPreprocDefaults::NN_DEF_LVQINITIALALPHA
         *     </p>
         */
        double m_LVQInitialAlpha;

          /**< @brief LVQ Distance Measure
         *     <p>
         *
         *     DEFAULT:  LTKPreprocDefaults::NN_DEF_LVQDISTANCEMEASURE
         *     </p>
         */
        string m_LVQDistanceMeasure;

        /**< @brief Pointer to LTKOSUtil interface
         *     <p>
         *
         *     </p>
         */
        LTKOSUtil*  m_OSUtilPtr;

          vector<LTKShapeSample> m_trainSet;

};


#endif
