#include "UI/PWTraitText.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTraits"

namespace PWUI
{
namespace
{
TMap<FName, FTraitText> BuildTraitTable()
{
    TMap<FName, FTraitText> Table;
    const auto Add = [&Table](const TCHAR* Id, const FText& Short, const FText& Effect)
    {
        FTraitText Entry;
        Entry.Short = Short;
        Entry.Effect = Effect;
        Table.Add(FName(Id), Entry);
    };

    Add(TEXT("physical_presence"),
        LOCTEXT("physical_presence_short", "A learned bearing that steadies first impressions."),
        LOCTEXT("physical_presence_effect", "Composure window in first-impression interactions: +1% / +2% / +3% / +4%."));
    Add(TEXT("personal_presentation"),
        LOCTEXT("personal_presentation_short", "Preparation and care taken over how you present yourself."),
        LOCTEXT("personal_presentation_effect", "Stress gained from formal school and social checks: 1% / 2% / 3% / 4% lower."));
    Add(TEXT("household_resources"),
        LOCTEXT("household_resources_short", "Where you grew up. Rank 0 means institutional care, in a children's home. Ranks 1 to 4 give a family household."),
        LOCTEXT("household_resources_effect", "Household reserve for authored household needs: Rs 500 / Rs 1,000 / Rs 2,000 / Rs 4,000. It is not cash you carry."));
    Add(TEXT("guardian_support"),
        LOCTEXT("guardian_support_short", "How much care and backing you get at home."),
        LOCTEXT("guardian_support_effect", "Stress recovery from supportive home interactions: +1% / +2% / +3% / +4%."));
    Add(TEXT("housing_stability"),
        LOCTEXT("housing_stability_short", "How secure and settled your home is."),
        LOCTEXT("housing_stability_effect", "Sleep recovery at the starting home: +1% / +2% / +3% / +4%."));
    Add(TEXT("vitality"),
        LOCTEXT("vitality_short", "The health you start with."),
        LOCTEXT("vitality_effect", "Maximum Health at start: +1% / +2% / +3% / +4%."));
    Add(TEXT("strength"),
        LOCTEXT("strength_short", "Raw physical force."),
        LOCTEXT("strength_effect", "Carry capacity and force-task tolerance: +1% / +2% / +3% / +4%."));
    Add(TEXT("agility"),
        LOCTEXT("agility_short", "Quickness and endurance on your feet."),
        LOCTEXT("agility_effect", "Maximum exertion stamina at start: +1% / +2% / +3% / +4%."));
    Add(TEXT("analytical_reasoning"),
        LOCTEXT("analytical_reasoning_short", "Working carefully through logic and problems."),
        LOCTEXT("analytical_reasoning_effect", "Progress from logic and problem-solving activities: +1% / +2% / +3% / +4%."));
    Add(TEXT("learning_discipline"),
        LOCTEXT("learning_discipline_short", "Staying with a study session."),
        LOCTEXT("learning_discipline_effect", "Focused-study efficiency before fatigue: +1% / +2% / +3% / +4%."));
    Add(TEXT("perception"),
        LOCTEXT("perception_short", "Noticing what others miss."),
        LOCTEXT("perception_effect", "More generous timing on observation prompts: +1% / +2% / +3% / +4%."));
    Add(TEXT("technical_aptitude"),
        LOCTEXT("technical_aptitude_short", "A feel for machines, circuits and code."),
        LOCTEXT("technical_aptitude_effect", "Computing and Practical Repair each start with +10 / +25 / +45 / +70 XP."));
    Add(TEXT("creative_aptitude"),
        LOCTEXT("creative_aptitude_short", "Making things and expressing ideas."),
        LOCTEXT("creative_aptitude_effect", "Communication and future Creative skill start with +10 / +25 / +45 / +70 XP."));
    Add(TEXT("athletic_aptitude"),
        LOCTEXT("athletic_aptitude_short", "Sport and physical training."),
        LOCTEXT("athletic_aptitude_effect", "Fitness starts with +10 / +25 / +45 / +70 XP."));
    Add(TEXT("empathy"),
        LOCTEXT("empathy_short", "Reading how other people feel."),
        LOCTEXT("empathy_effect", "More generous timing on emotion-reading prompts: +1% / +2% / +3% / +4%."));
    Add(TEXT("confidence"),
        LOCTEXT("confidence_short", "Speaking up and holding your ground."),
        LOCTEXT("confidence_effect", "Stress from public speaking and assertion: 1% / 2% / 3% / 4% lower."));
    Add(TEXT("community_connections"),
        LOCTEXT("community_connections_short", "People in the neighbourhood who know you."),
        LOCTEXT("community_connections_effect", "Neighbourhood contact leads at the start: 1 / 2 / 3 / 4. Leads are introductions, not automatic friends."));
    Add(TEXT("academic_foundation"),
        LOCTEXT("academic_foundation_short", "Schoolwork you have already put in."),
        LOCTEXT("academic_foundation_effect", "Academics starts with +10 / +25 / +45 / +70 XP."));
    Add(TEXT("practical_experience"),
        LOCTEXT("practical_experience_short", "Hands-on practice with household tasks and repairs."),
        LOCTEXT("practical_experience_effect", "Practical Repair and household-task familiarity start with +10 / +25 / +45 / +70 XP."));
    Add(TEXT("adversity_resilience"),
        LOCTEXT("adversity_resilience_short", "Bouncing back after setbacks."),
        LOCTEXT("adversity_resilience_effect", "Recovery from noncombat setbacks after rest or support: +1% / +2% / +3% / +4%."));
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

FText GetGroupTitle(FName GroupId)
{
    static const FName Appearance(TEXT("appearance_presentation"));
    static const FName Family(TEXT("family_resources_support"));
    static const FName Physical(TEXT("physical"));
    static const FName Cognition(TEXT("cognition"));
    static const FName Talents(TEXT("talents"));
    static const FName Social(TEXT("social"));
    static const FName Prior(TEXT("prior_experiences"));

    if (GroupId == Appearance) return LOCTEXT("GroupAppearance", "Appearance & presentation");
    if (GroupId == Family) return LOCTEXT("GroupFamily", "Family, resources & support");
    if (GroupId == Physical) return LOCTEXT("GroupPhysical", "Physical");
    if (GroupId == Cognition) return LOCTEXT("GroupCognition", "Cognition");
    if (GroupId == Talents) return LOCTEXT("GroupTalents", "Talents");
    if (GroupId == Social) return LOCTEXT("GroupSocial", "Social");
    if (GroupId == Prior) return LOCTEXT("GroupPrior", "Prior experiences");
    return FText::FromName(GroupId);
}
} // namespace PWUI

#undef LOCTEXT_NAMESPACE
