// Copyright Perfect World. All Rights Reserved.

#include "PerfectWorldAwakeningResultBridge.h"

namespace PWAwakening
{
	namespace
	{
		FString ApFormatHourMinute(PerfectWorld::GameInstant Instant)
		{
			const PerfectWorld::CalendarTime Cal = PerfectWorld::GameClock::ToCalendar(Instant);
			return FString::Printf(TEXT("%02d:%02d"), Cal.Hour, Cal.Minute);
		}

		FText ApFormatFullClock(PerfectWorld::GameInstant Instant)
		{
			const PerfectWorld::CalendarTime Cal = PerfectWorld::GameClock::ToCalendar(Instant);
			static const TCHAR* kMonths[] = {
				TEXT(""), TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"),
				TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"),
				TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")
			};
			const int32 MonthIdx = FMath::Clamp(Cal.Month, 1, 12);
			return FText::FromString(FString::Printf(
				TEXT("%d %s %04d · %02d:%02d"),
				Cal.Day, kMonths[MonthIdx], Cal.Year, Cal.Hour, Cal.Minute));
		}

		FText ApAvailabilityLabel(PerfectWorld::OpportunityAvailability Avail, bool bCommitted)
		{
			if (bCommitted || Avail == PerfectWorld::OpportunityAvailability::Taken)
			{
				return FText::FromString(TEXT("Committed for today"));
			}
			switch (Avail)
			{
			case PerfectWorld::OpportunityAvailability::Open:
				return FText::FromString(TEXT("Open today (optional)"));
			case PerfectWorld::OpportunityAvailability::ClosedByGroup:
				return FText::FromString(TEXT("Closed because another option in this window was chosen"));
			case PerfectWorld::OpportunityAvailability::PrerequisitesUnmet:
				return FText::FromString(TEXT("Requires prior arrangement before opening"));
			case PerfectWorld::OpportunityAvailability::OutsideWindow:
				return FText::FromString(TEXT("Outside today's active hour window"));
			case PerfectWorld::OpportunityAvailability::Completed:
				return FText::FromString(TEXT("Completed earlier in the countdown"));
			case PerfectWorld::OpportunityAvailability::Expired:
				return FText::FromString(TEXT("Window has already passed"));
			case PerfectWorld::OpportunityAvailability::Taken:
				return FText::FromString(TEXT("Committed for today"));
			}
			return FText::FromString(TEXT("Optional practice window"));
		}

		FText ApObligationStatusLabel(PerfectWorld::ObligationDisposition Disp)
		{
			switch (Disp)
			{
			case PerfectWorld::ObligationDisposition::Pending:
				return FText::FromString(TEXT("Scheduled on your ordinary day"));
			case PerfectWorld::ObligationDisposition::Honored:
				return FText::FromString(TEXT("Kept as promised"));
			case PerfectWorld::ObligationDisposition::Rescheduled:
				return FText::FromString(TEXT("Moved to another hour"));
			case PerfectWorld::ObligationDisposition::Missed:
				return FText::FromString(TEXT("Missed during another commitment"));
			}
			return FText::FromString(TEXT("Scheduled on your ordinary day"));
		}

		FString ApLookupOpportunityDisplayName(
			const PerfectWorld::CountdownCatalogue& Catalog,
			const std::string& Id)
		{
			if (const PerfectWorld::CountdownOpportunityDef* Def = Catalog.FindOpportunity(Id))
			{
				const FString Candidate = UTF8_TO_TCHAR(Def->Name.c_str());
				if (!LooksLikeRawIdentifier(Candidate))
				{
					return Candidate;
				}
			}
			return TEXT("Another optional session in the same hour window");
		}
	}

	FPWCountdownModel BuildCountdownModelFromCore(
		PerfectWorld::GameInstant Now,
		const PerfectWorld::WorldState& World,
		const PerfectWorld::CountdownState& State,
		const PerfectWorld::CountdownContext& Ctx,
		bool bOrdinaryDayKept,
		const FString& CommittedOfferId,
		const FText& StatusBanner)
	{
		FPWCountdownModel Model;
		Model.bAvailable = true;
		Model.CurrentClockText = ApFormatFullClock(Now);
		Model.ExamDeadlineText = FText::FromString(TEXT("2 February 2043 · 06:00"));

		const PerfectWorld::GameInstant ExamAt = PerfectWorld::CountdownEndInstant();
		const int64_t RemainingMinutes = FMath::Max<int64_t>(0, ExamAt - Now);
		Model.DaysRemaining = static_cast<int32>(RemainingMinutes / PerfectWorld::MinutesPerDay);
		Model.HoursRemaining = static_cast<int32>(
			(RemainingMinutes % PerfectWorld::MinutesPerDay) / PerfectWorld::MinutesPerHour);

		if (RemainingMinutes > 0)
		{
			Model.CountdownHeadlineText = FText::FromString(FString::Printf(
				TEXT("%d %s, %d %s until the examination (2 February 2043 · 06:00)"),
				Model.DaysRemaining,
				Model.DaysRemaining == 1 ? TEXT("day") : TEXT("days"),
				Model.HoursRemaining,
				Model.HoursRemaining == 1 ? TEXT("hour") : TEXT("hours")));
		}
		else
		{
			Model.CountdownHeadlineText = FText::FromString(
				TEXT("0 days, 0 hours — examination morning (2 February 2043 · 06:00)"));
		}

		Model.FreedomNoticeText = FText::FromString(
			TEXT("Every practice offer is optional. Choosing 'Keep an ordinary day' leaves your classes, household commitments, and friendships intact."));
		Model.bOrdinaryDayKept = bOrdinaryDayKept;
		Model.CommittedOfferId = CommittedOfferId;
		Model.StatusBannerText = StatusBanner;

		for (const PerfectWorld::StandingObligation& Obl : State.Obligations)
		{
			FPWCountdownObligationRow Row;
			Row.Id = UTF8_TO_TCHAR(Obl.Def.Id.c_str());
			Row.Title = SanitizeDisplayEvidence(
				UTF8_TO_TCHAR(Obl.Def.Title.c_str()),
				FText::FromString(TEXT("Household or school commitment")));
			Row.PersonText = SanitizeDisplayEvidence(
				UTF8_TO_TCHAR(Obl.Def.PersonName.c_str()),
				FText::FromString(TEXT("Household & neighbours")));
			Row.WindowText = FText::FromString(FString::Printf(
				TEXT("%s–%s"),
				*ApFormatHourMinute(Obl.Start),
				*ApFormatHourMinute(Obl.End)));
			Row.StatusText = ApObligationStatusLabel(Obl.Disposition);
			Model.Obligations.Add(MoveTemp(Row));
		}

		const std::vector<PerfectWorld::CountdownOffer> CoreOffers =
			PerfectWorld::CountdownOffers(State, World, Ctx, Now);
		for (std::size_t i = 0; i < CoreOffers.size(); ++i)
		{
			const PerfectWorld::CountdownOffer& Offer = CoreOffers[i];
			FPWCountdownOfferRow Row;
			Row.Id = UTF8_TO_TCHAR(Offer.Def.Id.c_str());
			Row.Title = SanitizeDisplayEvidence(
				UTF8_TO_TCHAR(Offer.Def.Name.c_str()),
				FText::FromString(TEXT("Optional Practice Window")));

			const int32 Hours = FMath::Max(1, Offer.Minutes / 60);
			Row.WindowText = FText::FromString(FString::Printf(
				TEXT("%s–%s (%dh)"),
				*ApFormatHourMinute(Offer.Start),
				*ApFormatHourMinute(Offer.End),
				Hours));

			const FString ContactName = UTF8_TO_TCHAR(Offer.Contact.Name.c_str());
			if (!ContactName.IsEmpty() && !LooksLikeRawIdentifier(ContactName))
			{
				Row.ContactText = FText::FromString(FString::Printf(TEXT("With %s"), *ContactName));
			}
			else
			{
				Row.ContactText = FText::FromString(TEXT("Self-directed practice window"));
			}

			Row.SummaryText = FText::FromString(FString::Printf(
				TEXT("Estimated readiness +%d to +%d · strain +%d to +%d"),
				Offer.Bands.ScoreMin,
				Offer.Bands.ScoreMax,
				Offer.Bands.StrainMin,
				Offer.Bands.StrainMax));

			Row.bCommitted = (!CommittedOfferId.IsEmpty() && Row.Id == CommittedOfferId)
				|| (Offer.Availability == PerfectWorld::OpportunityAvailability::Taken);
			Row.bOpen = (Offer.Availability == PerfectWorld::OpportunityAvailability::Open)
				&& !bOrdinaryDayKept
				&& (CommittedOfferId.IsEmpty() || Row.Id == CommittedOfferId);
			Row.AvailabilityText = ApAvailabilityLabel(Offer.Availability, Row.bCommitted);

			for (const PerfectWorld::StandingObligation& Obl : State.Obligations)
			{
				if (Obl.Start < Offer.End && Offer.Start < Obl.End)
				{
					const FText OblTitle = SanitizeDisplayEvidence(
						UTF8_TO_TCHAR(Obl.Def.Title.c_str()),
						FText::FromString(TEXT("Scheduled commitment")));
					const FText PersonName = SanitizeDisplayEvidence(
						UTF8_TO_TCHAR(Obl.Def.PersonName.c_str()),
						FText::FromString(TEXT("household")));
					Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
						TEXT("Displaces standing commitment: %s with %s (%s–%s)"),
						*OblTitle.ToString(),
						*PersonName.ToString(),
						*ApFormatHourMinute(Obl.Start),
						*ApFormatHourMinute(Obl.End))));
				}
			}

			for (std::size_t c = 0; c < Offer.Closes.size(); ++c)
			{
				const FString ClosedTitle = ApLookupOpportunityDisplayName(State.Catalog, Offer.Closes[c]);
				Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
					TEXT("Closes same-day option: %s"),
					*ClosedTitle)));
			}

			if (Row.DisplacedCommitments.Num() == 0)
			{
				Row.DisplacedCommitments.Add(FText::FromString(FString::Printf(
					TEXT("Sets aside %s–%s from your unhurried afternoon and evening hours"),
					*ApFormatHourMinute(Offer.Start),
					*ApFormatHourMinute(Offer.End))));
			}

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
		CountdownModel = BuildCountdownModelFromCore(Now, World, State, Ctx);
	}

	void FApFakeAwakeningWorld::SeedSampleGrantedAwakening()
	{
		PerfectWorld::AwakeningInput Input{};
		for (int32 i = 0; i < 6; ++i)
		{
			Input.Attributes[i] = 52 + i * 4;
		}
		for (int32 i = 0; i < 10; ++i)
		{
			Input.Skills[i] = 44 + (i % 5) * 8;
		}
		for (int32 i = 0; i < 12; ++i)
		{
			Input.Facets[i] = 48 + (i % 4) * 7;
		}
		Input.Conduct.Observed = true;
		Input.Conduct.Resolve = 680;
		Input.Conduct.Care = 790;
		Input.Conduct.Steadfastness = 840;
		Input.Conduct.Restraint = 640;
		Input.Conduct.Resourcefulness = 710;
		Input.Conduct.Attentiveness = 760;

		PerfectWorld::DungeonChoiceRecord Choice0;
		Choice0.TestId = "crossing_span";
		Choice0.ChoiceId = "You steadied the crossing so the student beside you could pass.";
		Choice0.CareDelta = 290;
		Choice0.SteadfastnessDelta = 340;
		Input.Conduct.Choices.push_back(Choice0);

		PerfectWorld::DungeonChoiceRecord Choice1;
		Choice1.TestId = "lintel_seam";
		Choice1.ChoiceId = "You traced the seam along the tutorial pillar before stepping.";
		Choice1.AttentivenessDelta = 260;
		Choice1.ResourcefulnessDelta = 210;
		Input.Conduct.Choices.push_back(Choice1);

		const PerfectWorld::AbilityRecipeSet& Cat = DefaultAbilityRecipeCatalogue();
		PerfectWorld::AwakeningGrant Grant;
		Grant.At = PerfectWorld::GameClock::FromCalendar(2043, 1, 23, 15, 0);
		Grant.Input = Input;
		Grant.Result = PerfectWorld::ResolveAwakening(Input, Cat);
		Grant.StartingProfile = PerfectWorld::BuildStartingAwakenedProfile(Grant.Result, Grant.At);

		ResultModel = BuildResultModelFromGrant(Grant, &Cat, true);
	}

	void FApFakeAwakeningWorld::SeedSampleCountdownDay()
	{
		const PerfectWorld::GameInstant Now =
			PerfectWorld::GameClock::FromCalendar(2043, 1, 24, 8, 0);
		const PerfectWorld::WorldState World{};

		PerfectWorld::CountdownState State{};
		PerfectWorld::StandingObligation Obl0;
		Obl0.Def.Id = "oblig_evening_tea";
		Obl0.Def.Title = "Evening tea and market list at home";
		Obl0.Def.PersonId = "npc_household_aunt";
		Obl0.Def.PersonName = "Meera Roy";
		Obl0.Def.Day = 2;
		Obl0.Def.StartMinute = 17 * 60;
		Obl0.Def.DurationMinutes = 90;
		Obl0.Start = PerfectWorld::GameClock::FromCalendar(2043, 1, 24, 17, 0);
		Obl0.End = Obl0.Start + 90;
		State.Obligations.push_back(Obl0);

		PerfectWorld::CountdownOpportunityDef Opp0;
		Opp0.Id = "opp_courtyard_footing";
		Opp0.Name = "Courtyard Footing Drill";
		Opp0.Group = "day2_late_afternoon";
		Opp0.Day = 2;
		Opp0.StartMinute = 16 * 60 + 30;
		Opp0.DurationMinutes = 120;
		Opp0.ContactId = "npc_mentor_sen";
		Opp0.ContactName = "Subir Sen";
		Opp0.ScoreGain = 115;
		Opp0.StrainDelta = 9;
		State.Catalog.Opportunities.push_back(Opp0);

		PerfectWorld::CountdownOpportunityDef Opp1;
		Opp1.Id = "opp_archive_cadence";
		Opp1.Name = "Reading Room Cadence Study";
		Opp1.Group = "day2_late_afternoon";
		Opp1.Day = 2;
		Opp1.StartMinute = 16 * 60 + 30;
		Opp1.DurationMinutes = 120;
		Opp1.ContactId = "npc_classmate_das";
		Opp1.ContactName = "Ananya Das";
		Opp1.ScoreGain = 95;
		Opp1.StrainDelta = 5;
		State.Catalog.Opportunities.push_back(Opp1);

		const PerfectWorld::CountdownContext Ctx{};
		CountdownModel = BuildCountdownModelFromCore(Now, World, State, Ctx);
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
						? FText::FromString(TEXT("Committed for today"))
						: FText::FromString(TEXT("Closed because another option in this window was chosen"));
				}
				CountdownModel.StatusBannerText = FText::FromString(FString::Printf(
					TEXT("Committed to %s. You can still return to an ordinary day tomorrow."),
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
