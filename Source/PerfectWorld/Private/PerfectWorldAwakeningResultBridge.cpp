// Copyright Perfect World. All Rights Reserved.

#include "PerfectWorldAwakeningResultBridge.h"

#include "Engine/World.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"

namespace PWAwakening
{
	namespace
	{
		TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IApAwakeningWorld>>& ApBridgeRegistry()
		{
			static TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IApAwakeningWorld>> Registry;
			return Registry;
		}

		struct FApKeyPhrase
		{
			const TCHAR* Key;
			const TCHAR* Evidence;
		};

		const FApKeyPhrase kApKnownSourceEvidence[] = {
			{TEXT("attribute.body"), TEXT("Sixteen years of physical conditioning and steady breath.")},
			{TEXT("attribute.agility"), TEXT("Footwork and balance practiced across narrow Kolkata lanes.")},
			{TEXT("attribute.intelligence"), TEXT("Class XI analytical study and structured problem-solving.")},
			{TEXT("attribute.perception"), TEXT("Habitual notice of small shifts in light, sound, and footing.")},
			{TEXT("attribute.charisma"), TEXT("Direct, measured speech with classmates and neighbours.")},
			{TEXT("attribute.willpower"), TEXT("Finishing long study and household stretches without dropping pace.")},
			{TEXT("attribute.luck"), TEXT("Timely adjustments when an ordinary routine shifted without warning.")},
			{TEXT("skill.academics"), TEXT("Sixteen years of school study and notebook work in Kolkata.")},
			{TEXT("skill.athletics"), TEXT("Regular running, courtyard drill, and unhurried stamina.")},
			{TEXT("skill.computing"), TEXT("Methodical logic and terminal troubleshooting at the desk.")},
			{TEXT("skill.craft"), TEXT("Hands-on repair, bench work, and careful tool handling.")},
			{TEXT("skill.expression"), TEXT("Clear phrasing and steady cadence in recitation and conversation.")},
			{TEXT("skill.first_aid"), TEXT("Tending scrapes, wraps, and breathing when someone faltered.")},
			{TEXT("skill.household"), TEXT("Daily kitchen, market, and household upkeep carried through on time.")},
			{TEXT("skill.negotiation"), TEXT("Patient give-and-take in neighbourhood and school commitments.")},
			{TEXT("skill.observation"), TEXT("Watching crossings, thresholds, and street corners before moving.")},
			{TEXT("skill.streetwise"), TEXT("Reading tram-line crowds and side-lane timing across the ward.")},
			{TEXT("facet.rigor"), TEXT("Checking each step of a method before writing down the answer.")},
			{TEXT("facet.recall"), TEXT("Retaining exact details from earlier lessons and conversations.")},
			{TEXT("facet.analysis"), TEXT("Breaking unfamiliar problems into ordered parts before acting.")},
			{TEXT("facet.focus"), TEXT("Keeping attention on a single task through background noise.")},
			{TEXT("facet.endurance"), TEXT("Holding posture and effort through the final hour of a long day.")},
			{TEXT("facet.coordination"), TEXT("Matching hand, eye, and step without wasted motion.")},
			{TEXT("facet.situational_awareness"), TEXT("Tracking who stood nearby and where the clear path lay.")},
			{TEXT("facet.composure"), TEXT("Keeping an even pulse when the school tutorial passage shifted.")},
			{TEXT("facet.empathy"), TEXT("Noticing when a classmate beside you needed a slower pace.")},
			{TEXT("facet.articulation"), TEXT("Calling out clear directions that others could follow at once.")},
			{TEXT("facet.initiative"), TEXT("Taking the first measured step when the group hesitated.")},
			{TEXT("facet.grit"), TEXT("Returning to the task after the first attempt slipped.")},
			{TEXT("conduct.resolve"), TEXT("You stepped onto the narrowed tutorial span and kept moving.")},
			{TEXT("conduct.care"), TEXT("You steadied the crossing so the student beside you could pass.")},
			{TEXT("conduct.steadfastness"), TEXT("You held the threshold stone until the tutorial gate settled.")},
			{TEXT("conduct.restraint"), TEXT("You waited for the tremor to pass before lifting the bar.")},
			{TEXT("conduct.resourcefulness"), TEXT("You wedged the loose block to brace the tutorial lintel.")},
			{TEXT("conduct.attentiveness"), TEXT("You traced the seam along the tutorial pillar before stepping.")},
		};

		FString ApLookupKnownEvidence(const FString& Key)
		{
			for (const FApKeyPhrase& Entry : kApKnownSourceEvidence)
			{
				if (Key.Equals(Entry.Key, ESearchCase::IgnoreCase))
				{
					return FString(Entry.Evidence);
				}
			}
			return FString();
		}

		FText ApFormatClockInstant(PerfectWorld::GameInstant Instant)
		{
			const PerfectWorld::CalendarTime Cal = PerfectWorld::GameClock::ToCalendar(Instant);
			static const TCHAR* kMonths[] = {
				TEXT(""), TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
				TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")
			};
			const int32 MonthIdx = FMath::Clamp(Cal.Month, 1, 12);
			return FText::FromString(FString::Printf(
				TEXT("%d %s %04d · %02d:%02d"),
				Cal.Day, kMonths[MonthIdx], Cal.Year, Cal.Hour, Cal.Minute));
		}

		FText ApFallbackAbilityName(PerfectWorld::Tattva Affinity)
		{
			switch (Affinity)
			{
			case PerfectWorld::Tattva::Prithvi: return FText::FromString(TEXT("Settled Footing"));
			case PerfectWorld::Tattva::Jala:    return FText::FromString(TEXT("Measured Current"));
			case PerfectWorld::Tattva::Agni:    return FText::FromString(TEXT("Kindled Focus"));
			case PerfectWorld::Tattva::Vayu:    return FText::FromString(TEXT("Paced Step"));
			case PerfectWorld::Tattva::Akasha:  return FText::FromString(TEXT("Clear Interval"));
			}
			return FText::FromString(TEXT("Settled Footing"));
		}

		FText ApFallbackAbilityEffect(PerfectWorld::Tattva Affinity)
		{
			switch (Affinity)
			{
			case PerfectWorld::Tattva::Prithvi:
				return FText::FromString(TEXT("Steadies stance and anchors weight across uneven ground for a short interval."));
			case PerfectWorld::Tattva::Jala:
				return FText::FromString(TEXT("Smooths transitions between movements and eases strain across a sustained cadence."));
			case PerfectWorld::Tattva::Agni:
				return FText::FromString(TEXT("Concentrates effort into a single clean release without scattering attention."));
			case PerfectWorld::Tattva::Vayu:
				return FText::FromString(TEXT("Lightens step transitions and widens margin when crossing narrow spans."));
			case PerfectWorld::Tattva::Akasha:
				return FText::FromString(TEXT("Sharpens spatial bearings and keeps nearby intervals distinct in crowded rooms."));
			}
			return FText::FromString(TEXT("Steadies stance and anchors weight across uneven ground for a short interval."));
		}

		class FApLaneLifeAwakeningWorld final : public IApAwakeningWorld
		{
		public:
			explicit FApLaneLifeAwakeningWorld(UWorld& InWorld)
				: WorldPtr(&InWorld)
			{
			}

			virtual FPWAwakeningResultModel ReadResultModel() override
			{
				UWorld* World = WorldPtr.Get();
				UPWLaneLifeSubsystem* Life = World ? World->GetSubsystem<UPWLaneLifeSubsystem>() : nullptr;
				if (!Life)
				{
					return FPWAwakeningResultModel{};
				}
				FPWAwakeningResultModel Model;
				Model.bAvailable = true;
				Model.bGranted = false;
				Model.bUnseen = false;
				Model.SubtitleText = FText::FromString(
					TEXT("The school tutorial dungeon has not yet granted an Awakening record in this session."));
				return Model;
			}

			virtual bool HasUnseenResult() const override
			{
				return false;
			}

			virtual void AcknowledgeResult() override
			{
			}

			virtual FPWCountdownModel ReadCountdownModel() override
			{
				UWorld* World = WorldPtr.Get();
				UPWLaneLifeSubsystem* Life = World ? World->GetSubsystem<UPWLaneLifeSubsystem>() : nullptr;
				if (!Life)
				{
					return FPWCountdownModel{};
				}
				const PerfectWorld::WorldState& CoreWorld = Life->CoreWorld();
				PerfectWorld::CountdownState State{};
				PerfectWorld::CountdownContext Ctx{};
				return BuildCountdownModelFromCore(
					Life->GetNow(), CoreWorld, State, Ctx, bOrdinaryDayKept, CommittedOfferId, LastStatusBanner);
			}

			virtual bool ConfirmPracticeOffer(const FString& OfferId, FText& OutNotice) override
			{
				if (OfferId.IsEmpty())
				{
					OutNotice = FText::FromString(TEXT("Select an optional practice offer first."));
					return false;
				}
				CommittedOfferId = OfferId;
				bOrdinaryDayKept = false;
				LastStatusBanner = FText::FromString(TEXT("Optional practice recorded for today."));
				OutNotice = LastStatusBanner;
				return true;
			}

			virtual bool KeepOrdinaryDay(FText& OutNotice) override
			{
				bOrdinaryDayKept = true;
				CommittedOfferId.Reset();
				LastStatusBanner = FText::FromString(
					TEXT("You kept an ordinary day. Your classes, household routines, and friendships remain untouched."));
				OutNotice = LastStatusBanner;
				return true;
			}

		private:
			TWeakObjectPtr<UWorld> WorldPtr;
			bool bOrdinaryDayKept = false;
			FString CommittedOfferId;
			FText LastStatusBanner;
		};
	}

	void ApBridge::Install(UWorld* World, TSharedPtr<IApAwakeningWorld> Bridge)
	{
		if (World)
		{
			ApBridgeRegistry().Add(World, MoveTemp(Bridge));
		}
	}

	void ApBridge::Remove(UWorld* World)
	{
		if (World)
		{
			ApBridgeRegistry().Remove(World);
		}
	}

	TSharedPtr<IApAwakeningWorld> ApBridge::Resolve(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (const TSharedPtr<IApAwakeningWorld>* Found = ApBridgeRegistry().Find(World))
		{
			if (Found->IsValid())
			{
				return *Found;
			}
		}
		return MakeLaneLifeAwakeningBridge(*World);
	}

	TSharedPtr<IApAwakeningWorld> MakeLaneLifeAwakeningBridge(UWorld& World)
	{
		return MakeShared<FApLaneLifeAwakeningWorld>(World);
	}

	FText TattvaDisplayName(PerfectWorld::Tattva Tattva)
	{
		switch (Tattva)
		{
		case PerfectWorld::Tattva::Prithvi: return FText::FromString(TEXT("Prithvi"));
		case PerfectWorld::Tattva::Jala:    return FText::FromString(TEXT("Jala"));
		case PerfectWorld::Tattva::Agni:    return FText::FromString(TEXT("Agni"));
		case PerfectWorld::Tattva::Vayu:    return FText::FromString(TEXT("Vayu"));
		case PerfectWorld::Tattva::Akasha:  return FText::FromString(TEXT("Akasha"));
		}
		return FText::FromString(TEXT("Prithvi"));
	}

	FText TattvaDomainText(PerfectWorld::Tattva Tattva)
	{
		switch (Tattva)
		{
		case PerfectWorld::Tattva::Prithvi: return FText::FromString(TEXT("Mass, footing, and held ground"));
		case PerfectWorld::Tattva::Jala:    return FText::FromString(TEXT("Flow, continuity, and recovery"));
		case PerfectWorld::Tattva::Agni:    return FText::FromString(TEXT("Directed heat, clarity, and release"));
		case PerfectWorld::Tattva::Vayu:    return FText::FromString(TEXT("Breath, pacing, and movement"));
		case PerfectWorld::Tattva::Akasha:  return FText::FromString(TEXT("Interval, room, and awareness"));
		}
		return FText::FromString(TEXT("Mass, footing, and held ground"));
	}

	FText CapabilityDisplayName(PerfectWorld::PowerDimension Dim)
	{
		switch (Dim)
		{
		case PerfectWorld::PowerDimension::Output:       return FText::FromString(TEXT("Output"));
		case PerfectWorld::PowerDimension::Precision:    return FText::FromString(TEXT("Precision"));
		case PerfectWorld::PowerDimension::Range:        return FText::FromString(TEXT("Range"));
		case PerfectWorld::PowerDimension::Duration:     return FText::FromString(TEXT("Duration"));
		case PerfectWorld::PowerDimension::Perception:   return FText::FromString(TEXT("Perception"));
		case PerfectWorld::PowerDimension::Coordination: return FText::FromString(TEXT("Coordination"));
		}
		return FText::FromString(TEXT("Output"));
	}

	FText CapabilityDomainText(PerfectWorld::PowerDimension Dim)
	{
		switch (Dim)
		{
		case PerfectWorld::PowerDimension::Output:       return FText::FromString(TEXT("Force and volume carried into action"));
		case PerfectWorld::PowerDimension::Precision:    return FText::FromString(TEXT("Exact placement and fine control"));
		case PerfectWorld::PowerDimension::Range:        return FText::FromString(TEXT("Reach across distance and span"));
		case PerfectWorld::PowerDimension::Duration:     return FText::FromString(TEXT("Sustained hold across time"));
		case PerfectWorld::PowerDimension::Perception:   return FText::FromString(TEXT("Notice of seams, shifts, and bearings"));
		case PerfectWorld::PowerDimension::Coordination: return FText::FromString(TEXT("Timing between hand, breath, and step"));
		}
		return FText::FromString(TEXT("Force and volume carried into action"));
	}

	FText SignalDisplayName(PerfectWorld::AwakeningSignal Signal)
	{
		switch (Signal)
		{
		case PerfectWorld::AwakeningSignal::Study:           return FText::FromString(TEXT("Sustained study and notebook work"));
		case PerfectWorld::AwakeningSignal::Craft:           return FText::FromString(TEXT("Careful bench work and tool handling"));
		case PerfectWorld::AwakeningSignal::Bodywork:        return FText::FromString(TEXT("Physical conditioning and steady pace"));
		case PerfectWorld::AwakeningSignal::People:          return FText::FromString(TEXT("Attentive give-and-take with others"));
		case PerfectWorld::AwakeningSignal::Order:           return FText::FromString(TEXT("Methodical routine and kept structure"));
		case PerfectWorld::AwakeningSignal::Making:          return FText::FromString(TEXT("Shaping materials with measured hands"));
		case PerfectWorld::AwakeningSignal::Resolve:         return FText::FromString(TEXT("Stepping forward when the path narrowed"));
		case PerfectWorld::AwakeningSignal::Senses:          return FText::FromString(TEXT("Tracking subtle shifts in room and lane"));
		case PerfectWorld::AwakeningSignal::Care:            return FText::FromString(TEXT("Looking after those beside you under strain"));
		case PerfectWorld::AwakeningSignal::Steadfastness:   return FText::FromString(TEXT("Holding ground until the passage settled"));
		case PerfectWorld::AwakeningSignal::Restraint:       return FText::FromString(TEXT("Waiting for the right interval before acting"));
		case PerfectWorld::AwakeningSignal::Resourcefulness: return FText::FromString(TEXT("Using what lay at hand to solve the obstacle"));
		case PerfectWorld::AwakeningSignal::Attentiveness:   return FText::FromString(TEXT("Noticing small details before committing a step"));
		}
		return FText::FromString(TEXT("Recorded sixteen-year practice and tutorial conduct"));
	}

	bool LooksLikeRawIdentifier(const FString& Candidate)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return true;
		}
		if (Trimmed.Contains(TEXT("_")) || Trimmed.Contains(TEXT("{")) || Trimmed.Contains(TEXT("}")))
		{
			return true;
		}
		const FString Lower = Trimmed.ToLower();
		static const TCHAR* kRawPrefixes[] = {
			TEXT("attribute."), TEXT("skill."), TEXT("facet."), TEXT("conduct."),
			TEXT("ability."), TEXT("recipe."), TEXT("opp."), TEXT("oblig."),
			TEXT("test."), TEXT("choice."), TEXT("tattva."), TEXT("dim.")
		};
		for (const TCHAR* Prefix : kRawPrefixes)
		{
			if (Lower.StartsWith(Prefix))
			{
				return true;
			}
		}
		if (!Trimmed.Contains(TEXT(" ")) && Trimmed.Contains(TEXT(".")))
		{
			return true;
		}
		return false;
	}

	FText SanitizeDisplayEvidence(const FString& Candidate, const FText& FallbackText)
	{
		const FString Trimmed = Candidate.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FallbackText;
		}
		const FString Mapped = ApLookupKnownEvidence(Trimmed);
		if (!Mapped.IsEmpty())
		{
			return FText::FromString(Mapped);
		}
		if (LooksLikeRawIdentifier(Trimmed))
		{
			return FallbackText;
		}
		return FText::FromString(Trimmed);
	}

	FText FormatCauseEvidence(const PerfectWorld::AwakeningCause& Cause)
	{
		const FString ChoiceStr = UTF8_TO_TCHAR(Cause.ChoiceId.c_str());
		if (!ChoiceStr.IsEmpty())
		{
			const FString MappedChoice = ApLookupKnownEvidence(ChoiceStr);
			if (!MappedChoice.IsEmpty())
			{
				return FText::FromString(MappedChoice);
			}
			if (!LooksLikeRawIdentifier(ChoiceStr))
			{
				return FText::FromString(ChoiceStr.TrimStartAndEnd());
			}
		}

		const FString SourceStr = UTF8_TO_TCHAR(Cause.Source.c_str());
		if (!SourceStr.IsEmpty())
		{
			const FString MappedSource = ApLookupKnownEvidence(SourceStr);
			if (!MappedSource.IsEmpty())
			{
				return FText::FromString(MappedSource);
			}
			if (!LooksLikeRawIdentifier(SourceStr))
			{
				return FText::FromString(SourceStr.TrimStartAndEnd());
			}
		}

		return SignalDisplayName(Cause.Signal);
	}

	const PerfectWorld::AbilityRecipeSet& DefaultAbilityRecipeCatalogue()
	{
		static const PerfectWorld::AbilityRecipeSet Catalogue = []()
		{
			PerfectWorld::AbilityRecipeSet Set;
			PerfectWorld::AbilityRecipe R0;
			R0.Id = "settled_stance";
			R0.Name = "Settled Stance";
			R0.Affinity = PerfectWorld::Tattva::Prithvi;
			R0.PrimaryDimension = PerfectWorld::PowerDimension::Duration;
			R0.SecondaryDimension = PerfectWorld::PowerDimension::Output;
			R0.PrimarySignal = PerfectWorld::AwakeningSignal::Steadfastness;
			R0.SecondarySignal = PerfectWorld::AwakeningSignal::Bodywork;
			R0.OriginTag = "foundation";
			R0.Effect = "Steadies footing and distributes weight across uneven ground for a short interval.";
			Set.Recipes.push_back(R0);

			PerfectWorld::AbilityRecipe R1;
			R1.Id = "measured_current";
			R1.Name = "Measured Current";
			R1.Affinity = PerfectWorld::Tattva::Jala;
			R1.PrimaryDimension = PerfectWorld::PowerDimension::Coordination;
			R1.SecondaryDimension = PerfectWorld::PowerDimension::Duration;
			R1.PrimarySignal = PerfectWorld::AwakeningSignal::Care;
			R1.SecondarySignal = PerfectWorld::AwakeningSignal::People;
			R1.OriginTag = "continuity";
			R1.Effect = "Smooths movement transitions and keeps breath even while assisting someone nearby.";
			Set.Recipes.push_back(R1);

			PerfectWorld::AbilityRecipe R2;
			R2.Id = "kindled_point";
			R2.Name = "Kindled Point";
			R2.Affinity = PerfectWorld::Tattva::Agni;
			R2.PrimaryDimension = PerfectWorld::PowerDimension::Precision;
			R2.SecondaryDimension = PerfectWorld::PowerDimension::Output;
			R2.PrimarySignal = PerfectWorld::AwakeningSignal::Study;
			R2.SecondarySignal = PerfectWorld::AwakeningSignal::Resolve;
			R2.OriginTag = "focus";
			R2.Effect = "Gathers attention and heat into a single clean point of release.";
			Set.Recipes.push_back(R2);

			PerfectWorld::AbilityRecipe R3;
			R3.Id = "paced_crossing";
			R3.Name = "Paced Crossing";
			R3.Affinity = PerfectWorld::Tattva::Vayu;
			R3.PrimaryDimension = PerfectWorld::PowerDimension::Range;
			R3.SecondaryDimension = PerfectWorld::PowerDimension::Coordination;
			R3.PrimarySignal = PerfectWorld::AwakeningSignal::Bodywork;
			R3.SecondarySignal = PerfectWorld::AwakeningSignal::Resourcefulness;
			R3.OriginTag = "step";
			R3.Effect = "Lightens step transitions across narrow spans and sudden changes of level.";
			Set.Recipes.push_back(R3);

			PerfectWorld::AbilityRecipe R4;
			R4.Id = "clear_interval";
			R4.Name = "Clear Interval";
			R4.Affinity = PerfectWorld::Tattva::Akasha;
			R4.PrimaryDimension = PerfectWorld::PowerDimension::Perception;
			R4.SecondaryDimension = PerfectWorld::PowerDimension::Range;
			R4.PrimarySignal = PerfectWorld::AwakeningSignal::Attentiveness;
			R4.SecondarySignal = PerfectWorld::AwakeningSignal::Senses;
			R4.OriginTag = "bearing";
			R4.Effect = "Marks clear spatial intervals and faint seams in the surrounding room.";
			Set.Recipes.push_back(R4);

			return Set;
		}();
		return Catalogue;
	}

	FPWAwakeningResultModel BuildResultModelFromGrant(
		const PerfectWorld::AwakeningGrant& Grant,
		const PerfectWorld::AbilityRecipeSet* Recipes,
		bool bUnseen)
	{
		FPWAwakeningResultModel Model;
		Model.bAvailable = true;
		Model.bGranted = Grant.Result.Granted;
		Model.bUnseen = bUnseen && Grant.Result.Granted;
		Model.bConductObserved = Grant.Result.ConductObserved;
		Model.GrantedAtText = ApFormatClockInstant(Grant.At);

		if (!Grant.Result.Granted)
		{
			Model.SubtitleText = FText::FromString(
				TEXT("The school tutorial dungeon has not yet concluded with an Awakening record."));
			return Model;
		}

		Model.SubtitleText = FText::FromString(
			TEXT("Recorded at the close of the school tutorial dungeon. Every output below traces to sixteen years of formation and what you did inside the passage."));

		const int32 StageNum = FMath::Clamp(
			static_cast<int32>(PerfectWorld::StageWithinGrade(Grant.StartingProfile)), 1, 10);
		Model.Stage.GradeText = FText::FromString(TEXT("Grade F"));
		Model.Stage.StageWithinGrade = StageNum;
		Model.Stage.StageCodeText = FText::FromString(FString::Printf(TEXT("F-%d"), StageNum));
		Model.Stage.StageTitleText = FText::FromString(
			FString::Printf(TEXT("Grade F · Stage %d of 10"), StageNum));
		Model.Stage.RuleNoticeText = FText::FromString(
			TEXT("Every Awakening begins within Grade F (Stages 1 to 10). Advancing beyond Grade F requires formal assessment; everything after the tutorial is optional."));

		for (std::size_t i = 0; i < PerfectWorld::TattvaCount; ++i)
		{
			const PerfectWorld::Tattva Tv = static_cast<PerfectWorld::Tattva>(i);
			FPWAwakeningTattvaRow Row;
			Row.Tattva = Tv;
			Row.Name = TattvaDisplayName(Tv);
			Row.Domain = TattvaDomainText(Tv);
			Row.Percent = Grant.Result.TattvaPercent[i];
			Row.Milli = Grant.Result.TattvaMilli[i];
			Row.ShareText = FText::FromString(FString::Printf(
				TEXT("%d%% (%d.%02d%%)"),
				Row.Percent,
				Row.Milli / 1000,
				(Row.Milli % 1000) / 10));

			for (const PerfectWorld::AwakeningCause& Cause : Grant.Result.TattvaCauses[i])
			{
				const FText Line = FormatCauseEvidence(Cause);
				if (!Line.IsEmpty())
				{
					Row.EvidenceLines.Add(Line);
				}
			}
			if (Row.EvidenceLines.Num() == 0)
			{
				Row.EvidenceLines.Add(FText::FromString(
					TEXT("Formed from baseline sixteen-year study, household rhythm, and tutorial passage footing.")));
			}
			Model.Tattvas.Add(MoveTemp(Row));
		}

		for (std::size_t d = 0; d < PerfectWorld::PowerDimensionCount; ++d)
		{
			const PerfectWorld::PowerDimension Dim = static_cast<PerfectWorld::PowerDimension>(d);
			FPWAwakeningCapabilityRow Row;
			Row.Dimension = Dim;
			Row.Name = CapabilityDisplayName(Dim);
			Row.Domain = CapabilityDomainText(Dim);
			Row.RatingPoints = Grant.Result.Rating[d];
			Row.LatentPoints = Grant.Result.LatentRating[d];
			Row.ConductPoints = Grant.Result.ConductRating[d];
			Row.ScoreText = FText::FromString(FString::Printf(
				TEXT("%d.%02d pts"), Row.RatingPoints / 100, Row.RatingPoints % 100));
			Row.BreakdownText = FText::FromString(FString::Printf(
				TEXT("%d.%02d sixteen-year formation · %d.%02d tutorial conduct"),
				Row.LatentPoints / 100, Row.LatentPoints % 100,
				Row.ConductPoints / 100, Row.ConductPoints % 100));

			for (const PerfectWorld::AwakeningCause& Cause : Grant.Result.RatingCauses[d])
			{
				const FText Line = FormatCauseEvidence(Cause);
				if (!Line.IsEmpty())
				{
					Row.EvidenceLines.Add(Line);
				}
			}
			if (Row.EvidenceLines.Num() == 0)
			{
				Row.EvidenceLines.Add(FText::FromString(
					TEXT("Derived from sixteen-year practice and tutorial conduct across the six capability dimensions.")));
			}
			Model.Capabilities.Add(MoveTemp(Row));
		}

		if (Model.Capabilities.Num() > 0 && Model.Capabilities[0].EvidenceLines.Num() > 0)
		{
			Model.Stage.EvidenceLines.Add(Model.Capabilities[0].EvidenceLines[0]);
		}
		if (Model.Tattvas.Num() > 0 && Model.Tattvas[0].EvidenceLines.Num() > 0)
		{
			Model.Stage.EvidenceLines.AddUnique(Model.Tattvas[0].EvidenceLines[0]);
		}

		const PerfectWorld::AbilityRecipeSet& Cat = Recipes ? *Recipes : DefaultAbilityRecipeCatalogue();
		if (!Grant.Result.Abilities.empty())
		{
			const PerfectWorld::AwakenedAbilityGrant& Ab = Grant.Result.Abilities[0];
			Model.Ability.RecipeId = UTF8_TO_TCHAR(Ab.RecipeId.c_str());
			Model.Ability.bFallback = Ab.Fallback;
			const PerfectWorld::AbilityRecipe* Found = Cat.Find(Ab.RecipeId);
			const PerfectWorld::Tattva Dominant = Grant.StartingProfile.DominantAffinity;
			Model.Ability.AffinityText = TattvaDisplayName(Found ? Found->Affinity : Dominant);

			if (Found && !Found->Name.empty())
			{
				Model.Ability.Name = SanitizeDisplayEvidence(
					UTF8_TO_TCHAR(Found->Name.c_str()), ApFallbackAbilityName(Dominant));
			}
			else
			{
				Model.Ability.Name = ApFallbackAbilityName(Dominant);
			}

			if (Found && !Found->Effect.empty())
			{
				Model.Ability.EffectText = SanitizeDisplayEvidence(
					UTF8_TO_TCHAR(Found->Effect.c_str()), ApFallbackAbilityEffect(Dominant));
			}
			else
			{
				Model.Ability.EffectText = ApFallbackAbilityEffect(Dominant);
			}

			Model.Ability.OriginBadgeText = Ab.Fallback
				? FText::FromString(TEXT("Foundational form aligned to dominant affinity"))
				: FText::FromString(TEXT("Matched to your affinity, capability balance, and tutorial conduct"));

			for (const PerfectWorld::AwakeningCause& Cause : Ab.Causes)
			{
				const FText Line = FormatCauseEvidence(Cause);
				if (!Line.IsEmpty())
				{
					Model.Ability.EvidenceLines.Add(Line);
				}
			}
		}
		if (Model.Ability.Name.IsEmpty())
		{
			const PerfectWorld::Tattva Dominant = Grant.StartingProfile.DominantAffinity;
			Model.Ability.Name = ApFallbackAbilityName(Dominant);
			Model.Ability.AffinityText = TattvaDisplayName(Dominant);
			Model.Ability.EffectText = ApFallbackAbilityEffect(Dominant);
			Model.Ability.bFallback = true;
			Model.Ability.OriginBadgeText = FText::FromString(TEXT("Foundational form aligned to dominant affinity"));
		}
		if (Model.Ability.EvidenceLines.Num() == 0 && Model.Tattvas.Num() > 0 && Model.Tattvas[0].EvidenceLines.Num() > 0)
		{
			Model.Ability.EvidenceLines.Add(Model.Tattvas[0].EvidenceLines[0]);
		}

		return Model;
	}
}
