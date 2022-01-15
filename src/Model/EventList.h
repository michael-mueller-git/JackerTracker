#pragma once

#include "TrackingEvent.h"
#include <map>
#include <memory>


class EventList;

typedef std::unique_ptr<EventList> EventListPtr;
typedef std::shared_ptr<TrackingEvent> EventPtr;

class EventList {
public:
    EventList();

    EventPtr GetEvent(time_t time, EventType type, TrackingTarget* target = nullptr, bool findLast = false);
    EventPtr AddEvent(EventType type, time_t t, TrackingTarget* target = nullptr);
    EventPtr AddEvent(EventPtr e);
    void GetEvents(time_t from, time_t to, std::vector<EventPtr>& out);
    void ClearEvents(std::function<bool(EventPtr e)> keepIf);

    static EventListPtr Unserialize(json& j);
    void Serialize(json& j);

    std::mutex mtx;
protected:
    std::vector<EventPtr>& TimeSlot(time_t time);
    std::map<time_t, std::vector<EventPtr>> events;
};