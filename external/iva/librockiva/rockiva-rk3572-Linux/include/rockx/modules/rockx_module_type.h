#ifndef _ROCKX_MODULE_TYPE_H_
#define _ROCKX_MODULE_TYPE_H_

#include "rockx_type.h"
#include "rockx_module.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROCKX_MAX_DETECT_NUM 256
#define ROCKX_MAX_POINTS_NUM 32
#define ROCKX_MAX_LANDMARK_POINTS_NUM 512
#define ROCKX_MAX_FEATURE_SIZE 512
#define ROCKX_MAX_AREAS_NUM 32

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

typedef struct {
    int x;
    int y;
    int z;
} RockXPoint;

typedef struct {
    float x;
    float y;
    float z;
} RockXPointF;

typedef struct {
    int num;
    RockXPoint points[ROCKX_MAX_POINTS_NUM];
} RockXPoints;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} RockXRect;

typedef struct {
    int centerx;
    int centery;
    int width;
    int height;
    float angle;
} RockXRectRotated;

typedef struct {
    RockXPoint leftTop;
    RockXPoint rightTop;
    RockXPoint rightBottom;
    RockXPoint leftBottom;
} RockXQuad;

typedef RockXPoints RockXPolygon;
typedef RockXPoints RockXArea;

typedef struct {
    int num;
    RockXArea areas[ROCKX_MAX_AREAS_NUM];
} RockXAreas;

typedef struct {
    RockXPoint head;
    RockXPoint tail;
} RockXLine;

typedef struct {
    uint16_t width;
    uint16_t height;
} RockXSize;

typedef struct {
    float pitch;  ///< Pitch angle ( < 0: Up, > 0: Down )
    float yaw;    ///< Yaw angle ( < 0: Left, > 0: Right )
    float roll;   ///< Roll angle ( < 0: Right, > 0: Left )
} RockXAngle;

/**
 * @brief Color
 */
typedef struct rockx_color_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RockXColor;

/* 目标状态 */
typedef enum {
    ROCKX_OBJECT_STATE_NONE,                /* 目标状态未知 */
    ROCKX_OBJECT_STATE_FIRST,               /* 目标第一次出现 */
    ROCKX_OBJECT_STATE_TRACKING,            /* 目标检测跟踪过程中 */
    ROCKX_OBJECT_STATE_LOST,                /* 目标检测丢失 */
    ROCKX_OBJECT_STATE_DISPEAR,             /* 目标消失 */
} RockXObjectState;

typedef struct {
    int id;
    float score;
    int cls;
    RockXRect box;
    RockXRect trackBox;
    const char* clsName;
} RockXDetectObject;

typedef struct {
    int count;
    RockXDetectObject objects[ROCKX_MAX_DETECT_NUM];
    RockXSize imageSize;
    int isTracked;
} RockXDetectObjects;

typedef struct {
    int id;
    float score;
    RockXRect box;
    int count;
    RockXPoint points[ROCKX_MAX_POINTS_NUM];
    float scores[ROCKX_MAX_POINTS_NUM];
} RockXLandmark;

typedef struct {
    int count;
    RockXLandmark landmarks[ROCKX_MAX_DETECT_NUM];
    RockXSize imageSize;
} RockXLandmarks;

/**
 * 人体关键点
 */
typedef struct {
    int id;                                     ///< 目标id
    float score;                                ///< 置信度
    RockXRect box;                              ///< 人形框
    RockXRectRotated leftHandBox;               ///< 左手框
    RockXRectRotated rightHandBox;              ///< 右手框
    float leftHandScore;                         ///< 左手置信度
    float rightHandScore;                        ///< 右手置信度
    int count;                                  ///< 关键点数量
    RockXPoint points[ROCKX_MAX_POINTS_NUM];    ///< 关键点坐标
    float scores[ROCKX_MAX_POINTS_NUM];         ///< 关键点置信度
} RockXBodyLandmark;

/** 
 * 人体关键点检测结果
 */
typedef struct {
    int count;                                  ///< 人体数量
    RockXBodyLandmark landmarks[ROCKX_MAX_DETECT_NUM]; ///< 人体关键点
    RockXSize imageSize;                        ///< 图像尺寸
    int isTracked;                              ///< 是否跟踪
} RockXBodyLandmarks;

typedef struct {
    uint64_t version;
    uint64_t flags;
    uint64_t size;
    void* data;
} RockXModelExtraInfo;

/**
 * 输入shape
 */
typedef struct {
    int index;
    RockXTensorShape shape;
} RockXInputShape;

/**
 * 输入shape范围
 */
typedef struct {
    int num;
    RockXTensorShape shape[512];  // 最多512个动态shape
} RockXInputShapeRanges;

/**
 * 模型信息
 */
typedef struct {
    int inNum;
    int outNum;
    RockXTensor* inputs;
    RockXTensor* outputs;
    int inWidth;
    int inHeight;
    int inChannel;
    RockXInputShapeRanges* inputsRange;  // 输入对应的动态shape信息
    RockXModelExtraInfo extraInfo;
} RockXModelInfo;

typedef struct {
    size_t totalMemUsed;
    size_t commandMemSize;
    size_t weightMemSize;
    size_t internalMemSize;
    size_t kvcacheMemSize;
} RockXCoreUsage;

typedef struct {
    char deviceId[64];
    int coreNum;
    size_t totalMemUsed;
    RockXCoreUsage cores[64];
} RockXDeviceUsage;

typedef struct {
    int deviceNum;
    RockXDeviceUsage devices[64];
} RockXModuleUsage;

/**
 * @brief 图像等比例缩放后的letterbox
 * 
 */
typedef struct RockXImageLetterBox {
    int width;
    int height;
    RockXRect roi;
    int padx;
    int pady;
    float scalex;
    float scaley;
} RockXImageLetterBox;

typedef struct RockXFaceQuality {
    RockXRect box;                  ///< face box
    float score;                    ///< quality score
    RockXAngle angle;               ///< face angle
    float blurValue;               ///< face blur
    float faceScore;               ///< face or nonface score
    float eyeScore;               ///< eyes score
    float nodeScore;               ///< nose score
    float mouthScore;              ///< mouth score
    float faceMaskScore;          ///< face withmask score (value range [0,1]）
    float faceOccluScore;         ///< faceocclu score (value range [0,1] higher is less occlusion）
    float faceCompleteScore;      ///< face image complete score (value  range [0,1] higher is more complete)
    float pd;                       ///< pupil distance
} RockXFaceQuality;

typedef struct RockXCarplateAttributeResult {
    int color;                  // 0:blue, 1:yellow, 2:green, 3:black, 4:white
    float colorScore;
    int layer;                  // 0: single layer; 1: double layer
    float layerScore;
    float plateScore;
} RockXCarplateAttributeResult;

/**
 * @brief 车牌类型
 * 
 */
typedef enum {
    ROCKX_PLATE_TYPE_UNKNOWN = 0,   /* 无车牌 */
    ROCKX_PLATE_TYPE_LARGE_CAR,     /* 大型汽车号牌 */
    ROCKX_PLATE_TYPE_SMALL_CAR,     /* 小型汽车号牌 */
    ROCKX_PLATE_TYPE_EMABSSY_CAR,   /* 使馆汽车号牌 */
    ROCKX_PLATE_TYPE_CONSULATE_CAR, /* 领馆汽车号牌 */
    ROCKX_PLATE_TYPE_TRAILER,       /* 挂车号牌 */
    ROCKX_PLATE_TYPE_COACH_CAR,     /* 教练车号牌 */
    ROCKX_PLATE_TYPE_POLICE_CAR,    /* 警车号牌 */
    ROCKX_PLATE_TYPE_HONGKONG,      /* 香港出入境号牌 */
    ROCKX_PLATE_TYPE_MACAO,         /* 澳门出入境号牌 */
    ROCKX_PLATE_TYPE_ARMED_POLICE,  /* 武警号牌 */
    ROCKX_PLATE_TYPE_PLA,           /* 军队号牌 */
    ROCKX_PLATE_TYPE_NEW_ENERGY,    /* 新能源号牌 */
    ROCKX_PLATE_TYPE_OTHER,         /* 其它号牌 */
} RockXCarplateType;

/**
 * @brief 车牌颜色
 * 
 */
typedef enum {
    ROCKX_PLATE_COLOR_UNKNOWN = 0,  /* 车牌颜色未知 */
    ROCKX_PLATE_COLOR_BLUE,         /* 蓝牌 */
    ROCKX_PLATE_COLOR_YELLOW,       /* 黄牌 */
    ROCKX_PLATE_COLOR_GREEN,        /* 绿牌 */
    ROCKX_PLATE_COLOR_BLACK,        /* 黑牌 */
    ROCKX_PLATE_COLOR_WHITE         /* 白牌 */
} RockXCarplateColor;

/**
 * @brief 车牌属性
 * 
 */
typedef struct {
    RockXCarplateType type;       /* 车牌类型 */
    RockXCarplateColor color;     /* 车牌颜色 */
} RockXCarplateAttribute;

typedef struct RockXCarplateRecognizeResult {
    int namecode[10];                   ///< Result code array (code table: @ref CARPLATE_RECOG_CODE)
    float scores[10];                   ///< Confidence for each result code
    int length;                         ///< Result array length
    float clarity;
    RockXCarplateAttribute attr;
} RockXCarplateRecognizeResult;


/**
 * @brief Tensor结果汇总
 *
 */
typedef struct AllRockXTensorResult {
    int count;
    RockXTensor tensorResult[ROCKX_MAX_DETECT_NUM];
} AllRockXTensorResult;

typedef struct RockXPageText {
    int count;
    char* text_chunks[ROCKX_MAX_DETECT_NUM];
} RockXPageText;

typedef struct AllRockXPageText {
    int count;
    RockXPageText pageText[ROCKX_MAX_DETECT_NUM];
} AllRockXPageText;

/**
 * @brief 文本向量模型初始化参数
 * 
 */
typedef struct RockXTextEmbeddingInitParam {
    char* tokenizerModelName;        /**< 文本分词器模型名称 */
} RockXTextEmbeddingInitParam;

/**
 * @brief 模块内部执行流程回调函数
 * 
 */
typedef int (*RockXModuleProcessCallback)(RockXModuleContext ctx, RockXPacket* inData, int inNum, RockXPacket* outData, int outNum, void** extraData);

/**
 * @brief
 */
typedef int (*RockXBatchModuleGetSize)(RockXModuleContext ctx, RockXInput* input);

/**
 * @brief
 */
typedef int (*RockXBatchModuleInitOutput)(RockXModuleContext ctx, RockXOutput* output, int size);

/**
 * @brief
 */
typedef int (*RockXBatchModuleSlice)(RockXModuleContext ctx, int index, RockXPacket* BatchInData, int BatchInNum, RockXPacket* SlicedData, int SlicedInNum, void** extraData);

/**
 * @brief
 */
typedef int (*RockXBatchModuleGather)(RockXModuleContext ctx, int index, RockXPacket* SlicedOutData, int SlicedOutNum, RockXPacket* BatchOutData, int BatchOutNum, void** extraData);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif