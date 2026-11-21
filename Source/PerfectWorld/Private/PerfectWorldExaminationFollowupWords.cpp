#include "PerfectWorldExaminationBridge.h"

#define LOCTEXT_NAMESPACE "P407UFollowupWords"

FText P406Exam::P407UFollowupText(const std::string& Key)
{
    if (Key == "phase2.a8.apprenticeship.title") return LOCTEXT("phase2.a8.apprenticeship.title", "Request a workshop assessment");
    if (Key == "phase2.a8.apprenticeship.brief") return LOCTEXT("phase2.a8.apprenticeship.brief", "Bring one repair question to the school careers desk. Ask for a supervised visit to the Nabapara repair workshop; no placement or licence is promised.");
    if (Key == "phase2.a8.study_bridge.title") return LOCTEXT("phase2.a8.study_bridge.title", "Book a foundation study adviser meeting");
    if (Key == "phase2.a8.study_bridge.brief") return LOCTEXT("phase2.a8.study_bridge.brief", "Bring a recent homework page. Choose one topic to practise with the adviser and ask about foundation study and later adult bridge routes.");
    if (Key == "phase2.a8.community_practice.title") return LOCTEXT("phase2.a8.community_practice.title", "Ask about a community logistics placement");
    if (Key == "phase2.a8.community_practice.brief") return LOCTEXT("phase2.a8.community_practice.brief", "At the school careers desk, ask to observe a supervised stock-and-delivery planning session. Agree hours that leave school and home commitments possible.");
    if (Key == "phase2.a8.retake.title") return LOCTEXT("phase2.a8.retake.title", "Discuss next year's examination");
    if (Key == "phase2.a8.retake.brief") return LOCTEXT("phase2.a8.retake.brief", "Ask the school office about annual retake registration, allowed up to age twenty. You can leave without registering and continue Class XI.");
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
    if (Key == "phase2.a8.declined.brief") return LOCTEXT("phase2.a8.declined.brief", "Ask the careers adviser about workshop apprenticeships and foundation or adult bridge qualifications. Continue Class XI; you do not have to register for another tower attempt.");
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
    if (Key == "phase2.a8.p4_strong_mentor.title") return LOCTEXT("phase2.a8.p4_strong_mentor.title", "Arrange a mentor term plan with Amrita Basu");
    if (Key == "phase2.a8.p4_strong_mentor.brief") return LOCTEXT("phase2.a8.p4_strong_mentor.brief", "Sit with Amrita Basu in the Nabapara Vidyalaya staff room to sort through invitations from places such as Astra Research Institute and Hooghly Tide Gate Academy. You may turn any of them down and keep an unhurried Class XI year.");
    if (Key == "phase2.a8.p4_middle_coaching_scholarship.title") return LOCTEXT("phase2.a8.p4_middle_coaching_scholarship.title", "Enquire about a fee-waiver coaching scholarship");
    if (Key == "phase2.a8.p4_middle_coaching_scholarship.brief") return LOCTEXT("phase2.a8.p4_middle_coaching_scholarship.brief", "Speak to the Nabapara Vidyalaya careers adviser about the municipal evening study circle's fee-waiver scholarship for Class XI and XII students. Ask about seat dates and evening hours before deciding whether you want extra tuition.");
    if (Key == "phase2.a8.p4_middle_stream_review.title") return LOCTEXT("phase2.a8.p4_middle_stream_review.title", "Review your Class XI stream and subjects");
    if (Key == "phase2.a8.p4_middle_stream_review.brief") return LOCTEXT("phase2.a8.p4_middle_stream_review.brief", "Meet Amrita Basu at the Nabapara Vidyalaya office to compare Science, Commerce and Humanities subject options for next term. Changing direction after the examination is an ordinary choice, not a setback.");
    if (Key == "phase2.a8.p4_poor_workshop_apprenticeship.title") return LOCTEXT("phase2.a8.p4_poor_workshop_apprenticeship.title", "Visit Debashis Pal's repair workshop");
    if (Key == "phase2.a8.p4_poor_workshop_apprenticeship.brief") return LOCTEXT("phase2.a8.p4_poor_workshop_apprenticeship.brief", "Drop into the Nabapara Lane repair workshop after school to watch Debashis Pal diagnose a small appliance and talk about vocational apprenticeships and polytechnic bridges. A low tower rank has no bearing on bench craft.");
    if (Key == "phase2.a8.p4_poor_retake_prep.title") return LOCTEXT("phase2.a8.p4_poor_retake_prep.title", "Join the calm retake preparation table");
    if (Key == "phase2.a8.p4_poor_retake_prep.brief") return LOCTEXT("phase2.a8.p4_poor_retake_prep.brief", "Meet Amrita Basu with classmates in the Nabapara Vidyalaya library to map a steady, low-pressure plan for next year's examination, allowed up to age twenty. You can focus on non-combat routes or walk away whenever you prefer.");
    if (Key == "phase2.a8.p4_withdrawn_home_rest.title") return LOCTEXT("phase2.a8.p4_withdrawn_home_rest.title", "Take an unhurried evening at home");
    if (Key == "phase2.a8.p4_withdrawn_home_rest.brief") return LOCTEXT("phase2.a8.p4_withdrawn_home_rest.brief", "Spend a quiet hour at home, or with the staff and residents at the children's home, over tea and supper, with no exam talk required. Stepping out of the tower takes judgement, and your evening belongs to recovery.");
    if (Key == "phase2.a8.p4_withdrawn_school_mentor.title") return LOCTEXT("phase2.a8.p4_withdrawn_school_mentor.title", "Meet a school mentor on pacing and next steps");
    if (Key == "phase2.a8.p4_withdrawn_school_mentor.brief") return LOCTEXT("phase2.a8.p4_withdrawn_school_mentor.brief", "Talk with Amrita Basu at Nabapara Vidyalaya, alongside a visiting note from Dr. Ananya Chatterjee's clinic, about managing pressure and putting your earlier floor credentials to work. You never have to explain away leaving early.");
    if (Key == "phase2.a8.p4_deferred_year_study_plan.title") return LOCTEXT("phase2.a8.p4_deferred_year_study_plan.title", "Draft a twelve-month preparation calendar");
    if (Key == "phase2.a8.p4_deferred_year_study_plan.brief") return LOCTEXT("phase2.a8.p4_deferred_year_study_plan.brief", "Set aside a quiet session at your desk at home or in the care home's study room to balance Class XI lessons, rest and steady preparation for next year's examination. Deferring gives you a full year on your own terms.");
    if (Key == "phase2.a8.p4_deferred_community_volunteer.title") return LOCTEXT("phase2.a8.p4_deferred_community_volunteer.title", "Enquire about Nabapara community clinic volunteering");
    if (Key == "phase2.a8.p4_deferred_community_volunteer.brief") return LOCTEXT("phase2.a8.p4_deferred_community_volunteer.brief", "Ask at the Nabapara Vidyalaya notice desk about helping Dr. Ananya Chatterjee's Community Clinic with afternoon patient-card sorting or the neighbourhood reading table. Hours are agreed around classes and household routines.");
    if (Key == "phase2.a8.p4_declined_vocational_craft.title") return LOCTEXT("phase2.a8.p4_declined_vocational_craft.title", "Talk trade apprenticeships at the repair workshop");
    if (Key == "phase2.a8.p4_declined_vocational_craft.brief") return LOCTEXT("phase2.a8.p4_declined_vocational_craft.brief", "Meet Debashis Pal at the Nabapara Lane repair workshop after school to ask about electrical repair apprenticeships, polytechnic diplomas and neighbourhood shop work. Declining the tower leaves every ordinary craft and trade open.");
    if (Key == "phase2.a8.p4_declined_stream_civic_path.title") return LOCTEXT("phase2.a8.p4_declined_stream_civic_path.title", "Plan a board-exam and civic-work route");
    if (Key == "phase2.a8.p4_declined_stream_civic_path.brief") return LOCTEXT("phase2.a8.p4_declined_stream_civic_path.brief", "Meet Amrita Basu at Nabapara Vidyalaya to map Class XII board subjects, direct degree admission at Bhagirathi State University, or local civic work in Nabapara. A full adult life needs neither a tower rank nor supernatural practice.");
    if (Key == "phase2.a8.p407u.mentor.session") return LOCTEXT("phase2.a8.p407u.mentor.session", "Amrita Basu sets your result beside the school timetable. You discuss managing pressure and where your retained evidence can be useful. Ordinary school, a later attempt, and leaving the tower behind remain open. The session ends with one manageable next step for the coming week.");
    return LOCTEXT("Missing", "This next step has no available description.");
}

#undef LOCTEXT_NAMESPACE

