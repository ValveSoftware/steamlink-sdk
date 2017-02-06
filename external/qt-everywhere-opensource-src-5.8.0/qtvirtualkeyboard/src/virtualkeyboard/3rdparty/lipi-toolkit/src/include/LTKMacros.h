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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Contains common macro Definitions.
 *
 * CONTENTS:
 *
 * AUTHOR:     Vijayakumara M.
 *
 * DATE:       December 01, Aug,  2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Balaji, MNA  06 Jan, 2011    Macros defined in LTKInc.h are moved to this file
 ************************************************************************/

#ifndef __COMMONMACORS_H
#define __COMMONMACORS_H

#include "LTKTypes.h"

/** @file LTKmacros.h
* @brief Contains the macors used in reco/shaperec/pca and reco/shaperec/dtw modules.
*/
//macors used in reco/shaperec/pca and reco/shaperec/dtw modules.

/** @file LTKInc.h
*/
#define SUCCESS 0
#define FAILURE 1

#ifdef WIN32
    #define SEPARATOR "\\"
#else
    #define SEPARATOR "/"
#endif

#define COMMENTCHAR '#'
#define LIST_FILE_DELIMITER " \t\r"

//#define LTKReturnError(error)	errorCode = error; return error;
#define LTKReturnError(error)	return error;

#define SHAPESETFILE "shapeset.cfg"
#define PROFILEFILE "profile.cfg"
#define WORDFILE "word.cfg"
#define WORDPROFILEFILE "wordprofile.cfg"
#define STATSFILE "stats.txt"
#define NETWORKFILE "network.txt"
#define MODELSLISTFILE "modelslist.txt"
#define DICTIONARYFILE "dictionary.txt"

//File Extensions
#define CONFIGFILEEXT ".cfg"
#define DATFILEEXT ".mdt"
#define POSFILEEXT ".pos"

#define PI 3.1415926F
#define EPS 0.00001F
#define EPS1 0.001F

//Isolated character recognizers
#define PCA "pca"
#define DTW "dtw"
#define POS "pos"
#define ZONE "zone"
#define HOLISTIC "holistic"
#define BCC "bcc"
#define EigDef "eigdef"
#define NN "nn"
#define HMM "hmm"
#define ACTIVEDTW "activedtw"
#define NEURALNET	"neuralnet"

//Feature Extractors
#define POINT_FLOAT "pointfloat"
#define SUBSTROKE "substroke"
#define PARAMETRIC_CURVE "parametriccurve"
#define L7 "l7"
#define L7RADIUS "L7Radius"
#define NPEN "npen"
#define NPEN_WINDOW_SIZE "NPenWindowSize"
#define RELHANDPOSSC "relhandpossc"

//Word recognizers
#define DP "dp"
#define BOXFLD "boxfld"

//PCA Config  values
#define REVERSEFEATUREVECTORS "ReverseFeatureVectors"
#define TRACEDIMENSION "ResampTraceDimension"
#define NUMEIGENVECTORS "NumEigenVectors"
#define SIZETHRESHOLD "NormLineWidthThreshold"
#define ASPECTRATIOTHRESHOLD "NormPreserveAspectRatioThreshold"
#define DOTTHRESHOLD "NormDotSizeThreshold"
#define LOOPTHRESHOLD "LoopThreshold"
#define HOOKLENGTHTHRESHOLD1 "HookLengthThreshold1"
#define HOOKLENGTHTHRESHOLD2 "HookLengthThreshold2"
#define HOOKANGLETHRESHOLD "HookAngleThreshold"
#define SMOOTHFILTERLENGTH "SmoothWindowSize"
#define DISTANCEMEASURE "DistanceMeasure"
#define QUANTIZATIONSTEP "QuantizationStep"
#define CONFIDENCEMAPPINGFILE "ConfidenceMappingFile"
#define PRESERVEASPECTRATIO "NormPreserveAspectRatio"
#define PRESERVERELATIVEYPOSITION "NormPreserveRelativeYPosition"
#define RESAMPLINGMETHOD "ResampPointAllocation"


//NN Config values
#define FEATURES "Features"
#define PROTOTYPEDISTANCE "NNPrototypeDistanceMeasure"
#define NEARESTNEIGHBORS "NNRecoNumNearestNeighbors"
#define FEATUREPOINTDISTANCE "FeaturePointDistance"
#define DTWBANDING "NNDTWBandingRadius"
#define FEATUREEXTRACTOR "FeatureExtractor"
#define FEATUREREPRESENTATION "FeatureRepresentation"
#define LENGTHBASED "lengthbased"
#define POINTBASED "pointbased"
#define INTERPOINTDISTBASED "interpointdistbased"
#define LENGTHBASED_VARIABLE_NUMBER_OF_SAMPLES "lengthbasedvariablenumberofsamples"

//ACTIVEDTW Config values
#define RETAINPERCENTEIGENENERGY "ActiveDTWRetainPercentEigenEnergy"
#define EIGENSPREADVALUE "ActiveDTWEigenSpreadValue"
#define USESINGLETON "ActiveDTWUseSingleton"
#define MINCLUSTERSIZE "ActiveDTWMinClusterSize"
#define MAXCLUSTERSIZE "ActiveDTWMaxClusterSize"
#define MDTFILEUPDATEFREQ "ActiveDTWMDTFileUpdateFreq"

//NEURAL NET Config values
#define RANDOM_NUMBER_SEED	"SeedValueForRandomNumberGenaretor"
#define NEURALNET_LEARNING_RATE	"NeuralNetLearningRate"
#define NEURALNET_MOMEMTUM_RATE	"NeuralNetMomemtumRate"
#define NEURALNET_TOTAL_ERROR	"NeuralNetTotalError"
#define NEURALNET_INDIVIDUAL_ERROR	"NeuralNetIndividualError"
#define NEURALNET_HIDDEN_LAYERS_SIZE	"NeuralNetHiddenLayersSize"
#define NEURALNET_HIDDEN_LAYERS_UNITS	"NeuralNetHiddenLayersUnitSize"
#define NEURALNET_NORMALISED_FACTOR	"NeuralNetNormalizationFactor"
#define NEURALNET_WEIGHT_REESTIMATION		"ReestimateNeuralnetConnectionWeights"
#define	NEURALNET_TRAINING_ITERATION		"NeuralnetTrainingIteration"
#define	NEURALNET_TRAINING_SEQUENCE		"PrepareTrainingSequence"

//holistic config values
#define NUMNNS "numnns"
#define WORSTCASEVALUE "worstcasevalue"
#define NUMRESAMPLEPOINTS1 "NumResamplePoints1"
#define NUMRESAMPLEPOINTS2 "NumResamplePoints2"
#define SMOOTHFILTERLENGTH1 "SmoothFilterLength1"
#define SMOOTHFILTERLENGTH2 "SmoothFilterLength2"
#define SWANGNORMFACTOR "SwangNormFactor"
#define SWANGHOOKTHRESH "SwangHookThresh"
#define HORIZSTROKEANGLETHRESH "HorizStrokeAngleThresh"
//#define CONFIDENCEMAPPINGFILE "ConfidenceMappingFile"
//for bcc
#define NUMCHOICES "NumChoices"
#define COMBINATIONSCHEME "Combination"


#define NORMALIZETRACES "normalizetraces"
#define RESAMPLETRACES "resampletraces"
#define CENTERTRACES "centertraces"
#define REORDERTRACES "reordertraces"
#define REORIENTTRACES "reorienttraces"
#define NUMXGRIDS "numxgrids"
#define NUMYGRIDS "numygrids"
#define DISTRIBUTIONTHRESHOLD "distributionthreshold"
#define MINSHAPESUBSET "minshapesubset"
#define XTOLERANCE "xtolerance"
#define YTOLERANCE "xtolerance"
#define NUMXZONES "numxzones"
#define NUMYZONES "numyzones"
#define DENSITYTHRESH "densitythresh"
#define SAMPLETHRESH "samplethresh"
#define MAXBOXCOUNT "MaxBoxCount"
#define BOXEDSHAPEPROJECT "BoxedShapeProject"
#define BOXEDSHAPEPROFILE "BoxedShapeProfile"
#define DPSHAPEPROJECT "DPShapeProject"
#define DPSHAPEPROFILE "DPShapeProfile"
#define NUMSYMBOLS "NumSymbols"
#define NUMSHAPECHOICES "NumShapeChoices"
#define MINSHAPECONFID "MinShapeConfid"


//settings for DTW
#define FLEXIBILITYINDEX "FlexibilityIndex"
#define PROTOTYPESELECTION "NNTrainPrototypeSelectionMethod"
#define BANDING "Banding"
#define NUMFEATURES "NumFeatures"
#define PROTOREDFACTOR "NNTrainPrototypeReductionFactorPerClass"
#define NUMCLUSTERS "NNTrainNumPrototypesPerClass"
#define DTWEUCLIDEANFILTER "NNRecoDTWEuFilterOutputSize"
#define REJECT_THRESHOLD "NNRecoRejectThreshold"
#define ADAPTIVE_kNN "NNRecoUseAdaptiveKNN"

//settings of word recognizer
#define REC_UNIT_INFO "rec_unit_info"
#define USE_GRAMMAR   "use_grammar"
#define USE_WORDLIST  "use_wordlist"
#define USE_POSITIONAL INFO "use_positional_info"
#define REC_MODE "rec_mode"
#define DICTIONARY_MODE "dictionary_mode"

//For Word recognizer 

#define LTK_TRUE               0x01 
#define LTK_FALSE              0x00

//Reset parameters for Recognition context
#define LTK_RST_INK        0x02
#define LTK_RST_RECOGNIZER 0x04
#define LTK_RST_ALL        0xff

//Recognition units
#define REC_UNIT_UNKNOWN  0x08 //No segment info
#define REC_UNIT_SYMBOL   0x10 //Symbol
#define REC_UNIT_CHAR     0x11 //Char
#define REC_UNIT_WORD     0x12 //Word 

//Recognition mode for the recognizer
#define REC_MODE_BATCH     0x14 //batch mode
#define REC_MODE_STREAMING 0x16 //streaming (trace by trace)


//Dictionary options
#define DICT_DRIVEN			0x20	//recognizes only the words in the dictio
#define DICT_ASSISTED		0x21
#define DICT_OFF			0x22


/**
* @brief Defines the sequence of preprocessor functions to be executed
*/
#define PREPROCSEQUENCE "PreprocSequence"

/**
* @brief Defines the scale to be used to compute the number of iterations for LVQ training
*/
#define LVQITERATIONSCALE "LVQIterationScale"

/**
* @brief Defines initial value of alpha used in LVQ training
*/
#define LVQINITIALALPHA "LVQInitialAlpha"

/**
* @brief Defines the distance measure to be used in LVQ training.
*/
#define LVQDISTANCEMEASURE "LVQDistanceMeasure"

/**
* @brief Defines the name of the preprocessor dll file
*/
#define PREPROC "preproc"

/**
* @brief Defines the string compare functions
*/
#ifdef _WIN32
#define LTKSTRCMP _stricmp
#else
#define LTKSTRCMP strcasecmp
#endif

/**
* @brief Defines the name of the lipiengine dll file
*/
#define LIPIENGINE_MODULE_STR "lipiengine"

/**
* @brief Defines the name of the logger dll file
*/
#define LOGGER_MODULE_STR "logger"

/**
* @brief Defines the Lipi Root environment string
*/
#define LIPIROOT "$LIPI_ROOT"

/**
* @brief Defines the project is dynamic, where the numshapes can take any number of values
*/
#define DYNAMIC "Dynamic"

/**
* @brief Defines the header version for the model data file
*/
#define HEADERVERSION		"1.0.0"


#define VERSION_STR_LEN_SMALL	4

/**
* @brief Defines the length of the version information
*/
#define VERSION_STR_LEN			10

/**
* @brief Defines the checksum header string length
*/
#define CKSUM_HDR_STR_LEN		10

/**
* @brief Defines the starting error number
*/
#define ERROR_START_NO			100

/**
* @brief Defines the maximum error number
*/
#define MAX_ERROR_NO			240

/**
* @brief Defines the maximum file path length
*/
#define MAX_PATH_LEN			255

/**
* @brief Defines the maximum string length
*/
#define MAX_STRLEN				255

/**
* @brief Defines the lipi root environment string used in LipiEngine.cfg
*/
#define LIPIROOT_ENV_STRING	"LIPI_ROOT"

/**
* @brief Defines the lipi library environment string used in LipiEngine.cfg
*/
#define LIPILIB_ENV_STRING	"LIPI_LIB"

/**
* @brief Defines the type of project used in LipiEngine.cfg
*/
#define PROJECT_TYPE_STRING "ProjectType"

#define PROJECT_TYPE_SHAPEREC	"SHAPEREC"

/**
* @brief Defines the shape recognizer name used in LipiEngine.cfg
*/
#define SHAPE_RECOGNIZER_STRING "ShapeRecMethod"

/**
* @brief Defines the word recognizer name used in LipiEngine.cfg
*/
#define WORD_RECOGNIZER_STRING "WordRecognizer"

/**
* @brief Defines the path of the project directory in the Lipi tree
*/
#define PROJECTS_PATH_STRING	SEPARATOR + "projects" + SEPARATOR

/**
* @brief Defines the path of the profile directory in the Lipi tree
*/
#define PROFILE_PATH_STRING		SEPARATOR + "config" + SEPARATOR

/**
* @brief Defines the name of the default profile
*/
#define DEFAULT_PROFILE "default"

/**
* @brief Defines the name of profile config file
*/
#define PROFILE_CFG_STRING		"profile.cfg"

/**
* @brief Defines the name of project config file
*/
#define PROJECT_CFG_STRING		"project.cfg"

/**
* @brief Defines the NumShapes attributes in the project config file
*/
#define PROJECT_CFG_ATTR_NUMSHAPES_STR	"NumShapes"

/**
* @brief Defines the name of the create shape recognizer function
*/
#define CREATESHAPERECOGNIZER_FUNC_NAME	"createShapeRecognizer"

/**
* @brief Defines the name of the delete shape recognizer function
*/
#define DELETESHAPERECOGNIZER_FUNC_NAME	"deleteShapeRecognizer"

/**
* @brief Defines the name of the createWordRecognizer function
*/
#define CREATEWORDRECOGNIZER_FUNC_NAME	"createWordRecognizer"

/**
* @brief Defines the name of the deleteWordRecognizer function
*/
#define DELETEWORDRECOGNIZER_FUNC_NAME	"deleteWordRecognizer"

/**
* @brief Defines the name of the resampleTraceGroup function
*/
#define RESAMPLE_TRACE_GROUP_FUNC		"resampleTraceGroup"

/**
* @brief Defines the name of the resampleTraceGroup1 function
*/
#define RESAMPLE_TRACE_GROUP_ONE_FUNC	"resampleTraceGroup1"

/**
* @brief Defines the name of the preprocess function
*/
#define	PREPROCESS_FUNC					"preprocess"

/**
* @brief Defines the name of the normalizeSize function
*/
#define NORMALIZE_FUNC				    "normalizeSize"

#define NORMALIZE_LARGE_FUNC		    "normalizeLargerSize"

/**
* @brief Defines the name of the removeDuplicate function
*/
#define REMOVE_DUPLICATE_POINTS_FUNC	"removeDuplicatePoints"

/**
* @brief Defines the name of another removeDuplicate function
*/
#define REMOVE_DUPLICATE_POINTS_BY_ISI_FUNC	"removeDuplicatePointsByISI"

/**
* @brief Defines the name of the smoothenTraceGroup function
*/
#define SMOOTHEN_TRACE_GROUP_FUNC		"smoothenTraceGroup"

#define SMOOTHEN_MOVINGAVERAGE_TRACE_GROUP_FUNC		"smoothingByMovingAverage"

/**
* @brief Defines the name of the dehookTraces function
*/
#define DEHOOKTRACES_FUNC				"dehookTraces"

/**
* @brief Defines the name of the normalizeOrientation function
*/
#define NORMALIZE_ORIENTATION_FUNC		"normalizeOrientation"

/**
* @brief Defines the Delimiter sequence for tokenize the strings
*/
#define DELEMITER_SEQUENCE " {},"

/**
* @brief Defines the Delimiter character for separating the function name with class name
*/
#define DELEMITER_FUNC "::"

/**
* @brief Defines the Delimiter sequence for tokenize the header information
*/
#define TOKENIZE_DELIMITER	"<>=\n\r"

/**
* @brief Defines the string "CKS" (ChecKSum)
*/
#define CKS					"CKS"

/**
* @brief Defines the project name used for header information
*/
#define PROJNAME			"PROJNAME"

/**
* @brief Defines the project type used for header information
*/
#define PROJTYPE			"PROJTYPE"

/**
* @brief Defines the recognizer name used for header information
*/
#define RECNAME				"RECNAME"

/**
* @brief Defines the recognizer version used for header information
*/
#define	RECVERSION			"RECVERSION"

/**
* @brief Defines the string CREATETIME for header information
*/
#define CREATETIME			"CREATETIME"

/**
* @brief Defines the string MODTIME for header information
*/
#define MODTIME				"MODTIME"

/**
* @brief Defines the platform name (Windows or Linux)
*/
#define PLATFORM			"PLATFORM"

/**
* @brief Defines the length of the comment used in header
*/
#define COMMENTLEN			"COMMENTLEN"

/**
* @brief Defines the data offset length used in header
*/
#define DATAOFFSET			"DATAOFFSET"

/**
* @brief Defines the number of shapes used in header information
*/
#define NUMSHAPES			"NUMSHAPES"

/**
* @brief Defines the size of integer used in header information
*/
#define SIZEOFINT			"SIZEOFINT"

/**
* @brief Defines the size of unsigned integer used in header information
*/
#define SIZEOFUINT			"SIZEOFUINT"

/**
* @brief Defines the size of short integer used in header information
*/
#define SIZEOFSHORTINT		"SIZEOFSHORTINT"

/**
* @brief Defines the size of float used in header information
*/
#define SIZEOFFLOAT			"SIZEOFFLOAT"

/**
* @brief Defines the size of character used in header information
*/
#define SIZEOFCHAR			"SIZEOFCHAR"

/**
* @brief Defines the header length
*/
#define HEADERLEN			"HEADERLEN"

#define HIDDENLAYERSUNIT	"HIDDENLAYERSUNIT"

/**
* @brief Defines the dataset for header information
*/
#define DATASET				"DATASET"

/**
* @brief Defines the comment information in header
*/
#define COMMENT				"COMMENT"

/**
* @brief Defines the type of byte order (Little Endian or Big Endian)
*/
#define BYTEORDER			"BYTEORDER"

/**
* @brief Defines the operating system version
*/
#define OSVERSION			"OSVERSION"

/**
* @brief Defines the type of processor architecture
*/
#define PROCESSOR_ARCHITEC	"PROCESSOR_ARCHITEC"

/**
* @brief Defines the Header version for header information
*/
#define HEADERVER			"HEADERVER"


#define PCA_ALGO			"PCA"
#define WINDOWS				"Windows"
#ifndef LINUX
#define LINUX				"Linux"
#endif

//Defined twice
#define BYTEORDER			"BYTEORDER"

/**
* @brief Defines the create preprocessor method name
*/
#define CREATEPREPROCINST	"createPreprocInst"

/**
* @brief Defines the delete preprocessor method name
*/
#define DESTROYPREPROCINST	"destroyPreprocInst"

#define FI_MIN				0
#define FI_MAX				2

/**
* @brief Defines the minimum value in which the prototype reduction factor can take
*/
#define PROTOTYPEREDFACT_MIN 0

/**
* @brief Defines the maximum value in which the prototype reduction factor can take
*/
#define PROTOTYPEREDFACT_MAX 100

/**
* @brief Defines the input file type is the ink file
*/
#define INK_FILE				"ink"

/**
* @brief Defines the input file type is the feature file
*/
#define FEATURE_FILE			"feature"

/**
* @brief Defines the DTW type of distance to be calculated
*/
#define DTW_DISTANCE 			"dtw"

/**
* @brief Defines the euclidean type of distance to be calculated
*/
#define EUCLIDEAN_DISTANCE 		"eu"

/**
* @brief Defines the delimiter for the feature extractor
*/
#define FEATURE_EXTRACTOR_DELIMITER		"|"

/**
* @brief Define the delimiter for the hidden layer unit
*/
#define HIDDEN_LAYER_UNIT_DELIMITER		":"

/**
* @brief Defines the delimiter for the space (assuming only signle space between
* class and the features in MDT file
*/
#define CLASSID_FEATURES_DELIMITER		" "

/**
* @brief Defines the code for PointFloat Feature Extractor
*/
#define POINT_FLOAT_SHAPE_FEATURE_EXTRACTOR  			101

/**
* @brief Define the code for Angle Feature Extractor
*/
#define SUBSTROKE_SHAPE_FEATURE_EXTRACTOR		103

/**
* @brief Define the code for spline Feature Extractor
*/
#define PARAMETRIC_CURVE_SHAPE_FEATURE_EXTRACTOR		104


/**
* @brief Uesd in computation of Feature Extraction (Angle)
*/
#define DIRECTION_CODE_EAST				1

#define DIRECTION_CODE_NORTH_EAST		2

#define DIRECTION_CODE_NORTH			3

#define DIRECTION_CODE_NORTH_WEST		4

#define DIRECTION_CODE_WEST				5

#define DIRECTION_CODE_SOUTH_WEST		6

#define DIRECTION_CODE_SOUTH			7

#define DIRECTION_CODE_SOUTH_EAST		8

#define OVERLAP_THRESHOLD		15.0


/**
* @brief Defines the code for L7 Feature Extractor
*/
#define L7_SHAPE_FEATURE_EXTRACTOR  			102

/**
* @brief Defines the create preprocessor method name
*/
#define CREATE_SHAPE_FEATURE_EXTRACTOR	"createShapeFeatureExtractor"

/**
* @brief Defines the delete preprocessor method name
*/
#define DELETE_SHAPE_FEATURE_EXTRACTOR	"deleteShapeFeatureExtractor"

#define LEARNING_RATE	"LEARNING_RATE"

#define HIDDEN_LAYER	"HIDDEN_LAYER"

#define NORMALISED_FACTOR "NORMALISED_FACTOR"

#define MOMEMTUM_RATE	"MOMEMTUM_RATE"

#define FE_NAME	"FE_NAME"

#define FE_VER	"FE_VER"

#define PREPROC_SEQ "PREPROC_SEQ"

#define DOT_SIZE_THRES "DOT_SIZE_THRES"

#define ASP_RATIO_THRES "ASP_RATIO_THRES"

#define DOT_THRES "DOT_THRES"

#define PRESER_REL_Y_POS "PRESER_REL_Y_POS"

#define PRESER_ASP_RATIO "PRESER_ASP_RATIO"

#define NORM_LN_WID_THRES "NORM_LN_WID_THRES"

#define RESAMP_POINT_ALLOC "RESAMP_POINT_ALLOC"

#define SMOOTH_WIND_SIZE "SMOOTH_WIND_SIZE"

#define TRACE_DIM "TRACE_DIM"

#define PROTOTYPE_SELECTION_CLUSTERING "hier-clustering"

#define PROTOTYPE_SELECTION_LVQ	"lvq"

#define PROTO_RED_FACTOR_AUTOMATIC "automatic"

#define PROTO_RED_FACTOR_NONE	"none"

#define PROTO_RED_FACTOR_COMPLETE	"complete"

#define DTW_EU_FILTER_ALL	"all"

#define NAME_POINT_FLOAT_SHAPE_FEATURE_EXTRACTOR "PointFloatShapeFeatureExtractor"

#define NAME_SUBSTROKE_SHAPE_FEATURE_EXTRACTOR "SubStrokeShapeFeatureExtractor"

#define NAME_PARAMETRIC_CURVE_SHAPE_FEATURE_EXTRACTOR "ParametricCurveShapeFeatureExtractor"

#define NAME_L7_SHAPE_FEATURE_EXTRACTOR "L7ShapeFeatureExtractor"

#define NAME_NPEN_SHAPE_FEATURE_EXTRACTOR "NPenShapeFeatureExtractor"

#define NAME_RELHANDPOSSC_SHAPE_FEATURE_EXTRACTOR "RelHandPosSCFeatureExtractor"

#define NEW_LINE_DELIMITER '\n'

#define DELTE_SHAPE_FTR_POINTER_FUNC	"deleteShapeFeaturePtr"

#define MDT_FOPEN_MODE  "MDT_OPEN_MODE"

/**
@brief Defines the log file name
*/
#define LOG_FILE_NAME           "LogFile"

/**
@brief Defines the log level
*/
#define LOG_LEVEL               "LogLevel"

#define DEFAULT_LOG_FILE        "lipi.log"

#define DEFAULT_LOG_LEVEL       LTKLogger::LTK_LOGLEVEL_ERR

#define LOG_LEVEL_DEBUG         "DEBUG"

#define LOG_LEVEL_ERROR         "ERR"

#define LOG_LEVEL_INFO          "INFO"

#define LOG_LEVEL_ALL           "ALL"

#define LOG_LEVEL_VERBOSE       "VERBOSE"

#define LOG_LEVEL_OFF           "OFF"

#define MDT_UPDATE_FREQUENCY "NNMDTFileUpdateFreq"
/**
  @brief Used in computation of Confidence
*/
#define MIN_NEARESTNEIGHBORS 2

#define EUCLIDEAN_FILTER_OFF -1

#define CONF_THRESHOLD_FILTER_OFF 0

#define NUM_CHOICES_FILTER_OFF -1

#define LTK_START_SHAPEID 0

#define ADAPT_SCHEME	"AdaptScheme"

#define NAME_ADD_LVQ_ADAPT_SCHEME "AddLVQ"

#define MAX_PROTOTYPES "MaxNumPrototypes"

#define ADAPT_CONF_LOWER_BOUND "ConfidenceLowerBound"

#define ADAPT_CONF_UPPER_BOUND "ConfidenceUpperBound"

#define ADAPT_MISMATCH_HIGH_CONF "MismatchHighConfidence"

#define ADAPT_MISMATCH_LOW_CONF "MismatchLowConfidence"

#define ADAPT_MATCH_HIGH_CONF "MatchHighConfidence"

#define ADAPT_MATCH_LOW_CONF "MatchLowConfidence"

#define ADAPT_MIN_NUMBER_SAMPLES_PER_CLASS "MinimumNumberOfSamplesPerClass"
#define DEFAULT_PROFILE        "default"

#define DEFAULT_CHANNEL_NAME	"X"

#define DEFAULT_DATA_TYPE		DT_FLOAT

#define X_CHANNEL_NAME          "X"

#define Y_CHANNEL_NAME			"Y"

#define DEFAULT_SAMPLING_RATE   100

#define DEFAULT_X_DPI           2000

#define DEFAULT_Y_DPI           2000

#define DEFAULT_LATENCY         0.0

#define DEFAULT_ERROR_CODE      -1

#define LIPIENGINE_CFG_STRING  "lipiengine.cfg"

#define DEFAULT_SHAPE_RECO_CHOICES 5

#define DEFAULT_SHAPE_RECO_MIN_CONFID 0.0

#define EMPTY_STRING " "

#define MDT_FILE_OPEN_MODE "NNMDTFileOpenMode"

#define MIN_PERCENT_EIGEN_ENERGY 1

#define MAX_PERCENT_EIGEN_ENERGY 100

#define MIN_EIGENSPREADVALUE 1

//neuralnet
#define CONTINUE			0
#define EXIT_FAILURE		1
#define EXIT_SUCCESSFULLY	2

//for hmm clasifire

#define FEATURE_DIMENTION 8

#endif // #ifndef __COMMONMACORS_H

