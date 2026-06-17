#ifndef __FACE_DETECTOR_H__
#define __FACE_DETECTOR_H__

#include "uvc_mpi_vi.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum FaceDetectorType {
  DETECTOR_TYPE_ROCKIVA = 0,
  DETECTOR_TYPE_FACE_NN = 1,
  DETECTOR_TYPE_UNKNOWN = 255
};

#define MAX_DETECT_OBJECTS 40
#define VI_MAX_INPUT_COUNT 8

typedef struct {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  int32_t score;
  int32_t id;
  int32_t yaw;
  int32_t pitch;
  int32_t roll;
} FaceBox;

typedef struct {
  int32_t count;
  FaceBox boxes[MAX_DETECT_OBJECTS];
} FaceInfo;

struct FrameCacheEntry {
  int frameId;
  void *frame;
  UVC_VI_CHN_CTX_S viCtx;
};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <mutex>
class IFaceDetector {
public:
  virtual ~IFaceDetector() {releaseAllFrames();};
  
  virtual void destroy() = 0;

  virtual int writeRGB888Frame(uint16_t width, uint16_t height,
                               uint32_t frame_id, unsigned char *buffer) = 0;
  virtual int writeRGB888FrameByFd(uint16_t width, uint16_t height,
                                   uint32_t frame_id, int32_t fd) = 0;
  virtual int writeNV12FrameByFd(uint16_t width, uint16_t height,
                                 uint32_t frame_id, int32_t fd) = 0;
  virtual void getFaceInfo(FaceInfo *outInfo) = 0;

  void insertFrame(int frameId, void *frame, UVC_VI_CHN_CTX_S *viCtx);

  void releaseFrameById(int frameId);

protected:
  FrameCacheEntry mFrameCache[VI_MAX_INPUT_COUNT] = {};

  std::mutex mFrameCacheMutex;

  void releaseAllFrames();
};
#endif // __cplusplus

#endif // __FACE_DETECTOR_H__
