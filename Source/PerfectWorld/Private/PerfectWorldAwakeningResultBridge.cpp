// Copyright Perfect World. All Rights Reserved.

#include "PerfectWorldAwakeningResultBridge.h"

#include "Engine/World.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"

namespace PWAwakening
{
	namespace
	{
		TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IApAwakeningWorld>>& R97BridgeRegistry()
		{
			static TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IApAwakeningWorld>> Registry;
			return Registry;
		}

		struct R97FKeyPhrase
		{
			const TCHAR* Key;
			const TCHAR* Evidence;
		};

		const R97FKeyPhrase R97kKnownSourceEvidence[] = {
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

		FString R97LookupKnownEvidence(const FString& Key)
		{
			for (const R97FKeyPhrase& Entry : R97kKnownSourceEvidence)
			{
				if (Key.Equals(Entry.Key, ESearchCase::IgnoreCase))
				{
					return FString(Entry.Evidence);
				}
			}
			return FString();
		}

		FString R97Utf8(std::string_view Text)
		{
			const std::string Owned(Text);
			return FString(UTF8_TO_TCHAR(Owned.c_str()));
		}

		PerfectWorld::Tattva R97DominantTattva(const PerfectWorld::AwakeningResult& Result)
		{
			int32 Best = 0;
			for (int32 i = 1; i < PerfectWorld::TattvaCount; ++i)
			{
				if (Result.TattvaMilli[i] > Result.TattvaMilli[Best])
				{
					Best = i;
				}
			}
			return static_cast<PerfectWorld::Tattva>(Best);
		}

		FText R97FormatClockInstant(PerfectWorld::GameInstant Instant)
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

		FText R97FallbackAbilityName(PerfectWorld::Tattva Affinity)
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

		FText R97FallbackAbilityEffect(PerfectWorld::Tattva Affinity)
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

		class R97FLaneLifeAwakeningWorld final : public IApAwakeningWorld
		{
		public:
			explicit R97FLaneLifeAwakeningWorld(UWorld& InWorld)
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
				// The running world holds no CountdownState yet (WorldState does not carry one), so
				// the clock and deadline are shown and no practice offers are invented.
				const PerfectWorld::CountdownContext Ctx{};
				return BuildCountdownModelFromCore(
					Life->GetNow(), Life->GetWorldState(), nullptr, Ctx, bOrdinaryDayKept, FString(), LastStatusBanner);
			}

			virtual bool ConfirmPracticeOffer(const FString& OfferId, FText& OutNotice) override
			{
				OutNotice = OfferId.IsEmpty()
					? FText::FromString(TEXT("Select an optional practice offer first."))
					: FText::FromString(TEXT("Practice sessions are not connected to the running world yet, so nothing was committed."));
				return false;
			}

			virtual bool KeepOrdinaryDay(FText& OutNotice) override
			{
				bOrdinaryDayKept = true;
				LastStatusBanner = FText::FromString(
					TEXT("You kept an ordinary day. Declining practice costs no time; your classes, household routines, and friendships remain untouched."));
				OutNotice = LastStatusBanner;
				return true;
			}

		private:
			TWeakObjectPtr<UWorld> WorldPtr;
			bool bOrdinaryDayKept = false;
			FText LastStatusBanner;
		};
	}

	void ApBridge::Install(UWorld* World, TSharedPtr<IApAwakeningWorld> Bridge)
	{
		if (World)
		{
			R97BridgeRegistry().Add(World, MoveTemp(Bridge));
		}
	}

	void ApBridge::Remove(UWorld* World)
	{
		if (World)
		{
			R97BridgeRegistry().Remove(World);
		}
	}

	TSharedPtr<IApAwakeningWorld> ApBridge::Resolve(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (const TSharedPtr<IApAwakeningWorld>* Found = R97BridgeRegistry().Find(World))
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
		return MakeShared<R97FLaneLifeAwakeningWorld>(World);
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
		const FString Mapped = R97LookupKnownEvidence(Trimmed);
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
			const FString MappedChoice = R97LookupKnownEvidence(ChoiceStr);
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
			const FString MappedSource = R97LookupKnownEvidence(SourceStr);
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
		// Display-only stand-in for tests and the fake world: one fallback recipe per Tattva, built
		// from the real AbilityRecipe fields. The running game uses the content registry's recipes.
		static const PerfectWorld::AbilityRecipeSet Catalogue = []()
		{
			PerfectWorld::AbilityRecipeSet Set;
			auto AddFallback = [&Set](const char* Id, const char* Name, PerfectWorld::Tattva Affinity, const char* Effect)
			{
				PerfectWorld::AbilityRecipe Recipe;
				Recipe.Id = Id;
				Recipe.Name = Name;
				Recipe.Affinity = Affinity;
				Recipe.Fallback = true;
				Recipe.Effect = Effect;
				Set.Recipes.push_back(Recipe);
			};
			AddFallback("settled_stance", "Settled Stance", PerfectWorld::Tattva::Prithvi,
				"Steadies footing and distributes weight across uneven ground for a short interval.");
			AddFallback("measured_current", "Measured Current", PerfectWorld::Tattva::Jala,
				"Smooths movement transitions and keeps breath even while assisting someone nearby.");
			AddFallback("kindled_point", "Kindled Point", PerfectWorld::Tattva::Agni,
				"Gathers attention and heat into a single clean point of release.");
			AddFallback("paced_crossing", "Paced Crossing", PerfectWorld::Tattva::Vayu,
				"Lightens step transitions across narrow spans and sudden changes of level.");
			AddFallback("clear_interval", "Clear Interval", PerfectWorld::Tattva::Akasha,
				"Marks clear spatial intervals and faint seams in the surrounding room.");
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
		Model.bGranted = Grant.Result.Ok();
		Model.bUnseen = bUnseen && Grant.Result.Ok();
		Model.bConductObserved = Grant.Result.ConductObserved;
		Model.GrantedAtText = R97FormatClockInstant(Grant.At);

		if (!Grant.Result.Ok())
		{
			Model.SubtitleText = FText::FromString(
				TEXT("The school tutorial dungeon has not yet concluded with an Awakening record."));
			return Model;
		}

		Model.SubtitleText = FText::FromString(
			TEXT("Recorded at the close of the school tutorial dungeon. Every output below traces to sixteen years of formation and what you did inside the passage."));

		const int32 StageNum = FMath::Clamp(
			static_cast<int32>(PerfectWorld::StageWithinGrade(Grant.StartingProfile)), 1, 10);
		const FString GradeName = R97Utf8(PerfectWorld::AwakenedGradeName(Grant.StartingProfile.Grade));
		Model.Stage.GradeText = FText::FromString(FString::Printf(TEXT("Grade %s"), *GradeName));
		Model.Stage.StageWithinGrade = StageNum;
		Model.Stage.StageCodeText = FText::FromString(FString::Printf(TEXT("%s-%d"), *GradeName, StageNum));
		Model.Stage.StageTitleText = FText::FromString(
			FString::Printf(TEXT("Grade %s · Stage %d of 10"), *GradeName, StageNum));
		Model.Stage.RuleNoticeText = FText::FromString(
			TEXT("Every Awakening begins within Grade F (Stages 1 to 10). Advancing beyond Grade F requires formal assessment; everything after the tutorial is optional."));

		for (int32 i = 0; i < PerfectWorld::TattvaCount; ++i)
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

		for (int32 d = 0; d < PerfectWorld::PowerDimensionCount; ++d)
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
				TEXT("%d points"), Row.RatingPoints));
			Row.BreakdownText = FText::FromString(FString::Printf(
				TEXT("%d from sixteen years of formation · %d from tutorial conduct"),
				Row.LatentPoints, Row.ConductPoints));

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
			const FText& TattvaLine = Model.Tattvas[0].EvidenceLines[0];
			bool bSeen = false;
			for (const FText& Existing : Model.Stage.EvidenceLines)
			{
				bSeen = bSeen || Existing.EqualTo(TattvaLine);
			}
			if (!bSeen)
			{
				Model.Stage.EvidenceLines.Add(TattvaLine);
			}
		}

		const PerfectWorld::AbilityRecipeSet& Cat = Recipes ? *Recipes : DefaultAbilityRecipeCatalogue();
		if (!Grant.Result.Ability.RecipeId.empty())
		{
			const PerfectWorld::AwakenedAbilityGrant& Ab = Grant.Result.Ability;
			Model.Ability.RecipeId = R97Utf8(Ab.RecipeId);
			Model.Ability.bFallback = Ab.Fallback;
			const PerfectWorld::AbilityRecipe* Found = Cat.Find(Ab.RecipeId);
			const PerfectWorld::Tattva Dominant = R97DominantTattva(Grant.Result);
			Model.Ability.AffinityText = TattvaDisplayName(Found ? Found->Affinity : Dominant);

			if (Found && !Found->Name.empty())
			{
				Model.Ability.Name = SanitizeDisplayEvidence(R97Utf8(Found->Name), R97FallbackAbilityName(Dominant));
			}
			else
			{
				Model.Ability.Name = R97FallbackAbilityName(Dominant);
			}

			if (Found && !Found->Effect.empty())
			{
				Model.Ability.EffectText = SanitizeDisplayEvidence(R97Utf8(Found->Effect), R97FallbackAbilityEffect(Dominant));
			}
			else
			{
				Model.Ability.EffectText = R97FallbackAbilityEffect(Dominant);
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
			const PerfectWorld::Tattva Dominant = R97DominantTattva(Grant.Result);
			Model.Ability.Name = R97FallbackAbilityName(Dominant);
			Model.Ability.AffinityText = TattvaDisplayName(Dominant);
			Model.Ability.EffectText = R97FallbackAbilityEffect(Dominant);
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
