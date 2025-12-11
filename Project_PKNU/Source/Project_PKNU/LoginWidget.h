
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginWidget.generated.h"

class UEditableTextBox;
class UButton;
class UWebSocketManager;

/**
 * 
 */
UCLASS()
class PROJECT_PKNU_API ULoginWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void OnLoginButtonClicked();

	UFUNCTION()
	void OnInitialStateSynced();

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* loginTextBox;

	UPROPERTY(meta = (BindWidget))
	UButton* loginButton;

	UPROPERTY()
	UWebSocketManager* WebSocketManager;

	FString PendingUsername;
};
