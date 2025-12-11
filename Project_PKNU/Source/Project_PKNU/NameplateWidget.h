
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NameplateWidget.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class PROJECT_PKNU_API UNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetName(const FString& Name);

	UPROPERTY(meta = (BindWidget))
	UTextBlock* NameText;
	
};
