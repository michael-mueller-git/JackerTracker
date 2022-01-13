#include "GpuTrackerGOTURN.h"

#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>

using namespace cv;
using namespace dnn;

GpuTrackerGOTURN::GpuTrackerGOTURN(TrackingTarget& target, TrackingStatus& state)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_RECT, __func__, FrameVariant::GPU_RGB)
{
    type = TRACKING_TYPE::TYPE_RECT;

    net = dnn::readNetFromCaffe("goturn.prototxt", "goturn.caffemodel");

    net.setPreferableBackend(dnn::DNN_BACKEND_CUDA);
    net.setPreferableTarget(dnn::DNN_TARGET_CUDA);

    CV_Assert(!net.empty());
}

void GpuTrackerGOTURN::initGpu(cuda::GpuMat image)
{
    image_ = image;
}

bool GpuTrackerGOTURN::updateGpu(cuda::GpuMat curFrame, cuda::Stream& stream)
{
    int INPUT_SIZE = 227;
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
    image_ = curFrame;

    return true;
}
