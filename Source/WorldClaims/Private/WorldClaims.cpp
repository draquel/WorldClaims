// Copyright Daniel Raquel. All Rights Reserved.

#include "WorldClaims.h"

#define LOCTEXT_NAMESPACE "FWorldClaimsModule"

DEFINE_LOG_CATEGORY(LogWorldClaims);

void FWorldClaimsModule::StartupModule()
{
	UE_LOG(LogWorldClaims, Log, TEXT("WorldClaims module started"));
}

void FWorldClaimsModule::ShutdownModule()
{
	UE_LOG(LogWorldClaims, Log, TEXT("WorldClaims module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWorldClaimsModule, WorldClaims)
