#include "Trackers.h"
#include <opencv2/dnn.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/highgui.hpp>
#include <typeinfo>

FrameCache* FRAME_CACHE = new FrameCache();

void FrameCache::Update(void* frame, FrameVariant v)
{
    Clear();
    current = frame;
    currentType = v;
};

void FrameCache::Clear()
{
    cache.clear();

    current = nullptr;
    currentType = FrameVariant::UNKNOWN;
}

void* FrameCache::GetVariant(FrameVariant v, cuda::Stream& stream)
{
    assert(currentType == FrameVariant::GPU_RGBA);
    assert(current != nullptr);

    if (cache.find(v) != cache.end())
        return cache.at(v).GetPtr();

    void* cachedPtr = nullptr;
    cuda::GpuMat* var;

    switch (v) {
        case FrameVariant::GPU_RGBA:
            return current;

        case FrameVariant::GPU_GREY:
            cache.emplace(pair<FrameVariant, someFrame>(v, false));
            cuda::cvtColor(*(cuda::GpuMat*)current, cache.at(v).gpuFrame, COLOR_BGRA2GRAY, 0, stream);
            
            return cache.at(v).GetPtr();

        case FrameVariant::GPU_RGB:
            cache.emplace(pair<FrameVariant, someFrame>(v, false));
            cuda::cvtColor(*(cuda::GpuMat*)current, cache.at(v).gpuFrame, COLOR_BGRA2BGR, 0, stream);
            return cache.at(v).GetPtr();

        case FrameVariant::LOCAL_RGB:
            cache.emplace(pair<FrameVariant, someFrame>(v, true));
            cachedPtr = GetVariant(FrameVariant::GPU_RGB);
            var = (cuda::GpuMat*)cachedPtr;
            var->download(cache.at(v).cpuFrame, stream);
            return cache.at(v).GetPtr();

        case FrameVariant::LOCAL_GREY:
            cache.emplace(pair<FrameVariant, someFrame>(v, true));
            cachedPtr = GetVariant(FrameVariant::GPU_GREY);
            var = (cuda::GpuMat*)cachedPtr;
            var->download(cache.at(v).cpuFrame, stream);
            return cache.at(v).GetPtr();
        default:
            throw "Failed";
    }

    return nullptr;
}

// TrackerJT
TrackerJT::TrackerJT(TrackingTarget& target, TrackingTarget::TrackingState& state, TRACKING_TYPE type, const char* name, FrameVariant frameType)
    :state(state), type(type), name(name), frameType(frameType)
{
    assert(target.SupportsTrackingType(type));
}

void TrackerJT::init()
{
    void* image = FRAME_CACHE->GetVariant(frameType);
    initInternal(image);
}

bool TrackerJT::update(cuda::Stream& stream)
{
    if (!state.active)
        return false;

    void* image = FRAME_CACHE->GetVariant(frameType, stream);
    updateInternal(image, stream);
    if (type == TRACKING_TYPE::TYPE_RECT) {
        state.center.x = state.rect.x + (state.rect.width / 2);
        state.center.y = state.rect.y + (state.rect.height / 2);
    }
}

// GpuTrackerGOTURN

GpuTrackerGOTURN::GpuTrackerGOTURN(TrackingTarget& target, TrackingTarget::TrackingState& state)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_RECT, __func__, FrameVariant::GPU_RGB)
{
    type = TRACKING_TYPE::TYPE_RECT;

    net = dnn::readNetFromCaffe("goturn.prototxt", "goturn.caffemodel");

    net.setPreferableBackend(dnn::DNN_BACKEND_CUDA);
    net.setPreferableTarget(dnn::DNN_TARGET_CUDA);

    CV_Assert(!net.empty());
}

void GpuTrackerGOTURN::initInternal(void* image)
{
    image_ = ((cuda::GpuMat*)image)->clone();
}

bool GpuTrackerGOTURN::updateInternal(void* image, cuda::Stream& stream)
{
    int INPUT_SIZE = 227;
    //Using prevFrame & prevBB from model and curFrame GOTURN calculating curBB
    cuda::GpuMat& curFrame = *(cuda::GpuMat*)image;
    cuda::GpuMat prevFrame = image_;
    
    Rect2d prevRect = state.rect;
    Rect curRect;

    float padTargetPatch = 2.0;
    Rect2f searchPatchRect, targetPatchRect;
    Point2f currCenter, prevCenter;
    
    cuda::GpuMat prevFramePadded, curFramePadded;
    cuda::GpuMat searchPatch, targetPatch;

    prevCenter.x = (float)(prevRect.x + prevRect.width / 2);
    prevCenter.y = (float)(prevRect.y + prevRect.height / 2);

    targetPatchRect.width = (float)(prevRect.width * padTargetPatch);
    targetPatchRect.height = (float)(prevRect.height * padTargetPatch);

    targetPatchRect.x = (float)(prevCenter.x - prevRect.width * padTargetPatch / 2.0 + targetPatchRect.width);
    targetPatchRect.y = (float)(prevCenter.y - prevRect.height * padTargetPatch / 2.0 + targetPatchRect.height);

    targetPatchRect.width = std::min(targetPatchRect.width, (float)prevFrame.cols);
    targetPatchRect.height = std::min(targetPatchRect.height, (float)prevFrame.rows);
    targetPatchRect.x = std::max(-prevFrame.cols * 0.5f, std::min(targetPatchRect.x, prevFrame.cols * 1.5f));
    targetPatchRect.y = std::max(-prevFrame.rows * 0.5f, std::min(targetPatchRect.y, prevFrame.rows * 1.5f));

    cuda::copyMakeBorder(
        prevFrame,
        prevFramePadded,
        (int)targetPatchRect.height,
        (int)targetPatchRect.height,
        (int)targetPatchRect.width,
        (int)targetPatchRect.width,
        BORDER_REPLICATE
    );
    targetPatch = prevFramePadded(targetPatchRect).clone();

    cuda::copyMakeBorder(
        curFrame,
        curFramePadded,
        (int)targetPatchRect.height,
        (int)targetPatchRect.height,
        (int)targetPatchRect.width,
        (int)targetPatchRect.width,
        BORDER_REPLICATE
    );
    searchPatch = curFramePadded(targetPatchRect).clone();

    // Preprocess
    // Resize
    cuda::resize(targetPatch, targetPatch, Size(INPUT_SIZE, INPUT_SIZE), 0, 0, INTER_LINEAR);
    cuda::resize(searchPatch, searchPatch, Size(INPUT_SIZE, INPUT_SIZE), 0, 0, INTER_LINEAR);

    // Convert to Float type and subtract mean
    Mat targetBlob = dnn::blobFromImage(Mat(targetPatch), 1.0f, Size(), Scalar::all(128), false);
    Mat searchBlob = dnn::blobFromImage(Mat(searchPatch), 1.0f, Size(), Scalar::all(128), false);

    net.setInput(targetBlob, "data1");
    net.setInput(searchBlob, "data2");

    Mat resMat = net.forward("scale").reshape(1, 1);

    curRect.x = cvRound(targetPatchRect.x + (resMat.at<float>(0) * targetPatchRect.width / INPUT_SIZE) - targetPatchRect.width);
    curRect.y = cvRound(targetPatchRect.y + (resMat.at<float>(1) * targetPatchRect.height / INPUT_SIZE) - targetPatchRect.height);
    curRect.width = cvRound((resMat.at<float>(2) - resMat.at<float>(0)) * targetPatchRect.width / INPUT_SIZE);
    curRect.height = cvRound((resMat.at<float>(3) - resMat.at<float>(1)) * targetPatchRect.height / INPUT_SIZE);

    // Predicted BB
    state.rect = curRect & Rect(Point(0, 0), image_.size());

    // Set new model image and BB from current frame
    image_ = curFrame.clone();
    
    return true;
}

// GpuTrackerPoints

GpuTrackerPoints::GpuTrackerPoints(TrackingTarget& target, TrackingTarget::TrackingState& state)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_POINTS, __func__, FrameVariant::GPU_GREY)
{
    opticalFlowTracker = cuda::SparsePyrLKOpticalFlow::create(
        Size(15, 15),
        5,
        30
    );
}

void GpuTrackerPoints::initInternal(void* image)
{
    image_ = ((cuda::GpuMat*)image)->clone();
    points_ = cuda::GpuMat(state.points.size());
    vector<Point2f> points;

    for (int p = 0; p < state.points.size(); p++)
    {
        PointState ps;
        ps.cudaIndex = p;
        ps.cudaIndexNew = -1;

        points.push_back(state.points.at(p).point);
        pointStates.insert(pair<TrackingTarget::PointState*, PointState>(&state.points[p], ps));
    }

    points_.upload(points);
}

bool GpuTrackerPoints::updateInternal(void* image, cuda::Stream& stream)
{
    cuda::GpuMat points, pointStatus, reduced;

    opticalFlowTracker->calc(
        image_,
        *(cuda::GpuMat*)image,
        points_,
        points,
        pointStatus,
        noArray(),
        stream
    );

    vector<uchar> status(pointStatus.cols);
    pointStatus.download(status, stream);

    vector<Point2f> pointsDL;
    points.download(pointsDL, stream);

    if (find(status.begin(), status.end(), 0) != status.end())
    {
        // We found some errors
        
        vector<Point2f> trackPointsNew;

        for (int p = 0; p < pointsDL.size(); p++)
        {
            auto it = find_if(
                pointStates.begin(),
                pointStates.end(),
                [p](pair<TrackingTarget::PointState*, PointState> ps) { return ps.second.cudaIndex == p; }
            );

            assert(it != pointStates.end());

            if (status[p] == 1)
            {
                it->second.cudaIndexNew = trackPointsNew.size();
                trackPointsNew.push_back(pointsDL[p]);
            }
            else
            {
                it->second.cudaIndex = -1;
                it->first->active = false;
            }
        }

        if (trackPointsNew.size() == 0)
        {
            state.active = false;
            return false;
        }

        points.upload(trackPointsNew, stream);
        for (auto& ps : pointStates)
        {
            if (ps.second.cudaIndexNew != -1)
            {
                ps.second.cudaIndex = ps.second.cudaIndexNew;
                ps.second.cudaIndexNew = -1;
            }
        }
    }

    cuda::reduce(points, reduced, 1, REDUCE_AVG, -1, stream);

    vector<Point2f> center;
    reduced.download(center, stream);
    state.center = center.at(0);

    for (auto& kv : pointStates)
    {
        if(kv.first->active)
            kv.first->point = pointsDL.at(kv.second.cudaIndex);
    }

    image_ = ((cuda::GpuMat*)image)->clone();
    points_ = points.clone();

    return true;
}

// TrackerOpenCV

TrackerOpenCV::TrackerOpenCV(TrackingTarget& target, TrackingTarget::TrackingState& state, Ptr<Tracker> tracker, const char* trackerName)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_RECT, __func__, FrameVariant::LOCAL_RGB)
    , tracker(tracker)
    , trackerName(trackerName)
{
    
}

void TrackerOpenCV::initInternal(void* image)
{
    Mat& m = *(Mat*)image;
    tracker->init(m, state.rect);
}

bool TrackerOpenCV::updateInternal(void* image, cuda::Stream& stream)
{
    Mat& m = *(Mat*)image;
    return tracker->update(m, state.rect);
}

const char* TrackerOpenCV::GetName()
{
    return trackerName;
};