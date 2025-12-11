// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PknuGameMode.generated.h"

class ULoginWidget;

UCLASS(minimalapi)
class APknuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APknuGameMode();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Widget")
	TSubclassOf<ULoginWidget> LoginWidgetClass;

	UPROPERTY()
	ULoginWidget* LoginWidgetInstance;
};



