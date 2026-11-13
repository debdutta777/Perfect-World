#include "UI/PWTraitText.h"

#include "CreationPresentation.h"
#include "PerfectWorldCreationAdapter.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraits"

namespace PWUI
{
namespace
{
FText CoreText(std::string_view Source)
{
    if (Source.empty()) return FText::GetEmpty();
    const FUTF8ToTCHAR Converted(Source.data(), static_cast<int32>(Source.size()));
    return FText::FromString(FString(Converted.Length(), Converted.Get()));
}

FText FormatRankEffects(const PerfectWorld::TraitRankEffect& Rank)
{
    return FText::Join(LOCTEXT("LineBreak", "\n"), PWCreationPresentation::RankEffectTexts(Rank));
}

TMap<FName, FTraitText> BuildTraitTable()
{
    TMap<FName, FTraitText> Table;
    for (const PerfectWorld::TraitPresentationEntry& Source : PerfectWorld::TraitPresentations())
    {
        FTraitText Entry;
        Entry.Short = CoreText(Source.Description);
        TArray<FText> Ranks;
        Ranks.Reserve(PerfectWorld::MaxRank);
        for (int32 Rank = 1; Rank <= PerfectWorld::MaxRank; ++Rank)
        {
            const FText Effects = FormatRankEffects(Source.Ranks[static_cast<std::size_t>(Rank)]);
            Ranks.Add(FText::Format(LOCTEXT("RankEffects", "Rank {0}: {1}"), FText::AsNumber(Rank), Effects));
        }
        Entry.Effect = FText::Join(LOCTEXT("RankSeparator", "\n"), Ranks);
        Table.Add(FName(CoreText(Source.TraitId).ToString()), MoveTemp(Entry));
    }
    return Table;
}
} // namespace

const FTraitText& GetTraitText(FName TraitId)
{
    static const TMap<FName, FTraitText> Table = BuildTraitTable();
    static const FTraitText Empty;
    const FTraitText* Found = Table.Find(TraitId);
    return Found != nullptr ? *Found : Empty;
}

FText GetTraitRankMeaning(FName TraitId, int32 Rank)
{
    const FString Id = TraitId.ToString();
    const auto* Source = PerfectWorld::RankEffect(TCHAR_TO_UTF8(*Id), Rank);
    return Source != nullptr ? CoreText(Source->Meaning) : FText::GetEmpty();
}

FText FormatTraitRankEffects(FName TraitId, int32 Rank)
{
    const FString Id = TraitId.ToString();
    const auto* Source = PerfectWorld::RankEffect(TCHAR_TO_UTF8(*Id), Rank);
    return Source != nullptr ? FormatRankEffects(*Source) : FText::GetEmpty();
}

FText GetGroupTitle(FName GroupId)
{
    const FString Id = GroupId.ToString();
    const auto* Group = PerfectWorld::FindTraitGroup(TCHAR_TO_UTF8(*Id));
    return Group != nullptr ? CoreText(Group->DisplayName) : FText::FromName(GroupId);
}
} // namespace PWUI

#undef LOCTEXT_NAMESPACE
