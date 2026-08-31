// Fill out your copyright notice in the Description page of Project Settings.

#include "RSViewModelWidget.h"

#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"

bool URSViewModelWidget::SetViewModel(UMVVMViewModelBase* InViewModel)
{
	if (!InViewModel)
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return false;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	ViewModelInterface.SetObject(InViewModel);
	ViewModelInterface.SetInterface(Cast<INotifyFieldValueChanged>(InViewModel));

	if (!ViewModelInterface)
	{
		return false;
	}

	return View->SetViewModelByClass(ViewModelInterface);
}
