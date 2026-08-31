// Fill out your copyright notice in the Description page of Project Settings.

#include "RSPrimaryLayout.h"

#include "Components/PanelWidget.h"

UUserWidget* URSPrimaryLayout::CreateWidgetInLayer(ERSWidgetLayer Layer, TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (!Widget)
	{
		return nullptr;
	}

	if (!AddWidgetToLayer(Layer, Widget))
	{
		return nullptr;
	}

	return Widget;
}

bool URSPrimaryLayout::AddWidgetToLayer(ERSWidgetLayer Layer, UWidget* Widget)
{
	if (!Widget || Widget->GetParent())
	{
		return false;
	}

	UPanelWidget* LayerPanel = GetLayer(Layer);
	if (!LayerPanel)
	{
		return false;
	}

	return LayerPanel->AddChild(Widget) != nullptr;
}

bool URSPrimaryLayout::RemoveWidgetFromLayer(UWidget* Widget)
{
	if (!Widget)
	{
		return false;
	}

	UPanelWidget* ParentLayer = Cast<UPanelWidget>(Widget->GetParent());
	if (ParentLayer != GameplayLayer && ParentLayer != MenuLayer && ParentLayer != ModalLayer && ParentLayer != NotificationLayer)
	{
		return false;
	}

	return ParentLayer->RemoveChild(Widget);
}

UPanelWidget* URSPrimaryLayout::GetLayer(ERSWidgetLayer Layer) const
{
	switch (Layer)
	{
	case ERSWidgetLayer::Gameplay:
		return GameplayLayer;
	case ERSWidgetLayer::Menu:
		return MenuLayer;
	case ERSWidgetLayer::Modal:
		return ModalLayer;
	case ERSWidgetLayer::Notification:
		return NotificationLayer;
	default:
		return nullptr;
	}
}
