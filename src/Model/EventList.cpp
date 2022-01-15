#include "EventList.h"

using namespace std;

EventList::EventList()
{

}

vector<EventPtr>& EventList::TimeSlot(time_t time)
{
    if (events.count(time) == 0)
    {
        events.emplace(time, vector<EventPtr>());
    }

    return events.at(time);
}

EventPtr EventList::GetEvent(time_t time, EventType type, TrackingTarget* target, bool findLast)
{
    if (!findLast)
    {
        if (events.count(time) == 0)
            return nullptr;

        for (auto& e : TimeSlot(time))
        {
            if (e->type != type)
                continue;

            if (target && e->targetGuid != target->GetGuid())
                continue;

            return e;
        }

        return nullptr;
    }

    bool search = false;
    for (auto rit = events.rbegin(); rit != events.rend(); rit++)
    {
        if (!search && rit->first < time)
            search = true;

        if (search)
        {
            EventPtr e = GetEvent(rit->first, type, target, false);
            if (e)
                return e;
        }
    }

    return nullptr;
}

EventPtr EventList::AddEvent(EventPtr e)
{
    TimeSlot(e->time).push_back(e);
    return e;
}

EventPtr EventList::AddEvent(EventType type, time_t t, TrackingTarget* target)
{
    EventPtr e = make_unique<TrackingEvent>(type, t);
    
    if (target)
        e->targetGuid = target->GetGuid();

    TimeSlot(t).push_back(e);

    return e;
}

void EventList::GetEvents(time_t from, time_t to, std::vector<EventPtr>& out)
{
    for (auto& kv : events)
    {
        if (from != 0 && kv.first < from)
            continue;

        if (to != 0 && kv.first > to)
            continue;

        for (auto& e : kv.second)
            out.push_back(e);
    }
}

void EventList::ClearEvents(std::function<bool(EventPtr e)> keepIf)
{
    map<time_t, vector<EventPtr>> filtered;

    for (auto& kv : events)
    {
        vector<EventPtr> eventsVector;
        copy_if(kv.second.begin(), kv.second.end(), back_inserter(eventsVector), keepIf);

        if (eventsVector.size() > 0)
        {
            filtered.emplace(kv.first, eventsVector);
        }
    }

    events = filtered;
}

EventListPtr EventList::Unserialize(json& j)
{
    EventListPtr list = make_unique<EventList>();

    for (auto& e : j)
    {
        EventPtr eptr = make_shared<TrackingEvent>(TrackingEvent::Unserialize(e));
        list->AddEvent(eptr);
    }

    return list;
}

void EventList::Serialize(json& j)
{
    j = json::array();

    for (auto& kv : events)
    {
        for (auto& e : kv.second)
        {
            e->Serialize(j[j.size()]);
        }
    }
}