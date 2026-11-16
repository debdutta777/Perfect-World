#pragma once

#include "Countdown.h"
#include "FaridaSchedule.h"
#include "LaneOrigin.h"
#include "SchoolLifeContent.h"

#include <algorithm>

namespace PWAwakening
{
// P315: this is also used by the map-free automation. The registry is the caller's
// authored cast / scheduled-person roster, never a guess based on a display label.
inline bool P315BindCountdownContact(PerfectWorld::CountdownContext& Context,
    const PerfectWorld::CountdownContact& Contact, std::span<const std::string> RegisteredPeople,
    std::vector<std::string>& Diagnostics)
{
    if (std::find(RegisteredPeople.begin(), RegisteredPeople.end(), Contact.Person) == RegisteredPeople.end())
    {
        Diagnostics.push_back("unbound " + Contact.Role + ": identity is not registered: " + Contact.Person);
        return false;
    }
    Context.Contacts.push_back(Contact);
    const auto Issue = PerfectWorld::ValidateCountdownContext(Context);
    if (!Issue.Ok())
    {
        Context.Contacts.pop_back();
        Diagnostics.push_back("unbound " + Contact.Role + ": " + Issue.Detail);
        return false;
    }
    return true;
}

inline PerfectWorld::CountdownContext P315BuildCountdownContext(const PerfectWorld::LaneOriginBinding* Origin,
    std::span<const std::string> RegisteredPeople, std::vector<std::string>& Diagnostics)
{
    PerfectWorld::CountdownContext Context;
    Context.Player = "player";
    for (const auto& Person : PerfectWorld::KnownSchoolLifeCast())
        if (Person.Role == "teacher" && !Person.bProposedNewCast)
        {
            P315BindCountdownContact(Context, {"teacher", std::string(Person.PersonId)}, RegisteredPeople, Diagnostics);
            break;
        }
    if (Origin != nullptr)
    {
        const auto Origins = PerfectWorld::AuthoredLaneOrigins();
        const auto Found = std::find(Origins.begin(), Origins.end(), *Origin);
        if (Found == Origins.end())
            Diagnostics.push_back("unbound origin counterpart: no matching authored origin");
        else
        {
            // The countdown accepts fair tokens, not SchoolLife's family_adult role
            // ("family" is forbidden). Only an actual older sibling gets sibling.
            const std::string Role = Origin->GrewUpInCare ? "care_worker" :
                Origin->HouseholdTemplateId == "adult_sibling_household" ? "sibling" :
                Origin->HouseholdTemplateId == "parent_household" ? "parent" :
                Origin->HouseholdTemplateId == "grandparent_household" ? "grandparent" :
                Origin->HouseholdTemplateId == "extended_family" ? "relative" : "guardian";
            P315BindCountdownContact(Context, {Role, Origin->CounterpartPersonId}, RegisteredPeople, Diagnostics);
        }
    }
    else Diagnostics.push_back("unbound origin counterpart: no resolved authored origin");
    P315BindCountdownContact(Context, {"neighbour", std::string(PerfectWorld::FaridaPersonId)}, RegisteredPeople, Diagnostics);
    // No integrated relationship book: retain the core's absent-book behaviour.
    return Context;
}
} // namespace PWAwakening
