#include "TrackerOpenCV.h"

#include <opencv2/tracking.hpp>

using namespace cv;
using namespace std;

TrackerOpenCV::TrackerOpenCV(TrackingTarget& target, TrackingStatus& state, Ptr<Tracker> tracker, const char* trackerName)
    :TrackerJT(target, state, TRACKING_TYPE::TYPE_RECT, __func__, FrameVariant::LOCAL_RGB)
    , tracker(tracker)
    , trackerName(trackerName)
{

}

void TrackerOpenCV::initCpu(Mat m)
{
    if (!window.empty())
    {
        Rect r(
            state.rect.x - window.x,
            state.rect.y - window.y,
            state.rect.width,
            state.rect.height
        );

        tracker->init(m(window), r);
    }
    else
    {
        tracker->init(m, state.rect);
    }
}

bool TrackerOpenCV::updateCpu(Mat m)
{
    try {
        Rect out;
        if (!window.empty())
        {
            if (!tracker->update(m(window), out))
                return false;

            out.x += window.x;
            out.y += window.y;
        }
        else
        {
            if (!tracker->update(m, out))
                return false;
        }


        state.rect = out;
    }
    catch (exception e)
    {
        return false;
    }

    return true;
}

const char* TrackerOpenCV::GetName()
{
    return trackerName;
};