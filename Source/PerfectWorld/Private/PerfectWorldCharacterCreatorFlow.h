#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

/**
 * Small text-conversion helpers shared by the front-end half of the character
 * creator subsystem (PerfectWorldCharacterCreatorFlow.cpp). The core speaks
 * UTF-8 std::string; Unreal speaks FString.
 */
namespace PWCreatorFlow
{
inline std::string ToUtf8(const FString& Value)
{
    return std::string(TCHAR_TO_UTF8(*Value));
}

inline FString FromUtf8(std::string_view Value)
{
    return FString(UTF8_TO_TCHAR(std::string(Value).c_str()));
}
} // namespace PWCreatorFlow
