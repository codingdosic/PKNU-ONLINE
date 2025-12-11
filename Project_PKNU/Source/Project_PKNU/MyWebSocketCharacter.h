// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PknuCharacter.h"
#include "Blueprint/UserWidget.h"
#include "MyRemoteCharacter.h"
#include "Components/EditableTextBox.h"
#include "WebSocketManager.h"
#include "MyWebSocketCharacter.generated.h"

class UWidgetComponent;
class UInputAction;

UCLASS()
class PROJECT_PKNU_API AMyWebSocketCharacter : public APknuCharacter
{
	GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UWidgetComponent* NameplateComponent;

    void SetName(const FString& Name);

	// 생성자
	AMyWebSocketCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleChatAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* WebSocketCharacterMappingContext;

	// 기존 부모 함수들 오버라이드
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


public:	
	// 기존 부모 함수들 오버라이드
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ChatWidget의 클래스 참조용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UChatWidget> ChatWidgetClass;

	// 생성된 인스턴스 저장용
	UPROPERTY()
	class UChatWidget* ChatWidgetInstance;

	UFUNCTION()
	void SendChatMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	FString GetMySocketId() const;


private:
    void ToggleChatInput();

    UPROPERTY()
    UWebSocketManager* WebSocketManager; // 소유하지 않고, 참조만 저장
};
