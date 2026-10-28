// gon.blockout.* console commands (gon.markers.debug is the cvar in GoNMarkers.cpp).
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GoNBlockoutLog.h"
#include "GoNBlockoutPiece.h"
#include "GoNMarkers.h"
#include "HAL/IConsoleManager.h"

namespace GoNBlockoutKitCommandsDetail
{
	FAutoConsoleCommandWithWorldAndArgs ListCommand(
		TEXT("gon.blockout.list"),
		TEXT("GoN Blockout: list blockout pieces (with cover point counts) and gameplay markers in the current world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: no world"));
				return;
			}
			int32 PieceCount = 0;
			for (TActorIterator<AGoNBlockoutPiece> It(World); It; ++It)
			{
				UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout:   %s %s: %s | cover %d low / %d high"), *It->GetClass()->GetName(), *It->GetActorNameOrLabel(),
					*It->GetPieceLabel(), It->CountCoverPoints(false), It->CountCoverPoints(true));
				++PieceCount;
			}
			int32 MarkerCount = 0;
			for (TActorIterator<AGoNMarker> It(World); It; ++It)
			{
				UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout:   %s %s: %s"), *It->GetClass()->GetName(), *It->GetActorNameOrLabel(), *It->GetMarkerLabel());
				++MarkerCount;
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: %d piece(s), %d marker(s)"), PieceCount, MarkerCount);
		}));

	FAutoConsoleCommandWithWorldAndArgs CoversCommand(
		TEXT("gon.blockout.covers"),
		TEXT("GoN Blockout: gon.blockout.covers <0|1> - show cover point cones and piece labels during play (no argument = toggle)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			static bool bShown = false;
			if (World == nullptr)
			{
				UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: no world"));
				return;
			}
			bShown = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !bShown;
			int32 PieceCount = 0;
			for (TActorIterator<AGoNBlockoutPiece> It(World); It; ++It)
			{
				It->SetDebugVisibleInGame(bShown);
				++PieceCount;
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: cover points %s on %d piece(s)"), bShown ? TEXT("shown") : TEXT("hidden"), PieceCount);
		}));

	FAutoConsoleCommandWithWorldAndArgs RebuildCommand(
		TEXT("gon.blockout.rebuild"),
		TEXT("GoN Blockout: rebuild every blockout piece in the current world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				UE_LOG(LogGoNBlockout, Warning, TEXT("GoN Blockout: no world"));
				return;
			}
			int32 PieceCount = 0;
			for (TActorIterator<AGoNBlockoutPiece> It(World); It; ++It)
			{
				It->Rebuild();
				++PieceCount;
			}
			UE_LOG(LogGoNBlockout, Display, TEXT("GoN Blockout: rebuilt %d piece(s)"), PieceCount);
		}));
}
