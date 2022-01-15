#include "Trackers.h"
#include "FrameCache.h"
#include "GpuTrackerPoints.h"
#include "TrackerOpenCV.h"

#include <opencv2/tracking.hpp>
#include <opencv2/tracking/tracking_legacy.hpp>
#include <opencv2/dnn.hpp>

using namespace std;
using namespace cv;

TrackerJTStruct GetTracker(TrackerJTType type)
{
    switch (type) {
    case GPU_POINTS_PYLSPRASE:
        return {
            type,
            TRACKING_TYPE::TYPE_POINTS,
            "GpuTrackerPoints",
            [](auto& t, auto& s) {
                return new GpuTrackerPoints(t, s);
            }
        };
        /*
    case GPU_RECT_GOTURN:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "GpuTrackerGOTURN",
            [](auto& t, auto& s) {
                return new GpuTrackerGOTURN(t, s);
                //return new TrackerOpenCV(t, s, TrackerGOTURN::create(), "TrackerGOTURN");
            }
        };
        */
    case GPU_RECT_DIASIAM:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "TrackerDaSiamRPN",
            [](auto& t, auto& s) {
                TrackerDaSiamRPN::Params p;
                p.backend = dnn::DNN_BACKEND_CUDA;
                p.target = dnn::DNN_TARGET_CUDA;

                return new TrackerOpenCV(t, s, TrackerDaSiamRPN::create(p), "TrackerDaSiamRPN");
            }
        };
    case CPU_RECT_MEDIAN_FLOW:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "TrackerMedianFlow",
            [](auto& t, auto& s) {
                return new TrackerOpenCV(t, s, legacy::upgradeTrackingAPI(legacy::TrackerMedianFlow::create()), "TrackerMedianFlow");
            }
        };
    case CPU_RECT_CSRT:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "TrackerCSRT",
            [](auto& t, auto& s) {
                return new TrackerOpenCV(t, s, TrackerCSRT::create(), "TrackerCSRT");
            }
        };
        
    case CPU_RECT_MIL:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "TrackerMIL",
            [](auto& t, auto& s) {
                return new TrackerOpenCV(t, s, TrackerMIL::create(), "TrackerMIL");
            }
        };
        
    case CPU_RECT_KCF:
        return {
            type,
            TRACKING_TYPE::TYPE_RECT,
            "TrackerKCF",
            [](auto& t, auto& s) {
                return new TrackerOpenCV(t, s, TrackerKCF::create(), "TrackerKCF");
            }
        };

    default:
        return {
            TrackerJTType::TRACKER_TYPE_UNKNOWN,
            TRACKING_TYPE::TYPE_NONE,
            "Unknown",
            [](auto& t, auto& s) {
                return nullptr;
            }
        };
    }
}

// TrackerJT
TrackerJT::TrackerJT(TrackingTarget& target, TrackingStatus& state, TRACKING_TYPE type, const char* name, FrameVariant frameType)
    :state(state), type(type), name(name), frameType(frameType)
{
    assert(target.SupportsTrackingType(type));
    if (!target.range.empty())
        window = target.range;
}

void TrackerJT::init(cuda::GpuMat frame)
{
    try {
        if (FRAME_CACHE->IsCpu(frameType))
        {
            Mat cpuFrame = FRAME_CACHE->CpuVariant(frame, frameType);
            initCpu(cpuFrame);
        }
        else
        {
            cuda::GpuMat gpuFrame = FRAME_CACHE->GpuVariant(frame, frameType);
            initGpu(gpuFrame);
        }
    }
    catch (exception e) {
        state.active = false;
    }
}

bool TrackerJT::update(cuda::GpuMat frame, cuda::Stream& stream)
{
    if (!state.active)
        return false;

    if (FRAME_CACHE->IsCpu(frameType))
    {
        Mat cpuFrame = FRAME_CACHE->CpuVariant(frame, frameType);
        updateCpu(cpuFrame);
    }
    else
    {
        cuda::GpuMat gpuFrame = FRAME_CACHE->GpuVariant(frame, frameType);
        updateGpu(gpuFrame);
    }

    if (state.active && type == TRACKING_TYPE::TYPE_RECT) {
        state.center.x = state.rect.x + (state.rect.width / 2);
        state.center.y = state.rect.y + (state.rect.height / 2);

        state.size = state.rect.width + state.rect.height / 2;
    }

    return state.active;
}