#include "Trackers.h"
#include <opencv2/dnn.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/highgui.hpp>

// TrackerJT
TrackerJT::TrackerJT(TrackingTarget& target, TRACKING_TYPE type)
    :state(target.InitTracking()), type(type)
{
    assert(type == target.trackingType);
}

void TrackerJT::init(cuda::GpuMat image)
{
    initInternal(image);
}

bool TrackerJT::update(cuda::GpuMat image)
{
    if (!state.active)
        return false;

    updateInternal(image);
}

// GpuTrackerGOTURN

GpuTrackerGOTURN::GpuTrackerGOTURN(TrackingTarget& target)
    :TrackerJT(target, TRACKING_TYPE::TYPE_RECT)
{
    type = TRACKING_TYPE::TYPE_RECT;

    net = dnn::readNetFromCaffe("goturn.prototxt", "goturn.caffemodel");

    net.setPreferableBackend(dnn::DNN_BACKEND_CUDA);
    net.setPreferableTarget(dnn::DNN_TARGET_CUDA);

    CV_Assert(!net.empty());
}

void GpuTrackerGOTURN::initInternal(cuda::GpuMat image)
{
    image_ = image.clone();
}

bool GpuTrackerGOTURN::updateInternal(cuda::GpuMat image)
{
    int INPUT_SIZE = 227;
    //Using prevFrame & prevBB from model and curFrame GOTURN calculating curBB
    cuda::GpuMat curFrame = (cuda::GpuMat)image;
    cuda::GpuMat prevFrame = image_;
    
    Rect2d prevRect = state.currentRect;
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

    imshow("Last", Mat(targetPatch));
    imshow("New", Mat(searchPatch));

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
    state.currentRect = curRect & Rect(Point(0, 0), image_.size());

    // Set new model image and BB from current frame
    image_ = ((cuda::GpuMat)image).clone();
    
    return true;
}

// GpuTrackerPoints

GpuTrackerPoints::GpuTrackerPoints(TrackingTarget& target)
    :TrackerJT(target, TRACKING_TYPE::TYPE_POINTS)
{

}

void GpuTrackerPoints::initInternal(cuda::GpuMat image)
{
    image_ = image.clone();
    points_ = cuda::GpuMat(state.points.size());

    for (int p = 0; p < state.points.size(); p++)
    {
        PointState ps;
        ps.lastPosition = state.points[p].point;
        ps.cudaIndex = p;
        ps.cudaIndexNew = -1;

        pointStates.insert(pair<TrackingTarget::PointState*, PointState>(&state.points[p], ps));
    }
}

bool GpuTrackerPoints::updateInternal(cuda::GpuMat image)
{
    cuda::GpuMat points, pointStatus, reduced;

    opticalFlowTracker->calc(
        image_,
        image,
        points_,
        points,
        pointStatus
    );

    cuda::reduce(points, reduced, 1, REDUCE_AVG);
    
    vector<Point2f> center;
    reduced.download(center);
    state.currentCenter = center.at(0);

    vector<uchar> status(pointStatus.cols);
    pointStatus.download(status);
    if (find(status.begin(), status.end(), 0) != status.end())
    {
        // We found some errors
        
        vector<Point2f> pointsDL(points.cols), points_DL(points_.cols);
        points.download(pointsDL);
        points_.download(points_DL);

        vector<Point2f> trackPointsNew;

        for (int p = 0; p < pointsDL.size(); p++)
        {
            auto it = find_if(
                pointStates.begin(),
                pointStates.end(),
                [p](PointState& ps) { return ps.cudaIndex == p; }
            );

            assert(it != kv.second.pointStatus.end());

            if (status[p] == 1)
            {
                it->second.cudaIndexNew = trackPointsNew.size();
                trackPointsNew.push_back(pointsDL[p]);
            }
            else
            {
                it->second.cudaIndexNew = -1;
                it->second.cudaIndex = -1;
                it->first->active = false;
            }
        }

        if (trackPointsNew.size() == 0)
        {
            state.active = false;
            return false;
        }

        points_.upload(trackPointsNew);
        for (auto& ps : pointStates)
        {
            if (ps.second.cudaIndexNew != -1)
            {
                ps.second.cudaIndex = ps.second.cudaIndexNew;
                ps.second.cudaIndexNew = -1;
            }
        }
    }

    image_ = image.clone();
    points_ = points.clone();

    return true;
}