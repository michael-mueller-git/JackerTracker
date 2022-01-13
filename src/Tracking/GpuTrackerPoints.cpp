#include "GpuTrackerPoints.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/cudaarithm.hpp>

using namespace cv;
using namespace std;

GpuTrackerPoints::GpuTrackerPoints(TrackingTarget& target, TrackingStatus& state, Params params)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_POINTS, __func__, FrameVariant::GPU_GREY),
    params(params)
{
    opticalFlowTracker = cuda::SparsePyrLKOpticalFlow::create(
        Size(params.size, params.size),
        params.maxLevel,
        params.iters
    );
}

void GpuTrackerPoints::initGpu(cuda::GpuMat sourceImg)
{
    if (!window.empty()) {
        image_ = sourceImg(window).clone();
    }
    else
    {
        image_ = sourceImg.clone();
    }

    points_ = cuda::GpuMat(state.points.size());
    vector<Point2f> points;

    for (int p = 0; p < state.points.size(); p++)
    {
        GpuPointState ps;
        ps.cudaIndex = p;
        ps.cudaIndexNew = -1;
        Point2f point = state.points.at(p).point;
        if (!window.empty()) {
            point -= Point2f(window.x, window.y);

            if (point.x > window.width || point.y > window.height)
                continue;
        }


        points.push_back(point);
        pointStates.insert(pair<PointState*, GpuPointState>(&state.points[p], ps));
    }

    points_.upload(points);
}

bool GpuTrackerPoints::updateGpu(cuda::GpuMat sourceImg, cuda::Stream& stream)
{
    cuda::GpuMat points, pointStatus, reduced;
    vector<Point2f> pointsDL;

    if (!window.empty()) {
        cuda::GpuMat img = sourceImg(window).clone();

        opticalFlowTracker->calc(
            image_,
            img,
            points_,
            points,
            pointStatus
        );
    }
    else {
        cuda::GpuMat img = sourceImg(Rect(0, 0, sourceImg.cols, sourceImg.rows));

        opticalFlowTracker->calc(
            image_,
            img,
            points_,
            points,
            pointStatus,
            noArray(),
            stream
        );
    }



    vector<uchar> status(pointStatus.cols);
    pointStatus.download(status, stream);


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
                [p](pair<PointState*, GpuPointState> ps) { return ps.second.cudaIndex == p; }
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

    if (state.targetType == TARGET_TYPE::TYPE_BACKGROUND)
    {
        Point2f center;
        float radius;
        minEnclosingCircle(pointsDL, center, radius);

        state.center = center;
        state.size = radius;
    }
    else
    {
        cuda::reduce(points, reduced, 1, REDUCE_AVG, -1, stream);

        vector<Point2f> center;
        reduced.download(center, stream);
        state.center = center.at(0);
    }

    if (!window.empty())
        state.center += Point(window.x, window.y);

    for (auto& kv : pointStates)
    {
        if (kv.first->active)
        {
            kv.first->point = pointsDL.at(kv.second.cudaIndex);
            if (!window.empty())
                kv.first->point += Point(window.x, window.y);
        }
    }

    if (window.empty())
        image_ = sourceImg.clone();
    else
        image_ = sourceImg(window).clone();

    points_ = points.clone();

    return true;
}