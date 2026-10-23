#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "Toolkits/AssetEditorToolkit.h"

class IDetailsView;
class SInv_InventoryGridPreview;
class UInv_InventoryGridAsset;

class FInv_InventoryGridAssetEditor : public FAssetEditorToolkit, public FEditorUndoClient, public FNotifyHook
{
public:
	virtual ~FInv_InventoryGridAssetEditor() override;
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InToolkitHost,
		UInv_InventoryGridAsset* Asset);
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override { return TEXT("InvInventoryGridEditor"); }
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Inventory Grid"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.2f, 0.6f, 0.8f); }
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

protected:
	virtual void SaveAsset_Execute() override;

private:
	void RefreshPreview();
	void SelectGrid(int32 Index);
	FReply AddGrid(bool bVertical);
	FReply RemoveGrid();
	FReply ClearGrids();

	UInv_InventoryGridAsset* GridAsset{nullptr};
	int32 SelectedGrid{INDEX_NONE};
	TSharedPtr<SInv_InventoryGridPreview> Preview;
	TSharedPtr<IDetailsView> AssetDetails;
	TSharedPtr<IDetailsView> GridDetails;
	TSharedPtr<SWidget> PropertiesWidget;
};
