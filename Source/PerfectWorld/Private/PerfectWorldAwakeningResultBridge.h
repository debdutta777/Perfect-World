// Copyright Perfect World. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AwakenedProgression.h"
#include "Awakening.h"
#include "AwakeningInputs.h"
#include "Countdown.h"
#include "GameClock.h"
#include "WorldState.h"

class UWorld;

namespace PWAwakening
{
	struct FPWAwakeningTattvaRow
	{
		PerfectWorld::Tattva Tattva = PerfectWorld::Tattva::Prithvi;
		FText Name;
		FText Domain;
		int32 Percent = 0;
		int32 Milli = 0;
		FText ShareText;
		TArray<FText> EvidenceLines;
	};

	struct FPWAwakeningCapabilityRow
	{
		PerfectWorld::PowerDimension Dimension = PerfectWorld::PowerDimension::Output;
		FText Name;
		FText Domain;
		int32 RatingPoints = 0;
		int32 LatentPoints = 0;
		int32 ConductPoints = 0;
		FText ScoreText;
		FText BreakdownText;
		TArray<FText> EvidenceLines;
	};

	struct FPWAwakeningAbilityView
	{
		FString RecipeId;
		FText Name;
		FText AffinityText;
		FText EffectText;
		bool bFallback = false;
		FText OriginBadgeText;
		TArray<FText> EvidenceLines;
	};

	struct FPWAwakeningStageView
	{
		FText GradeText;
		int32 StageWithinGrade = 1;
		FText StageCodeText;
		FText StageTitleText;
		FText RuleNoticeText;
		TArray<FText> EvidenceLines;
	};

	struct FPWAwakeningResultModel
	{
		bool bAvailable = false;
		bool bGranted = false;
		bool bUnseen = false;
		bool bConductObserved = false;
		FText GrantedAtText;
		FText SubtitleText;
		FPWAwakeningStageView Stage;
		TArray<FPWAwakeningTattvaRow> Tattvas;
		TArray<FPWAwakeningCapabilityRow> Capabilities;
		FPWAwakeningAbilityView Ability;
	};

	struct FPWCountdownOfferRow
	{
		FString Id;
		FText Title;
		FText WindowText;
		FText ContactText;
		FText SummaryText;
		bool bOpen = true;
		bool bCommitted = false;
		FText AvailabilityText;
		TArray<FText> DisplacedCommitments;
	};

	struct FPWCountdownObligationRow
	{
		FString Id;
		FText Title;
		FText PersonText;
		FText WindowText;
		FText StatusText;
	};

	struct FPWCountdownModel
	{
		bool bAvailable = false;
		FText CurrentClockText;
		FText ExamDeadlineText;
		int32 DaysRemaining = 0;
		int32 HoursRemaining = 0;
		FText CountdownHeadlineText;
		FText FreedomNoticeText;
		bool bOrdinaryDayKept = false;
		FString CommittedOfferId;
		FText StatusBannerText;
		TArray<FPWCountdownOfferRow> Offers;
		TArray<FPWCountdownObligationRow> Obligations;
	};

	class IApAwakeningWorld
	{
	public:
		virtual ~IApAwakeningWorld() = default;

		virtual FPWAwakeningResultModel ReadResultModel() = 0;
		virtual bool HasUnseenResult() const = 0;
		virtual void AcknowledgeResult() = 0;

		virtual FPWCountdownModel ReadCountdownModel() = 0;
		virtual bool ConfirmPracticeOffer(const FString& OfferId, FText& OutNotice) = 0;
		virtual bool KeepOrdinaryDay(FText& OutNotice) = 0;
	};

	struct ApBridge
	{
		static void Install(UWorld* World, TSharedPtr<IApAwakeningWorld> Bridge);
		static void Remove(UWorld* World);
		static TSharedPtr<IApAwakeningWorld> Resolve(UWorld* World);
	};

	TSharedPtr<IApAwakeningWorld> MakeLaneLifeAwakeningBridge(UWorld& World);

	FText TattvaDisplayName(PerfectWorld::Tattva Tattva);
	FText TattvaDomainText(PerfectWorld::Tattva Tattva);
	FText CapabilityDisplayName(PerfectWorld::PowerDimension Dim);
	FText CapabilityDomainText(PerfectWorld::PowerDimension Dim);
	FText SignalDisplayName(PerfectWorld::AwakeningSignal Signal);
	bool LooksLikeRawIdentifier(const FString& Candidate);
	FText SanitizeDisplayEvidence(const FString& Candidate, const FText& FallbackText);
	FText FormatCauseEvidence(const PerfectWorld::AwakeningCause& Cause);

	const PerfectWorld::AbilityRecipeSet& DefaultAbilityRecipeCatalogue();

	FPWAwakeningResultModel BuildResultModelFromGrant(
		const PerfectWorld::AwakeningGrant& Grant,
		const PerfectWorld::AbilityRecipeSet* Recipes = nullptr,
		bool bUnseen = true);

	FPWCountdownModel BuildCountdownModelFromCore(
		PerfectWorld::GameInstant Now,
		const PerfectWorld::WorldState& World,
		const PerfectWorld::CountdownState& State,
		const PerfectWorld::CountdownContext& Ctx,
		bool bOrdinaryDayKept = false,
		const FString& CommittedOfferId = FString(),
		const FText& StatusBanner = FText::GetEmpty());

	class FApFakeAwakeningWorld final : public IApAwakeningWorld
	{
	public:
		FApFakeAwakeningWorld();

		void SetResultModel(const FPWAwakeningResultModel& InModel);
		void SetCountdownModel(const FPWCountdownModel& InModel);

		void LoadFromCoreGrant(
			const PerfectWorld::AwakeningGrant& Grant,
			const PerfectWorld::AbilityRecipeSet* Recipes = nullptr,
			bool bUnseen = true);

		void LoadFromCoreCountdown(
			PerfectWorld::GameInstant Now,
			const PerfectWorld::WorldState& World,
			const PerfectWorld::CountdownState& State,
			const PerfectWorld::CountdownContext& Ctx);

		void SeedSampleGrantedAwakening();
		void SeedSampleCountdownDay();

		virtual FPWAwakeningResultModel ReadResultModel() override;
		virtual bool HasUnseenResult() const override;
		virtual void AcknowledgeResult() override;

		virtual FPWCountdownModel ReadCountdownModel() override;
		virtual bool ConfirmPracticeOffer(const FString& OfferId, FText& OutNotice) override;
		virtual bool KeepOrdinaryDay(FText& OutNotice) override;

		int32 GetAcknowledgeCount() const { return AcknowledgeCount; }
		int32 GetConfirmOfferCount() const { return ConfirmOfferCount; }
		int32 GetKeepOrdinaryDayCount() const { return KeepOrdinaryDayCount; }
		const FString& GetLastConfirmedOfferId() const { return LastConfirmedOfferId; }

	private:
		FPWAwakeningResultModel ResultModel;
		FPWCountdownModel CountdownModel;
		int32 AcknowledgeCount = 0;
		int32 ConfirmOfferCount = 0;
		int32 KeepOrdinaryDayCount = 0;
		FString LastConfirmedOfferId;
	};

	TSharedRef<FApFakeAwakeningWorld> MakeFakeAwakeningWorld();
}
