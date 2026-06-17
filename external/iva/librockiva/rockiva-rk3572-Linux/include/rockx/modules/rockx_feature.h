#ifndef __ROCKX_FEATURE_H__
#define __ROCKX_FEATURE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rockx_type.h"

typedef struct {
    int version;
    RockXDataType dtype;
    RockXTensorQntInfo qntInfo;
    void* extInfo;
    int size;
    char feature[0];
} RockXFeature;

typedef struct {
    float score;
    uint64_t id;
    RockXFeature* feature;
} RockXFeatureSearchResult;

size_t RockXFeatureObjectSize(int size, RockXDataType dtype);

RockXFeature* RockXFeatureObjectCreate(int version, int size, RockXDataType dtype, RockXTensorQntInfo* qntInfo, void* feature_data, void* extInfo); 

RockXFeature* RockXFeatureObjectCopy(RockXFeature* feature);

RockXFeature* RockXFeatureObjectArrayCreate(int num, int feature_size, RockXDataType dtype);

int RockXFeatureObjectArraySet(RockXFeature* feature_array, int num, int index, RockXFeature* feature);

int RockXFeatureObjectArraySet1(RockXFeature* feature_array, int num, int index, void* feature_data, RockXTensorQntInfo* qntInfo, void* extInfo);

RockXFeature* RockXFeatureObjectArrayGet(RockXFeature* feature_array, int num, int index);

int RockXFeatureObjectArrayFree(RockXFeature* feature_array, int num);

#ifdef __cplusplus
}
#endif

#endif