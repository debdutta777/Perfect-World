// NL-01 fixture generator. Built against a historical snapshot of the portable core (git archive <commit>), never against
// the working tree, so the file it writes is what that commit's writer really produced. Usage: GenDoorsFixture <out.json>
#include "CharacterCreation.h"
#include "Doors.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
void Need(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "GEN FAIL: " << message << '\n';
        std::exit(1);
    }
}

void Submit(WorldSim& sim, CommandSource source, CommandPayload payload, const char* message)
{
    const CommandResult result = sim.Submit({source, std::move(payload)});
    if (!result.Accepted)
        std::cerr << "refused: " << result.Why.Code << '\n';
    Need(result.Accepted, message);
}
} // namespace

int main(int argc, char** argv)
{
    Need(argc == 2, "usage: GenDoorsFixture <out.json>");
    CharacterCreator creator;
    Need(creator.ApplyPreset("balanced_scholar").Succeeded(), "preset");
    Need(creator.SetHouseholdTemplate("parent_household").Succeeded(), "household");
    Need(creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity");
    Need(creator.StartLife().Succeeded(), "start life");

    WorldSaveData data;
    Need(BeginWorldFromCreation(creator.CaptureSave(), 0x4e4c3031ULL, data).Succeeded(), "begin world");
    WorldSim sim(data.World);
    const std::string person(HouseholdAdultId);
    Submit(sim, CommandSource::Player, AdvanceTime{720}, "advance to the evening");
    Submit(sim, CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}, "open household talk");
    const std::uint32_t session = sim.State().Conversations.at(person).SessionCount;
    Submit(sim, CommandSource::Player, ConversationChoose{person, "choice.household_adult.talk_rota", session}, "paid choice");
    Submit(sim, CommandSource::Player, ConversationLeave{person}, "leave talk");
    // Non-default physical facts with both swing signs, so a migration that reset them would be caught.
    Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.home_front", true, -1}, "front open outward");
    Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.home_bedroom", true, 1}, "bedroom open inward");
    Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.home_living", false, 0}, "living closed");
    Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.shanti_chai_shutter", true, -1}, "shutter open");
    Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.care_common", false, 0}, "care common closed");
    if (FindDoor("door.nabapara.stationery_front") != nullptr)
        Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.stationery_front", true, -1}, "stationery open");
    // NL-01 doors (sixteen-door core, ec27cc7): a two-way leaf open outward and a one-way dwelling leaf open inward.
    if (FindDoor("door.nabapara.tall_flats_front") != nullptr)
        Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.tall_flats_front", true, -1}, "flats entry open");
    if (FindDoor("door.nabapara.tea_row_house_front") != nullptr)
        Submit(sim, CommandSource::Presentation, DoorSettle{"door.nabapara.tea_row_house_front", true, 1}, "row house open");
    Submit(sim, CommandSource::Player, AdvanceTime{37}, "a little later");

    data.World = sim.State();
    data.Player.ZoneId = "zone.street.nabapara_lane";
    data.Player.PositionCm = {120, -3400, 20};
    data.Player.YawCdeg = 9000;
    const std::string text = SerializeWorldSave(data);
    Need(!text.empty(), "writer produced text");
    WorldSaveData back;
    const WorldSaveResult loaded = DeserializeWorldSave(text, back);
    Need(loaded.Succeeded() && loaded.HashVerified, "the writing commit certifies its own save");
    std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
    out << text;
    Need(static_cast<bool>(out), "write file");
    std::cout << "doors " << AuthoredDoors().size() << " bytes " << text.size() << '\n';
    return 0;
}
