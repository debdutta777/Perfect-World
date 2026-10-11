#pragma once

#include "Awakening.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Shared by AwakeningSignals.cpp (the readings, the apportioning and the causes) and
// AwakeningResolve.cpp (the distribution, the capability and the ability). Not public.
namespace PerfectWorld::AwakeningDetail
{
/** A weight per signal, in AwakeningSignal order. */
using SignalWeights = std::array<std::int32_t, AwakeningSignalCount>;

/** The thirteen readings and where each came from. */
struct SignalSet
{
    std::array<std::int32_t, AwakeningSignalCount> Reading{};
    /** The strongest input behind each signal, empty when the signal is zero. */
    std::array<std::string, AwakeningSignalCount> Source;
    /** Conduct signals only: the test and choice that added most. */
    std::array<std::string, AwakeningSignalCount> TestId;
    std::array<std::string, AwakeningSignalCount> ChoiceId;
};

/** Reduces the inputs to readings. Latent signals are the mean of their inputs; conduct signals are the summary's. */
SignalSet ComputeSignals(const AwakeningInput& input);

/**
 * Splits `total` across the weights in proportion (largest remainder), exactly. Remainder
 * ties are broken by a keyed draw from the seed under the text `what` and the index, never
 * by position. The weights must not all be zero (a zero sum gives an even split).
 */
std::vector<std::int32_t> Apportion(const std::vector<std::int64_t>& weights, std::int64_t total, std::uint64_t seed,
                                    std::string_view what);

/** The strongest up to AwakeningCauseCount signals by weight times reading, those above zero, ties in signal order. */
std::vector<AwakeningCause> TopCauses(const SignalSet& signals, const SignalWeights& weights);

/** A keyed draw naming a tie: a pure function of the seed and the text. */
std::uint64_t TieDraw(std::uint64_t seed, std::string_view text);
} // namespace PerfectWorld::AwakeningDetail
