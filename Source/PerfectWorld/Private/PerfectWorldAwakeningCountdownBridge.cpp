// Copyright Perfect World. All Rights Reserved.

#include "PerfectWorldAwakeningResultBridge.h"
#include "WorldReducer.h" // P38

namespace PWAwakening
{
	namespace
	{
		FString R97Utf8(std::string_view Text)
		{
			const std::string Owned(Text);
			return FString(UTF8_TO_TCHAR(Owned.c_str()));
		}

		FString R97HourMinute(PerfectWorld::GameInstant Instant)
		{
			const PerfectWorld::CalendarTime Cal = PerfectWorld::GameClock::ToCalendar(Instant);
			return FString::Printf(TEXT("%02d:%02d"), Cal.Hour, Cal.Minute);
		}

		FText R97FullClock(PerfectWorld::GameInstant Instant)
		{
			const PerfectWorld::CalendarTime Cal = PerfectWorld::GameClock::ToCalendar(Instant);
			static const TCHAR* const Months[] = {
				TEXT(""), TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"),
				TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"),
				TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")
			};
			const int32 MonthIdx = FMath::Clamp(static_cast<int32>(Cal.Month), 1, 12);
			return FText::FromString(FString::Printf(
				TEXT("%d %s %04d · %02d:%02d"),
				Cal.Day, Months[MonthIdx], Cal.Year, Cal.Hour, Cal.Minute));
		}

		FText R97AvailabilityLabel(PerfectWorld::CountdownAvailability Avail, bool bCommitted)
		{
			if (bCommitted)
			{
				return FText::FromString(TEXT("Committed"));
			}
			switch (Avail)
			{
			case PerfectWorld::CountdownAvailability::Open:
				return FText::FromString(TEXT("Open (optional)"));
			case PerfectWorld::CountdownAvailability::Taken:
				return FText::FromString(TEXT("Already committed"));
			case PerfectWorld::CountdownAvailability::WindowClosed:
				return FText::FromString(TEXT("This window has already passed"));
			case PerfectWorld::CountdownAvailability::NeedsNews:
				return FText::FromString(TEXT("Opens once the news has arrived"));
			case PerfectWorld::CountdownAvailability::NoContact:
				return FText::FromString(TEXT("Needs someone you know for this session"));
			case PerfectWorld::CountdownAvailability::RelationTooLow:
				return FText::FromString(TEXT("Needs a closer relationship with the person involved"));
			case PerfectWorld::CountdownAvailability::GroupTaken:
				return FText::FromString(TEXT("Closed because another option in this window was chosen"));
			case PerfectWorld::CountdownAvailability::TimeConflict:
				return FText::FromString(TEXT("Overlaps something you have already committed to"));
			}
			return FText::FromString(TEXT("Optional practice window"));
		}

		FText R97BandLabel(PerfectWorld::CountdownBand Band)
		{
			switch (Band)
			{
			case PerfectWorld::CountdownBand::Unlikely: return FText::FromString(TEXT("unlikely"));
			case PerfectWorld::CountdownBand::Possible: return FText::FromString(TEXT("possible"));
			case PerfectWorld::CountdownBand::Likely: return FText::FromString(TEXT("likely"));
			case PerfectWorld::CountdownBand::VeryLikely: return FText::FromString(TEXT("very likely"));
			}
			return FText::FromString(TEXT("possible"));
		}

		FText R97ObligationStatus(const PerfectWorld::CountdownObligationRecord& Record)
		{
			if (Record.Done)
			{
				return FText::FromString(TEXT("Done"));
			}
			switch (Record.Disposition)
			{
			case PerfectWorld::CountdownDisposition::Standing:
				return FText::FromString(TEXT("Standing: kept on your ordinary day"));
			case PerfectWorld::CountdownDisposition::Shared:
				return FText::FromString(TEXT("Shared with a helper"));
			case PerfectWorld::CountdownDisposition::Deferred:
				return FText::FromString(TEXT("Moved to another hour"));
			case PerfectWorld::CountdownDisposition::Declined:
				return FText::FromString(TEXT("Declined"));
			}
			return FText::FromString(TEXT("Standing: kept on your ordinary day"));
		}

		FText R97ObligationTitle(const PerfectWorld::CountdownObligationRecord& Record)
		{
			const PerfectWorld::CountdownObligationDef* Def = PerfectWorld::CountdownFindObligation(Record.Id);
			return SanitizeDisplayEvidence(
				Def ? R97Utf8(Def->Name) : FString(),
				FText::FromString(TEXT("Household or school commitment")));
		}

		FText R97RoleText(const PerfectWorld::CountdownObligationDef* Def)
		{
			const FString Role = Def ? R97Utf8(Def->Role) : FString();
			if (Role.IsEmpty() || LooksLikeRawIdentifier(Role))
			{
				return FText::FromString(TEXT("Someone you know"));
			}
			return FText::FromString(FString::Printf(TEXT("Your %s"), *Role));
		}

		FString R97OfferName(const PerfectWorld::CountdownOpportunityDef* Def)
		{
			const FString Candidate = Def ? R97Utf8(Def->Name) : FString();
			if (!Candidate.IsEmpty() && !LooksLikeRawIdentifier(Candidate))
			{
				return Candidate;
			}
			return TEXT("Another optional session in the same window");
		}

		FText R97OddsSummary(const PerfectWorld::CountdownOffer& Offer)
		{
			return FText::FromString(FString::Printf(
				TEXT("Outcomes if taken: good %d%% (%s) · fair %d%% (%s) · poor %d%% (%s)"),
				Offer.Odds[0] / 10, *R97BandLabel(Offer.Bands[0]).ToString(),
				Offer.Odds[1] / 10, *R97BandLabel(Offer.Bands[1]).ToString(),
				Offer.Odds[2] / 10, *R97BandLabel(Offer.Bands[2]).ToString()));
		}
	}

	FPWCountdownModel BuildCountdownModelFromCore(
		PerfectWorld::GameInstant Now,
		const PerfectWorld::WorldState& World,
		const PerfectWorld::CountdownState* State,
		const PerfectWorld::CountdownContext& Ctx,
		bool bOrdinaryDayKept,
		const FString& CommittedOfferId,
		const FText& StatusBanner)
	{
		FPWCountdownModel Model;
		Model.bAvailable = true;
		Model.CurrentClockText = R97FullClock(Now);

		const PerfectWorld::GameInstant ExamAt = PerfectWorld::CountdownEndInstant();
		const FText ExamClock = R97FullClock(ExamAt);
		Model.ExamDeadlineText = ExamClock;

		const int64 RemainingMinutes = FMath::Max<int64>(0, ExamAt - Now);
		Model.DaysRemaining = static_cast<int32>(RemainingMinutes / PerfectWorld::MinutesPerDay);
		Model.HoursRemaining = static_cast<int32>(
			(RemainingMinutes % PerfectWorld::MinutesPerDay) / PerfectWorld::MinutesPerHour);

		if (RemainingMinutes > 0)
		{
			const TCHAR* DayWord = (Model.DaysRemaining == 1) ? TEXT("day") : TEXT("days");
			const TCHAR* HourWord = (Model.HoursRemaining == 1) ? TEXT("hour") : TEXT("hours");
			Model.CountdownHeadlineText = FText::FromString(FString::Printf(
				TEXT("%d %s, %d %s until the examination (%s)"),
				Model.DaysRemaining, DayWord, Model.HoursRemaining, HourWord, *ExamClock.ToString()));
		}
		else
		{
			Model.CountdownHeadlineText = FText::FromString(FString::Printf(
				TEXT("0 days, 0 hours: examination morning (%s)"), *ExamClock.ToString()));
		}

		Model.FreedomNoticeText = FText::FromString(
			TEXT("Every practice offer is optional. Choosing 'Keep an ordinary day' leaves your classes, household commitments, and friendships intact."));
		Model.bOrdinaryDayKept = bOrdinaryDayKept;
		Model.CommittedOfferId = CommittedOfferId;
		Model.StatusBannerText = StatusBanner;

		if (!State)
		{
			return Model;
		}

		for (const PerfectWorld::CountdownObligationRecord& Record : State->Obligations)
		{
			const PerfectWorld::CountdownObligationDef* Def = PerfectWorld::CountdownFindObligation(Record.Id);
			FPWCountdownObligationRow Row;
			Row.Id = R97Utf8(Record.Id);
			Row.Title = R97ObligationTitle(Record);
			Row.PersonText = R97RoleText(Def);
			Row.WindowText = FText::FromString(FString::Printf(
				TEXT("%s–%s"),
				*R97HourMinute(Record.Start),
				*R97HourMinute(Record.Start + Record.Minutes)));
			Row.StatusText = R97ObligationStatus(Record);
			Model.Obligations.Add(MoveTemp(Row));
		}

		const std::vector<PerfectWorld::CountdownOffer> CoreOffers =
			PerfectWorld::CountdownOffers(World, *State, Ctx);
		for (const PerfectWorld::CountdownOffer& Offer : CoreOffers)
		{
			if (!Offer.Def)
			{
				continue;
			}
			FPWCountdownOfferRow Row;
			Row.Id = R97Utf8(Offer.Def->Id);
			Row.Title = SanitizeDisplayEvidence(
				R97Utf8(Offer.Def->Name),
				FText::FromString(TEXT("Optional practice window")));

			const int32 Hours = FMath::Max(1, static_cast<int32>(Offer.Minutes / 60));
			Row.WindowText = FText::FromString(FString::Printf(
				TEXT("%s–%s (%dh)"),
				*R97HourMinute(Offer.Start),
				*R97HourMinute(Offer.End),
				Hours));

			const FString Role = R97Utf8(Offer.Def->Role);
			if (Offer.Contact.empty() || Role.IsEmpty() || LooksLikeRawIdentifier(Role))
			{
				Row.ContactText = FText::FromString(TEXT("Self-directed practice window"));
			}
			else
			{
				Row.ContactText = FText::FromString(FString::Printf(TEXT("With your %s"), *Role));
			}

			Row.SummaryText = R97OddsSummary(Offer);

			const bool bTaken = (PerfectWorld::CountdownFindCommitment(*State, Offer.Def->Id) != nullptr)
				|| (Offer.Availability == PerfectWorld::CountdownAvailability::Taken);
			Row.bCommitted = bTaken || (!CommittedOfferId.IsEmpty() && Row.Id == CommittedOfferId);
			Row.bOpen = (Offer.Availability == PerfectWorld::CountdownAvailability::Open)
				&& !bOrdinaryDayKept
				&& (CommittedOfferId.IsEmpty() || Row.Id == CommittedOfferId);
			Row.AvailabilityText = R97AvailabilityLabel(Offer.Availability, Row.bCommitted);

			for (const PerfectWorld::CountdownObligationRecord& Record : State->Obligations)
			{
				if (Record.Done || Record.Minutes <= 0)
				{
					continue;
				}
				const PerfectWorld::GameInstant OblEnd = Record.Start + Record.Minutes;
				if (Record.Start < Offer.End && Offer.Start < OblEnd)
				{
					Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
						TEXT("Overlaps a standing commitment: %s (%s–%s)"),
						*R97ObligationTitle(Record).ToString(),
						*R97HourMinute(Record.Start),
						*R97HourMinute(OblEnd))));
				}
			}

			for (const std::string& ClosedId : Offer.Closes)
			{
				Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
					TEXT("Closes another option in this window: %s"),
					*R97OfferName(PerfectWorld::CountdownFindOpportunity(ClosedId)))));
			}

			if (Row.DisplacedCommitments.Num() == 0)
			{
				Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
					TEXT("Sets aside %s–%s of your own time; nothing else you have committed to falls in it"),
					*R97HourMinute(Offer.Start),
					*R97HourMinute(Offer.End))));
			}

			Model.Offers.Add(MoveTemp(Row));
		}

		return Model;
	}

	FPWCountdownModel P38BuildLiveCountdownModel(const PerfectWorld::WorldSim& Sim, const bool bOrdinaryDayKept,
		const FText& StatusBanner, TArray<FP38PracticeDisclosure>& Disclosures)
	{
		const auto State = Sim.Countdown();
		auto Model = BuildCountdownModelFromCore(Sim.State().Clock.Now(), Sim.State(), State ? &*State : nullptr,
			Sim.AwakeningContent().Countdown, bOrdinaryDayKept, FString(), StatusBanner);
		Disclosures.Reset();
		// P38: authored TakeOpportunity rows have no WorldSim command yet. Only expose the routed practice command.
		Model.Offers.Reset();
		if (!State) return Model;
		for (int32 Index = 0; Index < PerfectWorld::PowerDimensionCount; ++Index)
		{
			const auto Dimension = static_cast<PerfectWorld::PowerDimension>(Index);
			const auto Offer = Sim.PreviewCountdownPractice(Dimension, 30);
			FPWCountdownOfferRow Row;
			Row.Id = FString::Printf(TEXT("p38.practice.%d"), Index);
			Row.Title = FText::Format(NSLOCTEXT("P38Awakening", "PracticeTitle", "Optional {0} practice"), CapabilityDisplayName(Dimension));
			Row.WindowText = FText::FromString(FString::Printf(TEXT("%s - %s (%d minutes)"),
				*R97HourMinute(Offer.Start), *R97HourMinute(Offer.End), Offer.Minutes));
			Row.ContactText = Offer.Supervisor.empty() ? FText::FromString(TEXT("No supervising teacher is available for this session"))
				: FText::FromString(TEXT("Supervised by your school teacher"));
			Row.bOpen = Offer.Available() && !bOrdinaryDayKept;
			Row.AvailabilityText = Row.bOpen ? FText::FromString(TEXT("Optional; confirm only after reading the costs"))
				: SanitizeDisplayEvidence(R97Utf8(Offer.Issue.Detail), FText::FromString(TEXT("This practice is unavailable right now")));
			Row.SummaryText = FText::FromString(TEXT("Trains the live profile; your original Awakening result stays as recorded."));
			for (const auto& Displaced : Offer.Displaced)
				Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(TEXT("Sets aside %d minutes of %s"),
					Displaced.OverlapMinutes, *SanitizeDisplayEvidence(R97Utf8(Displaced.Id),
						FText::FromString(TEXT("a known activity"))).ToString())));
			for (const auto& Block : Offer.BlockingCommitments)
				Row.DisplacedCommitments.Add(SanitizeDisplayEvidence(R97Utf8(Block), FText::FromString(TEXT("A prior commitment blocks this session"))));
			if (Row.DisplacedCommitments.Num() == 0)
				Row.DisplacedCommitments.Add(FText::FromString(TEXT("No displaced commitments reported by the core.")));
			FP38PracticeDisclosure Disclosure;
			Disclosure.Id = Row.Id;
			Disclosure.Offer = Offer;
			Disclosures.Add(MoveTemp(Disclosure));
			Model.Offers.Add(MoveTemp(Row));
		}
		return Model;
	}

	FApFakeAwakeningWorld::FApFakeAwakeningWorld()
	{
		SeedSampleGrantedAwakening();
		SeedSampleCountdownDay();
	}

	void FApFakeAwakeningWorld::SetResultModel(const FPWAwakeningResultModel& InModel)
	{
		ResultModel = InModel;
	}

	void FApFakeAwakeningWorld::SetCountdownModel(const FPWCountdownModel& InModel)
	{
		CountdownModel = InModel;
	}

	void FApFakeAwakeningWorld::LoadFromCoreGrant(
		const PerfectWorld::AwakeningGrant& Grant,
		const PerfectWorld::AbilityRecipeSet* Recipes,
		bool bUnseen)
	{
		ResultModel = BuildResultModelFromGrant(Grant, Recipes, bUnseen);
	}

	void FApFakeAwakeningWorld::LoadFromCoreCountdown(
		PerfectWorld::GameInstant Now,
		const PerfectWorld::WorldState& World,
		const PerfectWorld::CountdownState& State,
		const PerfectWorld::CountdownContext& Ctx)
	{
		CountdownModel = BuildCountdownModelFromCore(Now, World, &State, Ctx);
	}

	void FApFakeAwakeningWorld::SeedSampleGrantedAwakening()
	{
		// A hand-built grant using the real result types; no resolver is run, so the sample is
		// independent of the content registry.
		PerfectWorld::AwakeningGrant Grant;
		Grant.At = PerfectWorld::GameClock::FromCalendar(2043, 1, 23, 15, 0);

		PerfectWorld::AwakeningResult& Result = Grant.Result;
		Result.ConductObserved = true;
		const int32 Percent[PerfectWorld::TattvaCount] = {30, 20, 20, 15, 15};
		for (int32 i = 0; i < PerfectWorld::TattvaCount; ++i)
		{
			Result.TattvaPercent[i] = Percent[i];
			Result.TattvaMilli[i] = Percent[i] * 1000;

			PerfectWorld::AwakeningCause Cause;
			Cause.Signal = PerfectWorld::AwakeningSignal::Care;
			Cause.Reading = 700;
			Cause.Contribution = 100 + i;
			Cause.Source = "conduct.care";
			Cause.TestId = "crossing_span";
			Cause.ChoiceId = "You steadied the crossing so the student beside you could pass.";
			Result.TattvaCauses[i].push_back(Cause);
		}
		const int32 Latent[PerfectWorld::PowerDimensionCount] = {14, 18, 9, 22, 20, 16};
		for (int32 d = 0; d < PerfectWorld::PowerDimensionCount; ++d)
		{
			Result.LatentRating[d] = Latent[d];
			Result.ConductRating[d] = 10;
			Result.Rating[d] = Latent[d] + 10;

			PerfectWorld::AwakeningCause Cause;
			Cause.Signal = PerfectWorld::AwakeningSignal::Attentiveness;
			Cause.Reading = 640;
			Cause.Contribution = 90 + d;
			Cause.Source = "conduct.attentiveness";
			Cause.TestId = "lintel_seam";
			Cause.ChoiceId = "You traced the seam along the tutorial pillar before stepping.";
			Result.RatingCauses[d].push_back(Cause);
		}
		Result.Ability.RecipeId = "settled_stance";
		Result.Ability.Fallback = true;
		Result.Ability.Rank = PerfectWorld::AwakeningAbilityRank;
		Result.Ability.Causes = Result.TattvaCauses[0];

		Grant.StartingProfile = PerfectWorld::Awaken(
			Result.TattvaPercent, Result.Rating, PerfectWorld::GameClock::DayNumber(Grant.At))
			.value_or(PerfectWorld::AwakenedProfile{});

		ResultModel = BuildResultModelFromGrant(Grant, &DefaultAbilityRecipeCatalogue(), true);
	}

	void FApFakeAwakeningWorld::SeedSampleCountdownDay()
	{
		// Hand-authored sample rows (the fake never invents core data; the live builder above is
		// the only path from CountdownState).
		const PerfectWorld::GameInstant Now = PerfectWorld::GameClock::FromCalendar(2043, 1, 24, 8, 0);
		CountdownModel = BuildCountdownModelFromCore(
			Now, PerfectWorld::WorldState{}, nullptr, PerfectWorld::CountdownContext{});

		FPWCountdownObligationRow Obl;
		Obl.Id = TEXT("sample.obligation");
		Obl.Title = FText::FromString(TEXT("Evening tea and market list at home"));
		Obl.PersonText = FText::FromString(TEXT("Your aunt"));
		Obl.WindowText = FText::FromString(TEXT("17:00–18:30"));
		Obl.StatusText = FText::FromString(TEXT("Standing: kept on your ordinary day"));
		CountdownModel.Obligations.Add(Obl);

		FPWCountdownOfferRow Offer0;
		Offer0.Id = TEXT("sample.offer_footing");
		Offer0.Title = FText::FromString(TEXT("Courtyard footing drill"));
		Offer0.WindowText = FText::FromString(TEXT("16:30–18:30 (2h)"));
		Offer0.ContactText = FText::FromString(TEXT("With your teacher"));
		Offer0.SummaryText = FText::FromString(
			TEXT("Outcomes if taken: good 40% (possible) · fair 40% (possible) · poor 20% (unlikely)"));
		Offer0.AvailabilityText = FText::FromString(TEXT("Open (optional)"));
		Offer0.DisplacedCommitments.Add(FText::FromString(
			TEXT("Overlaps a standing commitment: Evening tea and market list at home (17:00–18:30)")));
		Offer0.DisplacedCommitments.Add(FText::FromString(
			TEXT("Closes another option in this window: Reading room cadence study")));
		CountdownModel.Offers.Add(Offer0);

		FPWCountdownOfferRow Offer1;
		Offer1.Id = TEXT("sample.offer_cadence");
		Offer1.Title = FText::FromString(TEXT("Reading room cadence study"));
		Offer1.WindowText = FText::FromString(TEXT("16:30–18:30 (2h)"));
		Offer1.ContactText = FText::FromString(TEXT("With your friend"));
		Offer1.SummaryText = FText::FromString(
			TEXT("Outcomes if taken: good 30% (possible) · fair 50% (possible) · poor 20% (unlikely)"));
		Offer1.AvailabilityText = FText::FromString(TEXT("Open (optional)"));
		Offer1.DisplacedCommitments.Add(FText::FromString(
			TEXT("Closes another option in this window: Courtyard footing drill")));
		CountdownModel.Offers.Add(Offer1);
	}

	FPWAwakeningResultModel FApFakeAwakeningWorld::ReadResultModel()
	{
		return ResultModel;
	}

	bool FApFakeAwakeningWorld::HasUnseenResult() const
	{
		return ResultModel.bAvailable && ResultModel.bGranted && ResultModel.bUnseen;
	}

	void FApFakeAwakeningWorld::AcknowledgeResult()
	{
		++AcknowledgeCount;
		ResultModel.bUnseen = false;
	}

	FPWCountdownModel FApFakeAwakeningWorld::ReadCountdownModel()
	{
		return CountdownModel;
	}

	bool FApFakeAwakeningWorld::ConfirmPracticeOffer(const FString& OfferId, FText& OutNotice)
	{
		for (int32 i = 0; i < CountdownModel.Offers.Num(); ++i)
		{
			FPWCountdownOfferRow& Row = CountdownModel.Offers[i];
			if (Row.Id == OfferId)
			{
				if (!Row.bOpen && !Row.bCommitted)
				{
					OutNotice = FText::FromString(TEXT("That optional practice window is not open right now."));
					return false;
				}
				++ConfirmOfferCount;
				LastConfirmedOfferId = OfferId;
				CountdownModel.bOrdinaryDayKept = false;
				CountdownModel.CommittedOfferId = OfferId;
				for (int32 j = 0; j < CountdownModel.Offers.Num(); ++j)
				{
					CountdownModel.Offers[j].bCommitted = (j == i);
					CountdownModel.Offers[j].bOpen = (j == i);
					CountdownModel.Offers[j].AvailabilityText = (j == i)
						? FText::FromString(TEXT("Committed"))
						: FText::FromString(TEXT("Closed because another option in this window was chosen"));
				}
				CountdownModel.StatusBannerText = FText::FromString(FString::Printf(
					TEXT("Committed to %s. You can still keep an ordinary day tomorrow."),
					*Row.Title.ToString()));
				OutNotice = CountdownModel.StatusBannerText;
				return true;
			}
		}
		OutNotice = FText::FromString(TEXT("Select an available practice window before confirming."));
		return false;
	}

	bool FApFakeAwakeningWorld::KeepOrdinaryDay(FText& OutNotice)
	{
		++KeepOrdinaryDayCount;
		CountdownModel.bOrdinaryDayKept = true;
		CountdownModel.CommittedOfferId.Reset();
		for (FPWCountdownOfferRow& Row : CountdownModel.Offers)
		{
			Row.bCommitted = false;
		}
		CountdownModel.StatusBannerText = FText::FromString(
			TEXT("You kept an ordinary day. Your classes, household commitments, and friendships remain complete."));
		OutNotice = CountdownModel.StatusBannerText;
		return true;
	}

	TSharedRef<FApFakeAwakeningWorld> MakeFakeAwakeningWorld()
	{
		return MakeShared<FApFakeAwakeningWorld>();
	}
}
