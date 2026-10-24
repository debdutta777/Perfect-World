#include "LaneOrigin.h"

#include <array>
#include <cstdlib>
#include <iostream>

using namespace PerfectWorld;

namespace
{
int Assertions = 0;
void Expect(bool condition, const char* message)
{
    ++Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

OpeningHomePlan Plan(std::string_view household, std::int32_t rank)
{
    CharacterCreator creator;
    creator.Reset();
    Expect(creator.SetTraitRank(HouseholdResourcesTraitId, 0).Succeeded(), "release resources budget");
    Expect(creator.SetHouseholdTemplate(household).Succeeded(), "select template");
    // Reset all ranks to make every resources rank affordable without duplicating creation costs.
    for (const auto& trait : CharacterCreator::TraitDefinitions())
        Expect(creator.SetTraitRank(trait.Id, 0).Succeeded(), "release trait budget");
    Expect(creator.SetTraitRank(HouseholdResourcesTraitId, rank).Succeeded(), "select resource rank");
    return creator.ResolveOpeningHome();
}

void OriginsAndPermissions()
{
    const std::array<std::string_view, 5> labels{
        "Your parent", "Your relative", "Your grandparent", "Your older sibling", "Your guardian"};
    std::size_t index = 0;
    const auto care = ResolveLaneOrigin(Plan(DefaultHouseholdTemplateId, 0));
    Expect(care.Ok(), "care resolves");
    for (const auto& household : CharacterCreator::HouseholdDefinitions())
    {
        const auto family = ResolveLaneOrigin(Plan(household.Id, 1));
        for (std::int32_t rank = 0; rank <= MaxRank; ++rank)
        {
            const auto plan = Plan(household.Id, rank);
            const auto result = ResolveLaneOrigin(plan);
            Expect(result.Ok(), "all 25 origins resolve");
            Expect(result.Binding == ResolveLaneOrigin(plan).Binding, "repeatable resolution");
            Expect(result.Binding.HomeKitId == plan.HomeKitId, "consume creation home kit");
            Expect(result.Binding.GrewUpInCare == plan.GrewUpInCare, "consume care decision");
            Expect(result.Binding == (rank == 0 ? care.Binding : family.Binding), "care overrides all templates");
            Expect(result.Binding.RelationshipLabel == (rank == 0 ? "Care worker" : labels[index]), "appropriate relationship");
            Expect(HasLaneOriginResidency(result.Binding, result.Binding.LocationId), "resident in own home");
            Expect(!HasLaneOriginResidency(result.Binding, "location.nabapara.shanti_chai"), "no staff permissions");
            Expect(!HasLaneOriginResidency(result.Binding, ""), "no empty location permission");
            for (const auto& other : AuthoredLaneOrigins())
                Expect(HasLaneOriginResidency(result.Binding, other.LocationId) ==
                       (result.Binding.LocationId == other.LocationId), "residency isolated per origin");
            if (rank == 0)
                Expect(!HasLaneOriginResidency(result.Binding, "location.nabapara.opening_home"), "care has no family residency");
        }
        ++index;
    }
    const auto parent = ResolveLaneOrigin(Plan(DefaultHouseholdTemplateId, 2));
    Expect(parent.Binding.CounterpartPersonId == HouseholdAdultId, "preserve household person ID");
    Expect(parent.Binding.LocationId == "location.nabapara.opening_home", "preserve opening home ID");
}

void InvalidInputs()
{
    const auto plan = Plan(DefaultHouseholdTemplateId, 2);
    for (const std::int32_t rank : {-1, 5})
    {
        auto invalid = plan;
        invalid.ResourceRank = rank;
        Expect(ResolveLaneOrigin(invalid).Error == LaneOriginError::InvalidPlan, "invalid rank refused");
    }
    auto invalid = plan;
    invalid.HouseholdTemplateId = "missing";
    Expect(ResolveLaneOrigin(invalid).Error == LaneOriginError::InvalidPlan, "unknown template refused");
    invalid = plan;
    invalid.GrewUpInCare = true;
    Expect(ResolveLaneOrigin(invalid).Error == LaneOriginError::InvalidPlan, "inconsistent care refused");
    invalid = plan;
    invalid.HomeKitId = InstitutionalHomeKitId;
    Expect(ResolveLaneOrigin(invalid).Error == LaneOriginError::InvalidPlan, "wrong home kit refused");
    invalid = Plan(DefaultHouseholdTemplateId, 0);
    invalid.GrewUpInCare = false;
    Expect(!ResolveLaneOrigin(invalid).Ok(), "rank zero cannot acquire family binding");
    Expect(!ResolveLaneOrigin(OpeningHomePlan{}).Ok(), "empty plan refused");
    const auto missing = ResolveLaneOrigin(plan, {});
    Expect(missing.Error == LaneOriginError::MissingBinding && missing.Binding.LocationId.empty(), "missing binding explicit and empty");
    auto rows = std::vector<LaneOriginBinding>(AuthoredLaneOrigins().begin(), AuthoredLaneOrigins().end());
    rows.push_back(rows.front());
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::DuplicateBinding, "duplicate binding refused");
    rows.pop_back();
    rows.front().ResidentLocationIds.push_back(rows.back().LocationId);
    const auto malformed = ResolveLaneOrigin(plan, rows);
    Expect(malformed.Error == LaneOriginError::InvalidBinding && malformed.Binding.LocationId.empty(), "permission contamination refused");
    Expect(!HasLaneOriginResidency(rows.front(), rows.front().LocationId), "malformed binding grants nothing");
    rows.front() = AuthoredLaneOrigins().front();
    rows.front().ScheduleId.clear();
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::InvalidBinding, "missing schedule refused");
    rows.front() = AuthoredLaneOrigins().front();
    rows.front().RelationshipLabel = "Invented label";
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::InvalidBinding, "altered relationship refused");
    rows.front() = AuthoredLaneOrigins().front();
    const auto owned = ResolveLaneOrigin(plan, rows);
    rows.front().LocationId.clear();
    Expect(owned.Ok() && owned.Binding == AuthoredLaneOrigins().front(), "result owns catalogue values");
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::InvalidBinding, "missing location refused");
    rows.front() = AuthoredLaneOrigins().front();
    rows.front().BuildingId.clear();
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::InvalidBinding, "missing building refused");
    rows.front() = AuthoredLaneOrigins().front();
    rows.front().CounterpartPersonId.clear();
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::InvalidBinding, "missing person refused");
    rows.erase(rows.begin());
    Expect(ResolveLaneOrigin(plan, rows).Error == LaneOriginError::MissingBinding, "no fallback to another family");
    rows.pop_back();
    Expect(ResolveLaneOrigin(Plan(DefaultHouseholdTemplateId, 0), rows).Error == LaneOriginError::MissingBinding,
           "no fallback from missing care to family");
}

void ScheduleAvailability()
{
    const auto binding = ResolveLaneOrigin(Plan(DefaultHouseholdTemplateId, 0)).Binding;
    PersonSchedule person;
    person.PersonId = binding.CounterpartPersonId;
    person.HomeZone = binding.LocationId;
    person.Routine = {
        {ScheduleAllWeekdays, 0, 360, binding.LocationId, ScheduleDoingKind::Role, "sleeping", 0},
        {ScheduleAllWeekdays, 360, 1080, binding.LocationId, ScheduleDoingKind::Role, "on_duty", 0},
        {ScheduleAllWeekdays, 1080, 1440, binding.LocationId, ScheduleDoingKind::Role, "off_duty", 0}};
    const ScheduleTravelTable travel;
    const auto availability = [&](GameInstant at) {
        return ResolveLaneOriginAvailability(binding, binding.ScheduleId, person, travel, at);
    };
    for (GameInstant day = 0; day < 8; ++day)
    {
        Expect(availability(day * MinutesPerDay).Availability == PersonAvailability::Awake, "duty begins from schedule at epoch 06:00");
        Expect(availability(day * MinutesPerDay + 719).Availability == PersonAvailability::Awake, "duty end exclusive");
        Expect(availability(day * MinutesPerDay + 720).Availability == PersonAvailability::Busy, "off duty at boundary");
        Expect(availability(day * MinutesPerDay + 1080).Availability == PersonAvailability::Asleep, "sleep from schedule");
    }
    person.Exceptions = {{"duty_change", 0, 30, binding.LocationId, ScheduleDoingKind::Role, "off_duty", 10}};
    Expect(availability(0).Ok() && availability(0).Availability == PersonAvailability::Busy, "exception changes duty without another clock");
    Expect(availability(30).Availability == PersonAvailability::Awake, "exception end resumes schedule");
    person.Exceptions.front().Zone = "location.other";
    Expect(availability(0).Error == LaneOriginError::InvalidSchedule, "missing travel leg refused");
    person.Exceptions = {{"visit", 60, 120, "location.other", ScheduleDoingKind::Role, "on_duty", 10}};
    const ScheduleTravelTable withTravel{{{binding.LocationId, "location.other", 10}}};
    const auto visiting = [&](GameInstant at) {
        return ResolveLaneOriginAvailability(binding, binding.ScheduleId, person, withTravel, at);
    };
    Expect(visiting(55).Ok() && visiting(55).Availability == PersonAvailability::Busy, "travel is unavailable");
    Expect(visiting(70).Ok() && visiting(70).Availability == PersonAvailability::Busy, "duty at another place is unavailable");
    person.Exceptions.clear();
    Expect(ResolveLaneOriginAvailability(binding, "wrong", person, travel, 0).Error == LaneOriginError::ScheduleMismatch, "wrong schedule ID refused");
    person.PersonId = HouseholdAdultId;
    Expect(availability(0).Error == LaneOriginError::ScheduleMismatch, "wrong counterpart refused");
    person.PersonId = binding.CounterpartPersonId;
    Expect(availability(-1).Error == LaneOriginError::InvalidTime, "negative instant refused");
    Expect(availability(ScheduleMaxInstant + 1).Error == LaneOriginError::InvalidTime, "excess instant refused");
    person.Routine.pop_back();
    Expect(availability(0).Error == LaneOriginError::InvalidSchedule, "incomplete week refused");
}
} // namespace

int main()
{
    OriginsAndPermissions();
    InvalidInputs();
    ScheduleAvailability();
    std::cout << "PASS: " << Assertions << " lane origin assertions\n";
}
