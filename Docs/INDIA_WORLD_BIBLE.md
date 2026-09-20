# India World Bible — Kolkata and Nabapara

**Document status:** authored setting plan for the initial district and its future expansion. It is a reference for writing, environment art, audio, systems, and cultural review. A technical scaffold may contain stable location or faction IDs; that does not mean the locations, schedules, dialogue, or regional authenticity work are implemented.

## Setting boundary

The game starts in **Kolkata, West Bengal**, in the fictional **Nabapara** neighborhood. Real city names anchor recognition; streets, school, households, clinic, businesses and Gate sites are authored fiction. Nabapara is a composite neighborhood, not an asserted municipal ward or a map of a real locality. Its school is **Nabapara Vidyalaya**. All starting locations remain in West Bengal; later chapters expand across India.

The setting draws on Kolkata para life: shared courtyards, older shuttered houses alongside flats, book and stationery shops, tea stalls, neighborhood clubs, school routines, buses, riverside work and monsoon drainage. Bengali is central, alongside multilingual residents. Do not merge hills, industrial districts and delta environments into one supposedly typical Kolkata street. Real geography is a reference; gameplay routes are openly fictionalized.

Kolkata retains its identity without forcing a one-to-one city replica. No real school, medical practice or person is assigned fictional scandal or Gate casualties. Agencies such as the National Gate Authority are invented. Real educational, transport and civic facts require current research when used in production; no fictional schedule, fee or Gate law is a claim about real West Bengal.

## Historical premise

Gates first appeared eighteen years before the opening. The earliest years brought panic, inconsistent jurisdiction, and dangerous civilian improvisation. By the present, routine low-rank Gates are handled like a hybrid of hazardous-material incident, disaster response, scientific site, and licensed contract work. Alerts are ordinary; casualties are not.

The central government established the National Gate Authority (NGA) through fictional emergency legislation. State and city offices coordinate cordons, evacuation, health response, material handling, and school closures. Private guilds can supply licensed specialists, but police powers and public safety remain governmental. Artifact ownership is regulated. Black markets exist because enforcement, poverty, and demand create pressure—not because every public institution is casually corrupt.

The player was born after the first Gates. Adults remember the disruption; classmates grew up with drills, alert symbols, school lessons, and influencer footage. This generational difference shapes dialogue.

## Kolkata

Kolkata is the real city anchor. Only Nabapara's playable street graph is built initially; the wider city is represented by references, news and later travel destinations. Future authored zones can explore book-market life, riverside industry, older residential districts and research offices. These are expansion themes, not a claim that every real landmark is adjacent to the school.

### Climate calendar

| Period | Conditions | City behavior | Gameplay use |
|---|---|---|---|
| March–May | hot humid days, storms, water pressure concerns | early errands, shaded stalls, power strain | energy and route planning; Agni rumors rise |
| June–September | monsoon bursts, humid intervals, runoff | delays, umbrellas, leaks, flooded underpasses | alternate paths, Jala conditions, school closures |
| October–November | retreating rain, clearer evenings | festivals and repairs | public events and crowd changes |
| December–February | dry, mild mornings and cool nights | outdoor study, sport, weddings | long outdoor schedules |

The slice begins in late June after the monsoon has started. Rain is episodic, not a permanent gray filter. Wet concrete, drain overflow, tree cover, laundry decisions, delayed buses, and footwear reactions carry the weather into daily life.

### Language

The authored neighborhood uses Bengali prominently, with English in selected school and institutional contexts. Hindi, Urdu and other languages appear through specific residents and situations. Household language is freely chosen and does not set intelligence or power. Code-switching serves relationship and context; it is not a punch line or an automatic marker of education.

The base script is professionally written in English with a localization glossary and room for natural regional adaptation. Signs have authored Bengali-script/Latin layouts rather than machine-translated texture swaps. Subtitles translate meaning without italicizing ordinary Indian words as exotic. Honorifics and kinship terms are retained only where context makes them clear.

### Money and class

The game shows economic difference through space, time, risk, privacy, commute, and available buffers. It avoids equating wealth with intelligence, virtue, cleanliness, English fluency, or family affection. Prices use ₹ and are internally consistent, then updated as a tuning table rather than claimed as a live cost-of-living index.

The player has personal pocket money and earnings, while the household has its own budget. A higher household reserve can absorb a repair or school expense; it is not lootable cash. Low resources create tradeoffs, and above Household Resources Rank 0 they never make a background a misery-for-points option. As of a 2026-09-20 design decision, Household Resources Rank 0 itself is the one deliberate exception: it means institutional care (a children's home) rather than an ordinary low-resource family, a coupling the design contract otherwise avoids on purpose. See CHARACTER_CREATION.md ("Household selection") for the full rule and its content-handling note.

## Nabapara

Nabapara is an invented neighborhood cluster containing homes, a school, a market, small workshops, and a canal-side municipal works site. It is not presented as an official ward. The playable area is compact enough to cross on foot in several minutes but divided by traffic, drainage, walls, railings, and opening hours. A large banyan tree at the maidan is a social landmark, not a mystical object by default.

### Location catalogue

Stable IDs are included for content and save references.

| ID | Location | Everyday function | Supernatural/system function | Required details |
|---|---|---|---|---|
| `loc_nhss` | Nabapara Vidyalaya | Class XI–XII classrooms, lab, office, court | drill procedure, incident closure, research club | repaired plaster, shaded corridors, notice boards, shoe/wet-floor behavior |
| `loc_shanti_chai` | Shanti Chai Corner | tea, breakfast, gossip, student stop | rumor exchange and alert reaction | steel vessels, token/payment flow, limited seating, no theme-park clutter |
| `loc_banyan_maidan` | Banyan Maidan | sport, walking, assembly, vendors | evacuation point and training boundary | worn desire paths, drainage edge, cricket marks, seasonal ground change |
| `loc_bus_hub` | Nabapara Bus Stand | local buses and shared-auto connection | rerouting during cordons | route boards, queues, shelter, accessibility gaps represented without ridicule |
| `loc_old_market` | Nabapara Old Market | food, stationery, tailoring, repair | shortages, witnesses, artifact rumor | mixed shop widths, shutters, delivery rhythm, waste and cleaning cycles |
| `loc_clinic` | Dr. Chatterjee Community Clinic | primary care and referrals | post-Gate observation | waiting system, privacy curtain, credible basic equipment |
| `loc_authority_annex` | NGA Ward Annex | permits, public information, incident coordination | interview and supervised training | security layers, public counter, operations room kept inaccessible |
| `loc_canal_works` | Canal Pump and Works | flood control and maintenance | first spatial anomaly source | brick and plaster retaining wall, pumps, restricted ledges, monsoon flow |
| `loc_industrial_yard` | Sen Riverside Works | partly closed light industry | controlled Gate mission kit | cranes, scrap categories, worker history, safety signage |
| `loc_gym` | Udaan Fitness Hall | community training and classes | physical progression and response volunteers | practical equipment, fans, schedule board, mixed-age membership |
| `loc_family_flats` | Nabapara Cooperative Housing | parent/grandparent homes | household simulation | shared landing, water schedule, scooters, society notices |
| `loc_row_houses` | Maitri Lane | extended-family homes and workshops | alternate home kit | thresholds, roof use, workshop noise, privacy gradients |
| `loc_staff_quarters` | School Staff Quarters | appointed-guardian home | alternate home kit | compact rooms, common courtyard, school boundary relationship |

Each public location needs opening hours, staff ownership, delivery/service access, waste and maintenance logic, power/rain response, and at least one reason to visit when no quest marker is present.

## Opening household life

The protagonist's guardian is built from a template plus authored scenes — or, at Household Resources Rank 0, the protagonist grew up in institutional care (a children's home) with institutional staff in place of a guardian; see CHARACTER_CREATION.md. Regardless of template or the institutional-care outcome, the household (or children's home) establishes:

- where shoes, school bag, charger, medicines, and keys belong;
- who wakes first and who prepares or buys breakfast;
- how water, laundry, cooking, cleaning, and errands are shared;
- what the guardian expects about attendance, phone contact, curfew, and Gate alerts;
- one recurring pressure and one reliable form of care;
- an emergency contact and a believable reaction to unexplained injury.

The opening morning has one choice that costs time: eat, help with a task, or leave quickly. None is presented as the correct cultural performance. Later dialogue recalls the concrete act.

## School bible

Nabapara Vidyalaya is a fictional coeducational higher-secondary school serving the opening story's Class XI and XII students. The current design uses Bengali as the principal classroom language, English as a subject with selected bilingual materials, and Science, Commerce, and Humanities streams. These are production assumptions, not claims about a real school or current West Bengal education rules. Board affiliation, subject combinations, term calendar, fees, attendance practice, examination details, and staffing must be checked against current authoritative sources and a West Bengal education consultant before scripts or UI present them as facts. The school is neither an elite academy nor a caricature of neglect.

The slice opens at the start of term. Students are learning new schedules and social groups. The player attends assembly, meets a class teacher, joins a stream-specific period, and faces an after-school choice. Teachers differ in style and workload; no single harsh-teacher trope represents the institution.

Gate safety is an explicitly fictional school procedure created for this setting: alerts have recognizable tones, staff check NGA notices, students follow the school's marked shelter or assembly instructions, guardians receive messages, and departures can be held. This procedure must not be presented as real West Bengal emergency policy. Students still joke, speculate, film, disobey, help, or panic according to character.

Future education stages—Class XII boards, competitive entrance routes, vocational training, open learning, college, apprenticeships, and professional qualifications—must show multiple viable paths. No exam score is treated as a measure of human worth, though characters may believe it is.

## Named cast for the slice

Names and details remain subject to sensitivity and voice casting review.

| ID | Name | Identity and role | Regular schedule | Personal tension | Relationship use |
|---|---|---|---|---|---|
| `npc_guardian_primary` | Player-defined | adult guardian from the selected household template, or (Household Resources Rank 0) children's-home staff | home plus a template-specific work shift | protection conflicts with the player's growing secrecy | trust, obligation, recovery |
| `npc_mira_sen` | Mira Sen | Class XI Commerce student; helps at her family's stationery counter | school weekdays; counter after class twice a week | wants independence without destabilizing the shop | planner, potential confidante or rival |
| `npc_ishan_sheikh` | Ishan Sheikh | Class XI Science student; records neighborhood stories | school weekdays; bus commute; maidan some evenings | curiosity crosses safety boundaries | friend, witness, media pressure |
| `npc_tara_das` | Tara Das | Class XI Humanities student and runner | school; maidan practice; younger-sibling pickup | disciplined scheduling conceals strain at home | training partner and honest critic |
| `npc_ms_basu` | Amrita Basu | Class XI teacher and player-class adviser | school mornings; preparation and marking afternoons | responsible for students under an untested fictional Gate policy | academic trust and accountability |
| `npc_dr_chatterjee` | Dr. Ananya Chatterjee | neighborhood clinic physician | alternating day and evening clinic sessions | receives incomplete information from Gate authorities | care, confidentiality, evidence |
| `npc_amit_roy` | Amit Roy | junior field officer at the NGA Annex | Annex shifts and incident callouts | believes procedure saves lives while seeing its gaps | authority reputation and supervision |
| `npc_farida_qadri` | Farida Qadri | proprietor of Shanti Chai Corner | opens early; closes after the evening commute | school and bus disruption threaten a narrow-margin business | neighborhood memory and rumors |

No NPC exists solely to explain lore. Each has ordinary work, a route, a private concern, a relationship to the neighborhood, and a response to Gate disruption. Muslim, Dalit, linguistic-minority, and other identities are not shorthand for tragedy, criminality, or mysticism. Caste may shape the world, but any explicit story requires dedicated authorship and review rather than incidental flavor text.

## Institutions and factions

### National Gate Authority

The NGA registers incidents, sets response zones, licenses specialists, secures artifacts, and coordinates research. Its local staff range from competent to overextended. Conflict comes from secrecy, unequal resources, jurisdiction, and risk thresholds, not a blanket “evil government” premise.

### Astra Research Institute

A public-private research network studies materials and Tattva phenomena. Its Kolkata unit supplies instruments and researchers to the Annex. It values evidence but has incentives around patents, prestige, and access.

### Vanguard Guild

A national private guild with a small city office. It can contract licensed adult Hunters and equipment, but has no legal authority over civilians. Celebrity branding and worker safety can conflict.

### Ananta Industries

A diversified company developing sensors, containment materials, and consumer products. Its influence grows in later chapters. The slice shows branded infrastructure and procurement, not a monologue from a corporate villain.

### The Bazaar

A decentralized network of repairers, traders, and smugglers rather than an ethnicized secret market. Some activity is legal reuse, some unlicensed, and some dangerous. It is future content, foreshadowed by one disputed component.

### The Silent Order

A later mystery faction that interprets Gates through old and new philosophical frameworks. It must never claim one living faith secretly “caused” the phenomenon. Tattva terminology is an in-world scientific-cultural naming convention, not a definitive statement about Hindu, Buddhist, or Jain belief.

## Tattvas and cultural handling

Researchers adopted Prithvi, Jala, Agni, Vayu, and Akasha as descriptive categories because the vocabulary was locally legible and the five patterns appeared worldwide. The correspondences are loose. Scholars and religious figures disagree about the terminology. The game distinguishes fictional mechanics from sacred claims.

Visuals avoid putting deities, scripture, sacred symbols, cremation imagery, or ritual objects into random enemy/loot pools. Temples, mosques, churches, gurdwaras, Buddhist and Jain sites, and neighborhood shrines may appear as lived places only with contextual purpose, accurate art, and review. They are not generic dungeon entrances.

## Food, clothing, and streets

The first district's everyday food set is deliberately small and location-specific: cha, muri, singara, dal-rice, luchi with an occasional breakfast plate, seasonal fruit, eggs, and a rotating canteen lunch; fish and vegetarian dishes follow household preferences. Other foods appear when a character or vendor makes the regional reason clear. “Curry” is not used as a universal item category.

School uniforms are institution-specific and practical for heat and rain. Outside school, wardrobes consider age, household, activity, climate, reuse, and personal taste: T-shirts, shirts, kurtas, jeans, salwar-kameez variants, track wear, dresses, sandals, shoes, rain layers, scarves, and head coverings. Clothing is not used to label a character as modern versus backward.

Roads have mixed users and uneven pedestrian provision. Gameplay simplifies collision and traffic density, but crossings, bus boarding, parking, deliveries, dogs, rainwater, vendors, and building thresholds establish real spatial behavior. The player cannot steal any parked vehicle or treat crowds as disposable combat scenery.

## Art briefs

### Environment key art: “First Monsoon Morning”

View from the player-home lane toward the school at 07:20. Warm light breaks after rain; darkened brick walls, moss at plaster edges, wet patched asphalt, school uniforms under mixed umbrellas, bus spray in the middle distance, Farida opening the chai stall, maintained but visually dense utility lines, and the banyan canopy beyond. The supernatural is limited to a small official Gate-status icon on a bus-stop display. Deliver one 16:9 keyframe, one overpainted blockout, and a callout sheet for materials and sign hierarchy.

### Environment key art: “Cordoned Canal Works”

The familiar canal route at blue hour after the anomaly. NGA barriers redirect a bus; pump-house work lights reflect on runoff; geometry beyond the cordon repeats at an impossible angle. Civilians react at a safe distance rather than gathering inside danger. Deliver normal and anomaly states using the same camera so lighting/VFX cost is measurable.

### Interior kit: school

Create a modular 4 m structural bay, corridor railing, classroom wall/door, science lab, office, stair, toilet exterior, notice boards, fans, tube lights, desks, grills, rain mats, and three maintenance states. Texture signage separately for localization. Target 20 modular pieces, 15 props, and 2 trim sheets for the first slice; expand only after profiling; hero classrooms must assemble from the kit.

### Character lineup

Produce the player at age sixteen plus the eight named cast members in neutral front/side/back, five expression busts, school/work outfit, rain variant, footwear, bag/held items, and skin/hair material references. Show variation in height, frame, complexion, facial structure, hair texture, age, and economic context without exaggeration. Final anatomy must work at the isometric camera and portrait crop.

### Gate kit: fractured infrastructure

Build recognizable concrete, tile, brick and plaster, steel, pipe, and signage pieces that can rotate, repeat, float, and shear under Akasha distortion. The kit requires gameplay-readable walkable edges and collision variants. Provide neutral, Prithvi, Jala, and Akasha material states; Agni and Vayu are VFX overlays in the slice.

### UI and iconography

The phone UI should feel like a durable midrange Android-era device without copying a brand. Use high-contrast cards, large type options, Bengali-script-tested fonts, controller focus, and icon-plus-text alerts. Gate rank color is redundant with letter, shape, and sound. Avoid saffron/green/white decoration as a default national shorthand.

## Audio briefs

Record or license region-appropriate ambiences with metadata by time, weather, and location. Dialogue casting should prioritize natural contemporary speech over accent performance. Crowd walla needs distinct school, market, bus, clinic, and home groups. Vendors should not loop a single shouted line every few seconds.

The Gate alert has three layers: public-device tone, spoken instruction, and local physical response. It must be recognizable without resembling real emergency tones closely enough to confuse players. The first anomaly gradually removes traffic low-end, desynchronizes rain impacts, and shifts the school bell interval before any musical sting.

## Representation review checklist

Before content lock, reviewers from the relevant region inspect language, names, school routine, uniforms, food, prices, transit, household layout, religious/caste implications, government behavior, and soundscape. Findings are tracked as blocking, high, medium, or polish. Blocking and high issues must close before release candidate.

Writers must ask of every scene: whose point of view frames it, whether an identity is carrying the entire burden of a problem, whether the joke works without mocking speech or poverty, whether an institution acts through a believable person and process, and whether the same story could happen during an ordinary week as well as a Gate crisis.

## Expansion map

Content expands outward from Nabapara rather than unlocking a country menu:

1. **Kolkata city:** additional fictionalized neighborhoods and research or riverside chapters after the first district is proven.
2. **West Bengal region:** evaluate separate Howrah, Durgapur and Siliguri chapters, each with its own geography, social setting and production budget. Rural Bengal and the Sundarbans are distinct later research tracks, not copied Kolkata assets.
3. **Other Indian regions:** each built as a chapter with climate, languages, architecture, institutions, Gate ecology, and local creative review.
4. **Lifecycle city:** the same Kolkata changes as the player ages—construction finishes, shops change hands, classmates leave or return, households move, and institutions remember major incidents.

Cross-region travel is planned only after the first city supports school-to-adult transitions. International locations are outside the current product promise.
