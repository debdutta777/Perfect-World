#include "PerfectWorldContentSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogFaContent, Log, All);

void UPWContentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // DefaultGame.ini stages Content/../Data/Content as NonUFS at Project/Data/Content.
    // Use Unreal IO in both PIE and packaged games; the core receives UTF-8, never OS file handles.
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Content")));
    TArray<FString> Paths;
    IFileManager::Get().FindFilesRecursive(Paths, *Root, TEXT("*.json"), true, false);
    Paths.Sort();
    std::vector<PerfectWorld::ContentFile> Files;
    int32 ReadIssues = 0;
    for (FString Path : Paths)
    {
        FPaths::NormalizeFilename(Path);
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path))
        {
            ++ReadIssues;
            UE_LOG(LogFaContent, Error, TEXT("Cannot read content file %s."), *Path);
            continue;
        }
        // Preserve .../<type>/<id>.json: LoadContent validates the enclosing type directory.
        Files.push_back({std::string(TCHAR_TO_UTF8(*Path)), std::string(TCHAR_TO_UTF8(*Text))});
    }
    PerfectWorld::ContentLoadResult Result = PerfectWorld::LoadContent(Files);
    UE_LOG(LogFaContent, Display, TEXT("Content %s: %d JSON files, %llu records, %llu validation issues, %d read issues."),
           *Root, Paths.Num(), static_cast<uint64>(Result.Db.Count()), static_cast<uint64>(Result.Issues.size()), ReadIssues);
    for (const PerfectWorld::ContentIssue& Issue : Result.Issues)
    {
        UE_LOG(LogFaContent, Error, TEXT("%s [%s]: %s"), UTF8_TO_TCHAR(Issue.File.c_str()),
               UTF8_TO_TCHAR(Issue.Id.c_str()), UTF8_TO_TCHAR(Issue.Message.c_str()));
    }
    bLoaded = !Paths.IsEmpty() && ReadIssues == 0 && Result.Ok();
    if (bLoaded)
    {
        Db = MoveTemp(Result.Db);
    }
    else
    {
        UE_LOG(LogFaContent, Error, TEXT("Content database unavailable; dependent NPCs will not spawn."));
    }
}

void UPWContentSubsystem::Deinitialize()
{
    bLoaded = false;
    Db = PerfectWorld::ContentDb();
    Super::Deinitialize();
}
