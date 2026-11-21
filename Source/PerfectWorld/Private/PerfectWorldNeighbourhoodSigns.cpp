#include "PerfectWorldNeighbourhoodFacade.h"
#include "PerfectWorldNeighbourhoodText.h"

namespace PWNs
{
namespace
{
// Fictional para businesses: Bengali family names and trades in Latin letters, as most Kolkata signboards are written.
const char* const NsShopNames[] = {
    "BASAK STORES",        "NEW STAR TAILORS",     "MAA TARA HARDWARE",   "ROY BROTHERS",         "SAHA CLINIC",
    "DEY ELECTRICALS",     "ANNAPURNA HOTEL",      "LAXMI JEWELLERS",     "GHOSH BOOK STALL",     "PAL SANITARY",
    "MUKHERJEE & SONS",    "PRIYA BOUTIQUE",       "SAMIR MOBILES",       "SEN & CO",             "KALI CYCLE STORE",
    "BHOLA PRINTING PRESS", "SONAR BANGLA GIFTS",  "NABAPARA SALON",      "CHOWDHURY FANCY",      "MAA MANASA STORES",
    "DAS GROCERY",         "JOY MAA FASHION",      "BIJOY SPORTS",        "ARATI TEXTILES",       "MONDAL PHOTO STUDIO",
    "PRANTIK LAUNDRY",     "NEW BHARAT RADIO",     "KARMAKAR WELDING",    "SHYAMA FOOTWEAR",      "RUPALI OPTICS",
};
constexpr int32 NsShopNameCount = static_cast<int32>(UE_ARRAY_COUNT(NsShopNames));

const char* const NsAds[] = {
    "SURYA CEMENT",   "NABANNA PAINTS", "GANGA FANS",   "PRAGATI BANK", "JOY HIND TEA",   "SAGAR OIL",
    "BANGA RICE",     "SHAKTI BATTERY", "MITALI SWEETS", "ASHA TUITION", "DIPALI SAREES", "UDAY CABLES",
};
constexpr int32 NsAdCount = static_cast<int32>(UE_ARRAY_COUNT(NsAds));

int32 NsLen(const char* Text)
{
    int32 N = 0;
    while (Text[N] != '\0')
    {
        ++N;
    }
    return N;
}
} // namespace

const char* NsShopName(uint32 Seed, int32 MaxChars)
{
    for (int32 I = 0; I < NsShopNameCount; ++I)
    {
        const char* Name = NsShopNames[(Seed + static_cast<uint32>(I)) % static_cast<uint32>(NsShopNameCount)];
        if (NsLen(Name) <= MaxChars)
        {
            return Name;
        }
    }
    return "SEN & CO";
}

const char* NsAdName(uint32 Seed)
{
    return NsAds[Seed % static_cast<uint32>(NsAdCount)];
}

int32 NsSignboard(FNsSink& K, const FLaneFrame& F, double U0, double U1, double W0, double W1, ELaneColour Board, ELaneColour Ink,
                  const char* Text)
{
    if (U1 - U0 < 40.0 || W1 - W0 < 20.0)
    {
        return 0;
    }
    // The board stands 6 cm proud of the face on a thin trim frame; lettering stands another 2 cm proud.
    K.Add(F, ELaneColour::TrimDark, U0 - 3.0, U1 + 3.0, -4.0, -1.0, W0 - 3.0, W1 + 3.0);
    K.Add(F, Board, U0, U1, -6.0, -4.0, W0, W1);
    constexpr double Margin = 6.0;
    for (double Px = 6.0; Px >= 2.0; Px -= 1.0)
    {
        const double Width = NsTextWidth(Text, Px);
        const double Height = 7.0 * Px;
        if (Width <= (U1 - U0) - 2.0 * Margin && Height <= (W1 - W0) - 2.0 * Margin)
        {
            const double Left = U0 + 0.5 * ((U1 - U0) - Width);
            const double Top = W0 + 0.5 * ((W1 - W0) + Height);
            return NsDrawText(K, F, Ink, Left, Top, Px, -8.0, -6.0, Text);
        }
    }
    return 0;
}
} // namespace PWNs
