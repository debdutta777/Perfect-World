#include "PerfectWorldAwakeningResultBridge.h"

#include "LaneJournal.h"
#include "LaneOrigin.h"
#include "SchoolLifeContent.h"
#include "WorldReducer.h"

namespace PWAwakening
{
namespace
{
FText P312Person(std::string_view Id, std::string_view Role)
{
    if (const auto* Cast = PerfectWorld::FindSchoolLifeCast(Id))
        return FText::FromString(UTF8_TO_TCHAR(std::string(Cast->DisplayName).c_str()));
    const auto Name = PerfectWorld::LaneJournalDisplayName(Id);
    if (!Name.empty()) return FText::FromString(UTF8_TO_TCHAR(std::string(Name).c_str()));
    for (const auto& Origin : PerfectWorld::AuthoredLaneOrigins())
        if (Origin.CounterpartPersonId == Id) return FText::FromString(UTF8_TO_TCHAR(Origin.RelationshipLabel.c_str()));
    if (Role == "teacher") return FText::FromString(TEXT("Your teacher"));
    if (Role == "neighbour") return FText::FromString(TEXT("Your neighbour"));
    if (Role == "sibling") return FText::FromString(TEXT("Your sibling"));
    if (Role == "care_adult") return FText::FromString(TEXT("Your care worker"));
    if (Role == "family_adult") return FText::FromString(TEXT("Your household adult"));
    return FText::FromString(TEXT("Someone you know"));
}

FString P312At(PerfectWorld::GameInstant At)
{
    const auto Cal = PerfectWorld::GameClock::ToCalendar(At);
    return FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d"), Cal.Day, Cal.Month, Cal.Year, Cal.Hour, Cal.Minute);
}

FString P312Effect(const PerfectWorld::CountdownEffect& Effect)
{
    FString Text;
    for (std::size_t Index = 0; Index < PerfectWorld::CountdownDimensionCount; ++Index)
    {
        if (Effect.Preparation[Index] == 0) continue;
        const auto Name = PerfectWorld::CountdownDimensionName(static_cast<PerfectWorld::CountdownDimension>(Index));
        Text += FString::Printf(TEXT("%s %+d; "), UTF8_TO_TCHAR(std::string(Name).c_str()), Effect.Preparation[Index]);
    }
    return Text + FString::Printf(TEXT("composure %+d; goodwill %+d; route delay reduced %d min"),
        Effect.Composure, Effect.Goodwill, Effect.RouteMinutes);
}
}

void P312PopulateCountdownModel(const PerfectWorld::WorldSim& Sim, FPWCountdownModel& Model)
{
    const auto State = Sim.Countdown();
    const auto View = Sim.CountdownView();
    const auto& Context = Sim.AwakeningContent().Countdown;
    Model.bAvailable = State.has_value() && View.Issue.Ok();
    Model.bP312Live = true;
    Model.P312WorldSeq = Sim.State().Seq;
    Model.bP312CanResolve = Model.bAvailable && State && !State->Closed && View.RemainingMinutes > 0;
    for (const auto& Contact : Context.Contacts)
        Model.P312Contacts.Add(P312Person(Contact.Person, Contact.Role));
    if (!State) return;

    const auto Offers = PerfectWorld::CountdownOffers(Sim.State(), *State, Context);
    for (auto& Row : Model.Offers)
    {
        for (const auto& Offer : Offers)
        {
            if (!Offer.Def || Row.Id != UTF8_TO_TCHAR(std::string(Offer.Def->Id).c_str())) continue;
            if (!Offer.Contact.empty()) Row.ContactText = P312Person(Offer.Contact, Offer.Def->Role);
            Row.bOpen = Row.bOpen && Model.bP312CanResolve;
            const auto* Taken = PerfectWorld::CountdownFindCommitment(*State, Offer.Def->Id);
            if (Taken && Taken->Resolved)
                Row.AvailabilityText = FText::FromString(FString::Printf(TEXT("Completed: %s outcome"),
                    UTF8_TO_TCHAR(std::string(PerfectWorld::CountdownOutcomeName(Taken->Outcome)).c_str())));
            for (std::size_t Index = 0; Index < PerfectWorld::CountdownOutcomeCount; ++Index)
            {
                const auto Outcome = PerfectWorld::CountdownOutcomeName(static_cast<PerfectWorld::CountdownOutcome>(Index));
                Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(TEXT("%s (%d/1000): %s"),
                    UTF8_TO_TCHAR(std::string(Outcome).c_str()), Offer.Odds[Index], *P312Effect(Offer.Def->Effects[Index]))));
            }
        }
    }
    for (auto& Row : Model.Obligations)
    {
        const std::string Id(TCHAR_TO_UTF8(*Row.Id));
        const auto* Record = PerfectWorld::CountdownFindRecord(*State, Id);
        const auto* Def = PerfectWorld::CountdownFindObligation(Id);
        if (!Record || !Def) continue;
        Row.PersonText = P312Person(Record->Person, Def->Role);
        if (Record->Negotiated)
            Row.StatusText = FText::Format(FText::FromString(TEXT("{0}. Your {1} proposal was {2}.")), Row.StatusText,
                FText::FromString(UTF8_TO_TCHAR(std::string(PerfectWorld::CountdownProposalName(Record->Attempt)).c_str())),
                FText::FromString(Record->Accepted ? TEXT("accepted") : TEXT("refused")));
        for (const auto& Offer : PerfectWorld::CountdownProposals(Sim.State(), *State, Context, Id))
        {
            FP312CountdownProposalRow Proposal;
            Proposal.Proposal = Offer.Proposal;
            Proposal.bAvailable = Offer.Available && !Record->Done && Model.bP312CanResolve;
            const auto Name = PerfectWorld::CountdownProposalName(Offer.Proposal);
            if (Offer.Proposal == PerfectWorld::CountdownProposal::Honour)
                Proposal.Disclosure = FText::FromString(TEXT("Honour: leave this promise standing and use Resolve this day on its day."));
            else
                Proposal.Disclosure = FText::FromString(FString::Printf(
                    TEXT("%s: %d minutes at %s; acceptance %d/1000; goodwill accepted %+d / refused %+d. %s"),
                    UTF8_TO_TCHAR(std::string(Name).c_str()), Offer.MinutesIfAccepted, *P312At(Offer.StartIfAccepted),
                    Offer.AcceptPermille, Offer.GoodwillIfAccepted, Offer.GoodwillIfRefused,
                    Proposal.bAvailable ? TEXT("One attempt.") : TEXT("Unavailable.")));
            if (!Offer.Helper.empty())
                Proposal.Disclosure = FText::Format(FText::FromString(TEXT("{0} Helper: {1}.")),
                    Proposal.Disclosure, P312Person(Offer.Helper, "friend"));
            Row.Proposals.Add(MoveTemp(Proposal));
        }
    }

    const auto News = PerfectWorld::CountdownNewsOf(Sim.State(), *State, Context);
    if (News.Fired)
        Model.P312NewsText = FText::FromString(FString::Printf(TEXT("%s. Examination journey delay: %d minutes (originally %d). Response opportunities are listed below."),
            UTF8_TO_TCHAR(std::string(PerfectWorld::CountdownWorldEvent().Name).c_str()), News.DelayNow, News.DelayUnmitigated));
    if (!Model.bP312CanResolve)
    {
        Model.P312PlanText = FText::FromString(TEXT("The countdown is closed. Examination morning has arrived; return to play."));
        return;
    }

    // A disclosed, one-day routine. The contract inserts promises and opportunities; UI adds no overlapping wait.
    const auto Midnight = PerfectWorld::GameClock::StartOfDay(Sim.State().Clock.Now());
    const auto DayEnd = Midnight + PerfectWorld::MinutesPerDay;
    PerfectWorld::WeekDayPlan Day;
    bool bReservedToMidnight = false;
    for (const auto& Block : PerfectWorld::CountdownReservedBlocks(*State))
        if (Block.Start < DayEnd && Block.End == DayEnd) bReservedToMidnight = true;
    if (!bReservedToMidnight)
    {
        PerfectWorld::WeekEntry Rest;
        Rest.Kind = PerfectWorld::WeekEntryKind::Spend;
        Rest.StartMinute = 1439;
        Rest.Minutes = 1;
        Rest.Pass = PerfectWorld::DayPassKind::Rest;
        Day.Entries.push_back(Rest);
    }
    Model.P312Days.push_back(Day);
    Model.P312PlanText = FText::FromString(FString::Printf(
        TEXT("Resolve the remainder of %s: perform the standing promises and taken opportunities at their listed times. Gaps are spent awake waiting; %s. This consumes the remaining day. %s"),
        *P312At(Midnight), bReservedToMidnight ? TEXT("the final promise reaches midnight") : TEXT("rest for one minute at 23:59"),
        DayEnd >= PerfectWorld::GameClock::StartOfDay(State->End)
            ? TEXT("The core also spends the final hours awake through 2 February 2043 06:00, closing the countdown.")
            : TEXT("News pauses the routine after its day, so you can choose a response before continuing.")));
}
}
