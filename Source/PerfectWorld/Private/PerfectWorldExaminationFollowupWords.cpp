#include "PerfectWorldExaminationBridge.h"

// P506: compiled fallback English mirrors the authored phase2_a8 bundle.
#define LOCTEXT_NAMESPACE "P407UFollowupWords"

FText P406Exam::P407UFollowupText(const std::string& Key)
{
    if (Key == "phase2.a8.apprenticeship.title") return LOCTEXT("phase2.a8.apprenticeship.title", "Request a workshop assessment");
    if (Key == "phase2.a8.apprenticeship.brief") return LOCTEXT("phase2.a8.apprenticeship.brief", "Bring one repair question to the school careers desk. Ask for a supervised visit to the Nabapara repair workshop; no placement or licence is promised.");
    if (Key == "phase2.a8.study_bridge.title") return LOCTEXT("phase2.a8.study_bridge.title", "Book a foundation study adviser meeting");
    if (Key == "phase2.a8.study_bridge.brief") return LOCTEXT("phase2.a8.study_bridge.brief", "Bring a recent homework page. Choose one topic to practise with the adviser and ask about foundation study and later adult bridge routes.");
    if (Key == "phase2.a8.community_practice.title") return LOCTEXT("phase2.a8.community_practice.title", "Ask about a community logistics placement");
    if (Key == "phase2.a8.community_practice.brief") return LOCTEXT("phase2.a8.community_practice.brief", "At the school careers desk, ask to observe a supervised stock-and-delivery planning session. Agree hours that leave school and home commitments possible.");
    if (Key == "phase2.a8.retake.title") return LOCTEXT("phase2.a8.retake.title", "Talk with your teacher about next steps");
    if (Key == "phase2.a8.retake.brief") return LOCTEXT("phase2.a8.retake.brief", "Meet your teacher at the Nabapara Vidyalaya office for 15 minutes to talk over ordinary Class XI next steps and school support. Attending is optional and carries no obligation.");
    if (Key == "phase2.a8.equipment.title") return LOCTEXT("phase2.a8.equipment.title", "Apply for a practice-equipment allowance");
    if (Key == "phase2.a8.equipment.brief") return LOCTEXT("phase2.a8.equipment.brief", "Bring a list of equipment you would use to the school careers desk. Ask about terms and application evidence. The displayed envelope is not money in your wallet.");
    if (Key == "phase2.a8.licence.title") return LOCTEXT("phase2.a8.licence.title", "Request a supervised licence assessment");
    if (Key == "phase2.a8.licence.brief") return LOCTEXT("phase2.a8.licence.brief", "Ask the careers adviser to review your hunter credential and explain supervision and eligibility. A tower credential alone does not grant a licence.");
    if (Key == "phase2.a8.patronage.title") return LOCTEXT("phase2.a8.patronage.title", "Prepare a portfolio for review");
    if (Key == "phase2.a8.patronage.brief") return LOCTEXT("phase2.a8.patronage.brief", "Bring a piece of school or independent work. Ask the careers adviser which sponsorship review accepts it and what commitments a sponsor requests.");
    if (Key == "phase2.a8.public_portfolio.title") return LOCTEXT("phase2.a8.public_portfolio.title", "Decide whether to share your portfolio");
    if (Key == "phase2.a8.public_portfolio.brief") return LOCTEXT("phase2.a8.public_portfolio.brief", "Meet the school adviser to review exactly which credentials and work would be public. Keeping the portfolio private remains a complete choice.");
    if (Key == "phase2.a8.scholarship.title") return LOCTEXT("phase2.a8.scholarship.title", "Request a scholarship interview");
    if (Key == "phase2.a8.scholarship.brief") return LOCTEXT("phase2.a8.scholarship.brief", "Bring your credential portfolio to the school careers desk. Ask about interview dates, study requirements and support terms; an offer is an application opportunity.");
    if (Key == "phase2.a8.enterprise.title") return LOCTEXT("phase2.a8.enterprise.title", "Book a proposal assessment");
    if (Key == "phase2.a8.enterprise.brief") return LOCTEXT("phase2.a8.enterprise.brief", "Bring a small service or workshop proposal. Ask the careers adviser about a supervised starting-capital review and its obligations; no loan is accepted here.");
    if (Key == "phase2.a8.strong.title") return LOCTEXT("phase2.a8.strong.title", "Make room after a strong result");
    if (Key == "phase2.a8.strong.brief") return LOCTEXT("phase2.a8.strong.brief", "Talk with your class teacher after lessons about interviews and the school work you still want to do. You do not have to publish your rank or take a competitive path.");
    if (Key == "phase2.a8.middle.title") return LOCTEXT("phase2.a8.middle.title", "Choose a next step at your own pace");
    if (Key == "phase2.a8.middle.brief") return LOCTEXT("phase2.a8.middle.brief", "Bring one tower observation to your class teacher. Compare an apprenticeship enquiry with a study bridge, then keep or change your ordinary school plan.");
    if (Key == "phase2.a8.poor.title") return LOCTEXT("phase2.a8.poor.title", "Ask for a practical next step");
    if (Key == "phase2.a8.poor.brief") return LOCTEXT("phase2.a8.poor.brief", "Meet your class teacher after lessons. Choose a workshop enquiry, a foundation study meeting or community logistics practice. There is no remedial punishment and no requirement to fight.");
    if (Key == "phase2.a8.withdrawn.title") return LOCTEXT("phase2.a8.withdrawn.title", "Talk through leaving the tower");
    if (Key == "phase2.a8.withdrawn.brief") return LOCTEXT("phase2.a8.withdrawn.brief", "If you want, tell your class teacher what made you stop. Earlier credentials remain on the result. Choose an ordinary school week or an application meeting without replaying the tower.");
    if (Key == "phase2.a8.deferred.title") return LOCTEXT("phase2.a8.deferred.title", "Plan a school week without this year's exam");
    if (Key == "phase2.a8.deferred.brief") return LOCTEXT("phase2.a8.deferred.brief", "Meet the school office after lessons to ask about next year's examination and its age limit, or focus on class work. Deferral does not reserve admission or interrupt school.");
    if (Key == "phase2.a8.declined.title") return LOCTEXT("phase2.a8.declined.title", "Explore a route without the examination");
    if (Key == "phase2.a8.declined.brief") return LOCTEXT("phase2.a8.declined.brief", "Ask the careers adviser about workshop apprenticeships and foundation or adult bridge qualifications. Continue Class XI; declining the examination closes nothing else at school.");
    if (Key == "phase2.a8.school.title") return LOCTEXT("phase2.a8.school.title", "Return to your next school day");
    if (Key == "phase2.a8.school.brief") return LOCTEXT("phase2.a8.school.brief", "Open the school timetable and attend the next scheduled class. The calendar, stream and school closures still govern attendance; the tower result does not cancel lessons.");
    if (Key == "phase2.a8.study.title") return LOCTEXT("phase2.a8.study.title", "Work on a homework question");
    if (Key == "phase2.a8.study.brief") return LOCTEXT("phase2.a8.study.brief", "Open the study desk at home or in the care home's study room. Choose a brief or ordinary study activity; existing deadlines and activity costs remain in force.");
    if (Key == "phase2.a8.rest.title") return LOCTEXT("phase2.a8.rest.title", "Leave room for rest");
    if (Key == "phase2.a8.rest.brief") return LOCTEXT("phase2.a8.rest.brief", "Open the ordinary day planner and choose a rest activity when you want it. You can spend the following week at school and home without pursuing any tower offer.");
    if (Key == "phase2.a8.credential.tower.hunter") return LOCTEXT("phase2.a8.credential.tower.hunter", "Hunter");
    if (Key == "phase2.a8.credential.tower.scholar") return LOCTEXT("phase2.a8.credential.tower.scholar", "Scholar");
    if (Key == "phase2.a8.credential.tower.engineer") return LOCTEXT("phase2.a8.credential.tower.engineer", "Engineer");
    if (Key == "phase2.a8.credential.tower.enterprise") return LOCTEXT("phase2.a8.credential.tower.enterprise", "Enterprise");
    if (Key == "phase2.a8.credential.tower.athlete") return LOCTEXT("phase2.a8.credential.tower.athlete", "Athlete");
    if (Key == "phase2.a8.credential.tower.clinician") return LOCTEXT("phase2.a8.credential.tower.clinician", "Clinician");
    if (Key == "phase2.a8.credential.tower.public_leader") return LOCTEXT("phase2.a8.credential.tower.public_leader", "Public leader");
    if (Key == "phase2.a8.credential.tower.logistician") return LOCTEXT("phase2.a8.credential.tower.logistician", "Logistician");
    if (Key == "phase2.a8.credential.tower.artist") return LOCTEXT("phase2.a8.credential.tower.artist", "Artist");
    if (Key == "phase2.a8.credential.tower.ecologist") return LOCTEXT("phase2.a8.credential.tower.ecologist", "Ecologist");
    if (Key == "phase2.a8.title") return LOCTEXT("phase2.a8.title", "After the tower");
    if (Key == "phase2.a8.continuity") return LOCTEXT("phase2.a8.continuity", "Class XI, friendships and life at home continue. These are optional next steps. Reading and planning cost no time.");
    if (Key == "phase2.a8.pending") return LOCTEXT("phase2.a8.pending", "A final tower decision is not available yet. Return to your current activity.");
    if (Key == "phase2.a8.close") return LOCTEXT("phase2.a8.close", "Back");
    if (Key == "phase2.a8.rejected") return LOCTEXT("phase2.a8.rejected", "That next step is unavailable. Refresh your plan or return to school.");
    if (Key == "phase2.a8.accepted") return LOCTEXT("phase2.a8.accepted", "Opened your next step. A booking exists only when the destination confirms it.");
    if (Key == "phase2.a8.next") return LOCTEXT("phase2.a8.next", "Choose this next step");
    if (Key == "phase2.a8.p4_strong_scholarship_interview.title") return LOCTEXT("phase2.a8.p4_strong_scholarship_interview.title", "Attend the Bhagirathi scholarship briefing");
    if (Key == "phase2.a8.p4_strong_scholarship_interview.brief") return LOCTEXT("phase2.a8.p4_strong_scholarship_interview.brief", "Meet the visiting adviser from Bhagirathi State University in the Nabapara Vidyalaya office after lessons. Ask how a strong tower portfolio pairs with Class XII board marks for a scholarship interview; attending carries no obligation to apply.");
    if (Key == "phase2.a8.p4_strong_mentor.title") return LOCTEXT("phase2.a8.p4_strong_mentor.title", "Draft a mentor term plan with Amrita Basu");
    if (Key == "phase2.a8.p4_strong_mentor.brief") return LOCTEXT("phase2.a8.p4_strong_mentor.brief", "Meet Amrita Basu in the Nabapara Vidyalaya staff room for 20 minutes to sort incoming invitations, pick one direction to explore, and name one enquiry to decline. Completing the session records your term plan on your next steps; no admission or funding is awarded here. Attending is optional and you may refuse or cancel with no obligation, keeping an unhurried Class XI week.");
    if (Key == "phase2.a8.p4_middle_coaching_scholarship.title") return LOCTEXT("phase2.a8.p4_middle_coaching_scholarship.title", "Enquire about a fee-waiver coaching scholarship");
    if (Key == "phase2.a8.p4_middle_coaching_scholarship.brief") return LOCTEXT("phase2.a8.p4_middle_coaching_scholarship.brief", "Speak to the Nabapara Vidyalaya careers adviser about the municipal evening study circle's fee-waiver scholarship for Class XI and XII students. Ask about seat dates and evening hours before deciding whether you want extra tuition.");
    if (Key == "phase2.a8.p4_middle_stream_review.title") return LOCTEXT("phase2.a8.p4_middle_stream_review.title", "Review your Class XI stream with Amrita Basu");
    if (Key == "phase2.a8.p4_middle_stream_review.brief") return LOCTEXT("phase2.a8.p4_middle_stream_review.brief", "Meet Amrita Basu at the Nabapara Vidyalaya office for 25 minutes to compare Science, Commerce, and Humanities options for next term. Completing the session records a tentative stream and a preview note showing which next-term period it would add, without changing your current timetable or awarding admission. Changing direction is ordinary, and you may refuse or cancel with no obligation to keep your present stream.");
    if (Key == "phase2.a8.p4_poor_workshop_apprenticeship.title") return LOCTEXT("phase2.a8.p4_poor_workshop_apprenticeship.title", "Complete a bench repair step with Debashis Pal");
    if (Key == "phase2.a8.p4_poor_workshop_apprenticeship.brief") return LOCTEXT("phase2.a8.p4_poor_workshop_apprenticeship.brief", "Work beside Debashis Pal at the Nabapara Lane repair workshop for 45 minutes to diagnose a small appliance and finish one hands-on step, either reseating a connector or securing its housing. Finishing the work records your completed repair step and a workshop reference from Debashis Pal; any talk of polytechnic bridges or apprenticeships is an enquiry only, with no wage, placement, or licence promised. You may refuse or cancel with no obligation, and ordinary school and home routines stay open.");
    if (Key == "phase2.a8.p4_poor_retake_prep.title") return LOCTEXT("phase2.a8.p4_poor_retake_prep.title", "Join a practical study session at school");
    if (Key == "phase2.a8.p4_poor_retake_prep.brief") return LOCTEXT("phase2.a8.p4_poor_retake_prep.brief", "Join Amrita Basu and classmates in the Nabapara Vidyalaya library for a 40-minute practical study session focused on ordinary Class XI coursework. Attending is optional and you may leave or refuse with no obligation.");
    if (Key == "phase2.a8.p4_withdrawn_home_rest.title") return LOCTEXT("phase2.a8.p4_withdrawn_home_rest.title", "Take an unhurried evening at home");
    if (Key == "phase2.a8.p4_withdrawn_home_rest.brief") return LOCTEXT("phase2.a8.p4_withdrawn_home_rest.brief", "Spend a quiet hour at home, or with the staff and residents at the children's home, over tea and supper, with no exam talk required. Stepping out of the tower takes judgement, and your evening belongs to recovery.");
    if (Key == "phase2.a8.p4_withdrawn_school_mentor.title") return LOCTEXT("phase2.a8.p4_withdrawn_school_mentor.title", "Set a pacing plan with Amrita Basu");
    if (Key == "phase2.a8.p4_withdrawn_school_mentor.brief") return LOCTEXT("phase2.a8.p4_withdrawn_school_mentor.brief", "Meet Amrita Basu at Nabapara Vidyalaya for 30 minutes, alongside a visiting note from Dr. Ananya Chatterjee's clinic, to choose either lighter evening study for two weeks or your usual rhythm and put any floor credential you already earned on your next-steps record. Completing the session records your chosen pacing plan and retained credential as a schedule note, with no admission, licence, or further examination sitting offered and no need to justify stepping out early. You may refuse or cancel with no obligation.");
    if (Key == "phase2.a8.p4_deferred_year_study_plan.title") return LOCTEXT("phase2.a8.p4_deferred_year_study_plan.title", "Draft a twelve-month preparation calendar");
    if (Key == "phase2.a8.p4_deferred_year_study_plan.brief") return LOCTEXT("phase2.a8.p4_deferred_year_study_plan.brief", "Set aside a quiet session at your desk at home or in the care home's study room to balance Class XI lessons, rest and steady preparation for next year's examination. Deferring gives you a full year on your own terms.");
    if (Key == "phase2.a8.p4_deferred_community_volunteer.title") return LOCTEXT("phase2.a8.p4_deferred_community_volunteer.title", "Enquire about volunteering with Dr. Ananya Chatterjee's clinic");
    if (Key == "phase2.a8.p4_deferred_community_volunteer.brief") return LOCTEXT("phase2.a8.p4_deferred_community_volunteer.brief", "Spend 30 minutes at the Nabapara Vidyalaya notice desk to submit an enquiry for Dr. Ananya Chatterjee's Community Clinic, choosing either the neighbourhood reading table or afternoon patient-card sorting around your classes and household routine. Completing the enquiry records your volunteer request and a proposed next-week calendar note; it is unpaid, carries no automatic enrolment or standing commitment, and does not book any examination sitting. You may refuse or cancel with no obligation.");
    if (Key == "phase2.a8.p4_declined_vocational_craft.title") return LOCTEXT("phase2.a8.p4_declined_vocational_craft.title", "Talk trade apprenticeships at the repair workshop");
    if (Key == "phase2.a8.p4_declined_vocational_craft.brief") return LOCTEXT("phase2.a8.p4_declined_vocational_craft.brief", "Meet Debashis Pal at the Nabapara Lane repair workshop after school to ask about electrical repair apprenticeships, polytechnic diplomas and neighbourhood shop work. Declining the tower leaves every ordinary craft and trade open.");
    if (Key == "phase2.a8.p4_declined_stream_civic_path.title") return LOCTEXT("phase2.a8.p4_declined_stream_civic_path.title", "Map a board-exam and civic route with Amrita Basu");
    if (Key == "phase2.a8.p4_declined_stream_civic_path.brief") return LOCTEXT("phase2.a8.p4_declined_stream_civic_path.brief", "Meet Amrita Basu at Nabapara Vidyalaya for 30 minutes to pair your Class XII board subjects with a local civic activity such as the neighbourhood reading table or neighbourhood records, and review direct degree pathways at Bhagirathi State University. Completing the session records your route sheet and a board-subject timetable note; university entry stays an enquiry rather than an award, and a full adult life requires neither a tower rank nor supernatural practice. You may refuse or cancel with no obligation.");
    if (Key == "phase2.a8.p407u.mentor.session") return LOCTEXT("phase2.a8.p407u.mentor.session", "Amrita Basu sets your result beside the school timetable. You discuss managing pressure and where your retained evidence can be useful. Ordinary school and paths beyond the tower remain open. The session ends with one manageable next step for the coming week.");
    return LOCTEXT("Missing", "This next step has no available description.");
}

FText P406Exam::P506TaskWords(const std::string& Token)
{
    if (Token == "explore_study") return LOCTEXT("p506.explore_study", "Explore study and decline the hunter enquiry");
    if (Token == "explore_civic") return LOCTEXT("p506.explore_civic", "Explore civic work and decline the university enquiry");
    if (Token == "science") return LOCTEXT("p506.science", "Science");
    if (Token == "commerce") return LOCTEXT("p506.commerce", "Commerce");
    if (Token == "humanities") return LOCTEXT("p506.humanities", "Humanities");
    if (Token == "reseat_connector") return LOCTEXT("p506.reseat_connector", "Reseat the connector");
    if (Token == "secure_housing") return LOCTEXT("p506.secure_housing", "Secure the housing");
    if (Token == "lighter_evenings") return LOCTEXT("p506.lighter_evenings", "Plan lighter evenings for two weeks");
    if (Token == "usual_rhythm") return LOCTEXT("p506.usual_rhythm", "Keep your usual rhythm");
    if (Token == "reading_table") return LOCTEXT("p506.reading_table", "Request the neighbourhood reading table");
    if (Token == "patient_cards") return LOCTEXT("p506.patient_cards", "Request afternoon patient-card sorting");
    if (Token == "science_reading") return LOCTEXT("p506.science_reading", "Pair Science board subjects with the reading table");
    if (Token == "humanities_civic") return LOCTEXT("p506.humanities_civic", "Pair Humanities board subjects with neighbourhood records");
    if (Token == "term.explore.study") return LOCTEXT("p506.term.explore.study", "Term plan: explore study");
    if (Token == "term.decline.hunter_enquiry") return LOCTEXT("p506.term.decline.hunter_enquiry", "Term plan: decline the hunter enquiry");
    if (Token == "term.explore.civic") return LOCTEXT("p506.term.explore.civic", "Term plan: explore civic work");
    if (Token == "term.decline.university_enquiry") return LOCTEXT("p506.term.decline.university_enquiry", "Term plan: decline the university enquiry");
    if (Token == "stream.tentative.science") return LOCTEXT("p506.stream.tentative.science", "Tentative stream: Science");
    if (Token == "stream.tentative.commerce") return LOCTEXT("p506.stream.tentative.commerce", "Tentative stream: Commerce");
    if (Token == "stream.tentative.humanities") return LOCTEXT("p506.stream.tentative.humanities", "Tentative stream: Humanities");
    if (Token == "timetable.preview.science_practical") return LOCTEXT("p506.timetable.preview.science_practical", "Next-term preview note: Science practical");
    if (Token == "timetable.preview.accounting") return LOCTEXT("p506.timetable.preview.accounting", "Next-term preview note: Accounting");
    if (Token == "timetable.preview.civics") return LOCTEXT("p506.timetable.preview.civics", "Next-term preview note: Civics");
    if (Token == "practical.connector_reseated") return LOCTEXT("p506.practical.connector_reseated", "Finished repair step: connector reseated");
    if (Token == "practical.housing_secured") return LOCTEXT("p506.practical.housing_secured", "Finished repair step: housing secured");
    if (Token == "workshop.reference.debashis_pal") return LOCTEXT("p506.workshop.reference.debashis_pal", "Workshop reference: Debashis Pal");
    if (Token == "pacing.lighter_evenings.two_weeks") return LOCTEXT("p506.pacing.lighter_evenings.two_weeks", "Schedule note: lighter evening study for two weeks");
    if (Token == "pacing.usual_rhythm") return LOCTEXT("p506.pacing.usual_rhythm", "Schedule note: usual rhythm");
    if (Token == "volunteer.request.reading_table") return LOCTEXT("p506.volunteer.request.reading_table", "Volunteer enquiry: neighbourhood reading table");
    if (Token == "volunteer.request.patient_cards") return LOCTEXT("p506.volunteer.request.patient_cards", "Volunteer enquiry: afternoon patient-card sorting");
    if (Token == "calendar.proposed.next_week.reading_table") return LOCTEXT("p506.calendar.proposed.next_week.reading_table", "Proposed next-week note: reading table");
    if (Token == "calendar.proposed.next_week.patient_cards") return LOCTEXT("p506.calendar.proposed.next_week.patient_cards", "Proposed next-week note: patient-card sorting");
    if (Token == "boards.science") return LOCTEXT("p506.boards.science", "Route sheet: Science board subjects");
    if (Token == "boards.humanities") return LOCTEXT("p506.boards.humanities", "Route sheet: Humanities board subjects");
    if (Token == "civic.reading_table") return LOCTEXT("p506.civic.reading_table", "Route sheet: neighbourhood reading table");
    if (Token == "civic.neighbourhood_records") return LOCTEXT("p506.civic.neighbourhood_records", "Route sheet: neighbourhood records");
    if (Token == "timetable.note.board_subjects") return LOCTEXT("p506.timetable.note.board_subjects", "Timetable note: board subjects");
    if (Token == "school.lessons") return LOCTEXT("p506.school.lessons", "Ordinary school lessons");
    if (Token == "activity.rest") return LOCTEXT("p506.activity.rest", "Rest");
    if (Token == "activity.study_at_home") return LOCTEXT("p506.activity.study_at_home", "Home or care-home study");
    if (Token == "activity.household_chores") return LOCTEXT("p506.activity.household_chores", "Household or care-home rota");
    if (Token == "tower.hunter") return LOCTEXT("p506.tower.hunter", "Retained Hunter credential");
    if (Token == "tower.scholar") return LOCTEXT("p506.tower.scholar", "Retained Scholar credential");
    if (Token == "tower.engineer") return LOCTEXT("p506.tower.engineer", "Retained Engineer credential");
    if (Token == "tower.enterprise") return LOCTEXT("p506.tower.enterprise", "Retained Enterprise credential");
    if (Token == "tower.athlete") return LOCTEXT("p506.tower.athlete", "Retained Athlete credential");
    // Unknown public tokens remain readable without fabricating a new consequence.
    FString Words(UTF8_TO_TCHAR(Token.c_str()));
    Words.ReplaceInline(TEXT("."), TEXT(" "));
    Words.ReplaceInline(TEXT("_"), TEXT(" "));
    return FText::FromString(Words);
}

#undef LOCTEXT_NAMESPACE

