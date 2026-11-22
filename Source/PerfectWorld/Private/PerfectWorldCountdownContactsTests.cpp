#include "PerfectWorldCountdownContacts.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP315CountdownContactsTest, "PerfectWorld.Countdown.OriginContacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FP315CountdownContactsTest::RunTest(const FString& Parameters)
{
    // No UWorld, map, pawn, or hand-built copy of the role validator.
    std::vector<std::string> People;
    for (const auto& Person : PerfectWorld::KnownSchoolLifeCast())
        if (!Person.bProposedNewCast) People.emplace_back(Person.PersonId);
    People.emplace_back(PerfectWorld::FaridaPersonId);
    for (const auto& Origin : PerfectWorld::AuthoredLaneOrigins())
    {
        auto Registered = People;
        Registered.push_back(Origin.CounterpartPersonId);
        std::vector<std::string> Diagnostics;
        auto Context = PWAwakening::P315BuildCountdownContext(&Origin, Registered, Diagnostics);
        TestTrue(TEXT("the production context passes the opening validator"), PerfectWorld::ValidateCountdownContext(Context).Ok());
        TestTrue(TEXT("all registered authored contacts bind without diagnostics"), Diagnostics.empty());
        bool bCounterpart = false;
        for (const auto& Contact : Context.Contacts)
        {
            if (Contact.Person == Origin.CounterpartPersonId)
            {
                bCounterpart = true;
                TestTrue(TEXT("sibling is reserved for an actual sibling"),
                    (Contact.Role == "sibling") == (Origin.HouseholdTemplateId == "adult_sibling_household"));
                TestTrue(TEXT("care worker is reserved for the care origin"),
                    (Contact.Role == "care_worker") == Origin.GrewUpInCare);
            }
        }
        TestTrue(TEXT("the actual origin counterpart is bound"), bCounterpart);
        const auto Count = Context.Contacts.size();
        TestFalse(TEXT("an unfair role is left unbound"), PWAwakening::P315BindCountdownContact(
            Context, {"family_adult", Origin.CounterpartPersonId}, Registered, Diagnostics));
        TestFalse(TEXT("an unregistered person is left unbound"), PWAwakening::P315BindCountdownContact(
            Context, {"friend", "person.missing"}, Registered, Diagnostics));
        Registered.emplace_back("player");
        TestFalse(TEXT("the player cannot be their own contact"), PWAwakening::P315BindCountdownContact(
            Context, {"friend", "player"}, Registered, Diagnostics));
        TestTrue(TEXT("the invalid optional contact cannot poison time input"),
            Context.Contacts.size() == Count && PerfectWorld::ValidateCountdownContext(Context).Ok());
    }
    std::vector<std::string> Diagnostics;
    const auto Empty = PWAwakening::P315BuildCountdownContext(nullptr, {}, Diagnostics);
    TestTrue(TEXT("an unavailable roster is diagnosed and leaves a valid empty context"),
        Empty.Contacts.empty() && !Diagnostics.empty() && PerfectWorld::ValidateCountdownContext(Empty).Ok());
    auto Altered = PerfectWorld::AuthoredLaneOrigins().front();
    Altered.CounterpartPersonId = "person.missing";
    Diagnostics.clear();
    const auto Unresolved = PWAwakening::P315BuildCountdownContext(&Altered, People, Diagnostics);
    TestTrue(TEXT("an altered origin cannot fabricate a contact"),
        !Diagnostics.empty() && PerfectWorld::ValidateCountdownContext(Unresolved).Ok());
    return true;
}
#endif
