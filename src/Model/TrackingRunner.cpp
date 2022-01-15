#include "TrackingRunner.h"
#include "Gui/TrackingWindow.h"
#include "Tracking/FrameCache.h"

#include <opencv2/imgproc.hpp>
#include <magic_enum.hpp>
#include <chrono>
#include <thread>

#if !WIN32
#ifndef high_resolution_clock
#define high_resolution_clock steady_clock
#endif
#endif

using namespace std;
using namespace cv;
using namespace chrono;

// TrackerBinding

TrackerBinding::TrackerBinding(TrackingSetPtr set, TrackingTarget* target, TrackerJTStruct& trackerStruct, bool saveResults)
    :set(set), target(target), trackerStruct(trackerStruct), saveResults(saveResults)
{
    state.reset(target->InitTracking(trackerStruct.trackingType));
    tracker.reset(trackerStruct.Create(*target, *state));
};

TrackerBinding::~TrackerBinding()
{
    Join();
}

void TrackerBinding::Start()
{
    running = true;
    this->myThread = thread(&TrackerBinding::RunThread, this);
}

void TrackerBinding::Join()
{
    if(running)
    {
        running = false;
        myThread.join();
    }
}

void TrackerBinding::RunThread()
{
    while (running)
    {
        workMtx.lock();
        if (work.size() == 0)
        {
            workMtx.unlock();
            this_thread::sleep_for(50us);
            continue;
        }

        ThreadWorkPtr w = work.back();
        work.pop_back();
        workMtx.unlock();

        auto now = high_resolution_clock::now();
        if (!tracker->update(w->frame))
            w->err = true;

        w->durationMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();
        lastUpdateMs = w->durationMs;

        if (saveResults)
            state->SnapResult(set->events, w->frameTime);

        w->done = true;
    }
}

// TrackingRunner
TrackingRunner::TrackingRunner(TrackingWindow* w, TrackingSetPtr set, TrackingTarget* target, bool saveResults, bool allTrackerTypes, bool videoThread)
    :w(w), set(set), target(target), saveResults(saveResults), allTrackerTypes(allTrackerTypes)
{
    if (videoThread)
        videoReader = VideoReader::create(w->project.video);
}

TrackingRunner::~TrackingRunner()
{
    SetRunning(false);
}

RunnerState TrackingRunner::GetState(bool resetFramesRdy) 
{
    RunnerState ret = state;
    if (resetFramesRdy)
        state.framesRdy = 0;

    return ret;
}

void TrackingRunner::PushWork(int frames)
{
    while (workMap.size() < maxWork && frames > 0)
    {
        cuda::GpuMat gpuFrame;
        time_t time;
        if (videoReader)
        {
            gpuFrame = videoReader->NextFrame();
            time = videoReader->GetPosition();
        }
        else
        {
            gpuFrame = w->ReadCleanFrame();
            time = w->GetCurrentPosition();
        }

        if (set->events->GetEvent(time, EventType::TET_BADFRAME))
            continue;

        frames--;

        assert(workMap.count(time) == 0);

        FrameWork fw;
        fw.time = time;
        fw.timeStart = steady_clock::now();
        
        for (auto& b : bindings)
        {
            lock_guard<mutex> lock(b->workMtx);
            ThreadWorkPtr w = make_shared<ThreadWork>(gpuFrame, time);
            b->work.push_front(w);
            fw.work.push_back(w);
        }

        workMap.insert(pair<time_t, FrameWork>(time, fw));
    }
}

void TrackingRunner::PopWork()
{
    for (auto it = workMap.begin(), next_it = it; it != workMap.end(); it = next_it)
    {
        ++next_it;

        bool isDone = true;

        for (auto w : it->second.work)
            if (!w->done)
            {
                isDone = false;
                break;
            }

        if (!isDone)
            continue;

        state.framesRdy++;
        state.lastTime = max(state.lastTime, it->first);
        state.lastWorkMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - it->second.timeStart).count();
        
        workMap.erase(it);

        if (saveResults)
        {
            set->timeEnd = state.lastTime;
            calculator.Update(set, state.lastTime);
        }
    }
}

void TrackingRunner::Update(bool blocking)
{
    if (!initialized && !Setup())
        throw "Tracking setup failed";


    /*
    cuda::GpuMat frame = w->ReadCleanFrame();
    time_t time = w->GetCurrentPosition();
    FRAME_CACHE->CpuVariant(frame, FrameVariant::LOCAL_GREY);
    FRAME_CACHE->CpuVariant(frame, FrameVariant::LOCAL_RGB);

    for (auto& b : bindings)
    {
        auto now = high_resolution_clock::now();
        b->tracker->update(frame);
        b->lastUpdateMs = duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count();

        if (saveResults)
            b->state->SnapResult(set->events, time);
    }
    state.framesRdy++;

    return;
    */

    if(blocking)
        PushWork(1);
    else
        PushWork();

    PopWork();

    if (!running && workMap.size() > 0 && (state.framesRdy <= 30 || blocking))
        SetRunning(true);

    if (blocking)
    {
        auto now = steady_clock::now();
        while (workMap.size() > 0)
        {
            PopWork();
            this_thread::sleep_for(1ms);

            if (duration_cast<chrono::milliseconds>(high_resolution_clock::now() - now).count() > 1000)
                throw "Failed";
        }

        if (workMap.size() == 0)
            SetRunning(false);
    }
}

void TrackingRunner::SetRunning(bool r)
{
    if (r == running)
        return;

    running = r;
    
    for (auto& b : bindings)
    {
        if (running)
            b->Start();
        else
            b->Join();
    }
}

void TrackingRunner::AddTarget(TrackingTarget* target)
{
    if (allTrackerTypes)
    {
        for (auto& t : magic_enum::enum_entries<TrackerJTType>())
        {
            auto s = GetTracker(t.first);
            if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
                continue;

            if (!target->SupportsTrackingType(s.trackingType))
                continue;

            bindings.emplace_back(make_unique<TrackerBinding>(set, target, s, saveResults));
            bindings.back()->state->UpdateColor(bindings.size());
        }
    }
    else
    {
        auto s = GetTracker(target->preferredTracker);

        if (s.type == TrackerJTType::TRACKER_TYPE_UNKNOWN)
            return;

        if (!target->SupportsTrackingType(s.trackingType))
            return;

        bindings.emplace_back(make_unique<TrackerBinding>(set, target, s, saveResults));
        bindings.back()->state->UpdateColor(bindings.size());
    }
}

bool TrackingRunner::Setup()
{
    initialized = false;
    SetRunning(false);

    if (saveResults)
        set->events->ClearEvents([](EventPtr e) { return e->type == EventType::TET_BADFRAME; });

    calculator.Reset();

    if (bindings.size() > 0)
        bindings.clear();

    if (workMap.size() > 0)
        workMap.clear();

    state.framesRdy = 0;
    state.lastTime = 0;
    state.lastWorkMs = 999;

    if (videoReader)
    {
        videoReader->Seek(set->timeStart);
    }
    else
    {
        w->timebar.SelectTrackingSet(set);
    }

    if (target)
        AddTarget(target);
    else
        for (auto& t : set->targets)
            AddTarget(&t);

    if (bindings.size() == 0)
        return false;

    cuda::GpuMat firstFrame;

    if (!videoReader)
        firstFrame = w->GetInFrame();
    else
        firstFrame = videoReader->NextFrame();

    for (auto& b : bindings)
        b->tracker->init(firstFrame);

    initialized = true;
    return initialized;
}

void TrackingRunner::Draw(Mat& frame)
{
    int y = 100;

    putText(frame, format("Frame: %dms", state.lastWorkMs), Point(400, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 0, 0), 2);
    y += 20;

    for (auto& b : bindings)
    {
        b->state->Draw(frame);
        putText(frame, format("%s: %dms", b->tracker->GetName(), b->lastUpdateMs), Point(400, y), FONT_HERSHEY_SIMPLEX, 0.6, b->state->color, 2);
        y += 20;
    }

    if (saveResults)
    {
        calculator.Draw(set, frame, state.lastTime);
    }
}
