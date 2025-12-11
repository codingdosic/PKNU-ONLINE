
// Fill out your copyright notice in the Description page of Project Settings.


#include "LoginWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "PknuGameInstance.h"
#include "WebSocketManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void ULoginWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UPknuGameInstance* GameInstance = Cast<UPknuGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (GameInstance)
	{
		WebSocketManager = GameInstance->GetWebSocketManager();
		if (WebSocketManager)
		{
			WebSocketManager->OnInitialStateSynced.AddDynamic(this, &ULoginWidget::OnInitialStateSynced);
		}
	}

	if (loginButton)
	{
		loginButton->OnClicked.AddDynamic(this, &ULoginWidget::OnLoginButtonClicked);
	}
}

void ULoginWidget::OnLoginButtonClicked()
{
	if (loginTextBox && WebSocketManager)
	{
		FString Username = loginTextBox->GetText().ToString();
		if (!Username.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Login button clicked! Attempting to connect with username: %s"), *Username);
			PendingUsername = Username; // Store username temporarily
			WebSocketManager->Connect(PendingUsername); // Connect with username
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Username cannot be empty!"));
		}
	}
}

void ULoginWidget::OnInitialStateSynced()
{
	UE_LOG(LogTemp, Warning, TEXT("WebSocket Connected! Removing login widget."));
	// 로그인 메시지 전송 로직 제거

	RemoveFromParent();

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController)
	{
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = false;
	}
}
