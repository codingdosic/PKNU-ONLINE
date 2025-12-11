// Copyright Epic Games, Inc. All Rights Reserved.

#include "PknuGameMode.h"
#include "PknuCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "LoginWidget.h"
#include "Blueprint/UserWidget.h"

APknuGameMode::APknuGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void APknuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (LoginWidgetClass)
	{
		LoginWidgetInstance = CreateWidget<ULoginWidget>(GetWorld(), LoginWidgetClass);
		if (LoginWidgetInstance)
		{
			LoginWidgetInstance->AddToViewport(100); // ZOrder를 100으로 설정하여 다른 UI 위에 표시

			APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
			if (PlayerController)
			{
				PlayerController->bShowMouseCursor = true;
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(LoginWidgetInstance->TakeWidget());
				PlayerController->SetInputMode(InputMode);
			}
		}
	}
}
