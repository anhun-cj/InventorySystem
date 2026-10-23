#include "Grid/Inv_InventoryGridAssetEditor.h"

#include "Editor.h"
#include "Grid/Inv_InventoryGridPreview.h"
#include "IDetailsView.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "InvInventoryGridEditor"

namespace
{
	const FName PreviewTabName{TEXT("InvInventoryGridPreview")};
	const FName PropertiesTabName{TEXT("InvInventoryGridProperties")};
}

FInv_InventoryGridAssetEditor::~FInv_InventoryGridAssetEditor()
{
	GEditor->UnregisterForUndo(this);
}

void FInv_InventoryGridAssetEditor::InitEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InToolkitHost, UInv_InventoryGridAsset* Asset)
{
	GridAsset = Asset;
	GridAsset->SetFlags(RF_Transactional);
	for (UInv_InventoryGridConfig* Grid : GridAsset->Grids)
		Grid->SetFlags(RF_Transactional);
	SelectedGrid = GridAsset->Grids.IsEmpty() ? INDEX_NONE : 0;
	GEditor->RegisterForUndo(this);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.NotifyHook = this;
	AssetDetails = PropertyEditor.CreateDetailView(DetailsArgs);
	AssetDetails->SetObject(GridAsset);
	GridDetails = PropertyEditor.CreateDetailView(DetailsArgs);
	GridDetails->SetObject(GridAsset->Grids.IsValidIndex(SelectedGrid) ? GridAsset->Grids[SelectedGrid].Get() : nullptr);

	SAssignNew(Preview, SInv_InventoryGridPreview)
		.GridAsset(GridAsset)
		.SelectedGrid_Lambda([this]() { return SelectedGrid; })
		.OnGridSelected(this, &FInv_InventoryGridAssetEditor::SelectGrid);

	PropertiesWidget = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(6)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("SingleGridNotice", "游戏当前仅使用子网格 0，其余区域用于布局配置。"))
		.AutoWrapText(true)
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(6)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)
		[
			SNew(SButton).Text(LOCTEXT("AddGridRight", "向右添加"))
			.OnClicked(this, &FInv_InventoryGridAssetEditor::AddGrid, false)
		]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
		[
			SNew(SButton).Text(LOCTEXT("AddGridDown", "向下添加"))
			.OnClicked(this, &FInv_InventoryGridAssetEditor::AddGrid, true)
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(6, 0, 6, 6)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)
		[
			SNew(SButton).Text(LOCTEXT("RemoveGrid", "删除选中网格"))
			.IsEnabled_Lambda([this]() { return GridAsset->Grids.IsValidIndex(SelectedGrid); })
			.OnClicked(this, &FInv_InventoryGridAssetEditor::RemoveGrid)
		]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0, 0, 0)
		[
			SNew(SButton).Text(LOCTEXT("ClearGrids", "清空子网格"))
			.IsEnabled_Lambda([this]() { return !GridAsset->Grids.IsEmpty(); })
			.OnClicked(this, &FInv_InventoryGridAssetEditor::ClearGrids)
		]
	]
	+ SVerticalBox::Slot().FillHeight(0.45f)
	[
		AssetDetails.ToSharedRef()
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(6)
	[
		SNew(STextBlock).Text_Lambda([this]()
		{
			return SelectedGrid == INDEX_NONE ? LOCTEXT("NoGridSelected", "请添加子网格")
				: FText::Format(LOCTEXT("SelectedGrid", "选中子网格：{0}"), FText::AsNumber(SelectedGrid));
		})
	]
	+ SVerticalBox::Slot().FillHeight(0.55f)
	[
		GridDetails.ToSharedRef()
	];

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("InvInventoryGridEditor_Layout_v1"))
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.7f)->AddTab(PreviewTabName, ETabState::OpenedTab))
			->Split(FTabManager::NewStack()->SetSizeCoefficient(0.3f)->AddTab(PropertiesTabName, ETabState::OpenedTab)));
	InitAssetEditor(Mode, InToolkitHost, TEXT("InvInventoryGridEditor"), Layout, true, true, GridAsset);
}

void FInv_InventoryGridAssetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(PreviewTabName, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
		[
			SNew(SScrollBox).Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SNew(SScrollBox).Orientation(Orient_Vertical)
				+ SScrollBox::Slot().Padding(16)
				[
					Preview.ToSharedRef()
				]
			]
		];
	})).SetDisplayName(LOCTEXT("PreviewTab", "布局预览"));
	InTabManager->RegisterTabSpawner(PropertiesTabName, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)[PropertiesWidget.ToSharedRef()];
	})).SetDisplayName(LOCTEXT("PropertiesTab", "配置"));
}

void FInv_InventoryGridAssetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(PreviewTabName);
	InTabManager->UnregisterTabSpawner(PropertiesTabName);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

FText FInv_InventoryGridAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("EditorName", "库存网格布局");
}

void FInv_InventoryGridAssetEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent,
	FProperty* PropertyThatChanged)
{
	GridAsset->MarkPackageDirty();
	RefreshPreview();
}

void FInv_InventoryGridAssetEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
		RefreshPreview();
}

void FInv_InventoryGridAssetEditor::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

void FInv_InventoryGridAssetEditor::SaveAsset_Execute()
{
	if (GridAsset->Grids.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("EmptyLayout", "保存前请至少添加一个子网格。"));
		return;
	}
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FInv_InventoryGridAssetEditor::RefreshPreview()
{
	SelectedGrid = GridAsset->Grids.IsEmpty() ? INDEX_NONE
		: FMath::Clamp(SelectedGrid, 0, GridAsset->Grids.Num() - 1);
	GridDetails->SetObject(SelectedGrid == INDEX_NONE ? nullptr : GridAsset->Grids[SelectedGrid].Get());
	Preview->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void FInv_InventoryGridAssetEditor::SelectGrid(int32 Index)
{
	SelectedGrid = Index;
	RefreshPreview();
}

FReply FInv_InventoryGridAssetEditor::AddGrid(bool bVertical)
{
	const FScopedTransaction Transaction(LOCTEXT("AddGridTransaction", "添加库存子网格"));
	GridAsset->Modify();
	UInv_InventoryGridConfig* Grid = NewObject<UInv_InventoryGridConfig>(GridAsset, NAME_None, RF_Transactional);
	if (!GridAsset->Grids.IsEmpty())
	{
		const UInv_InventoryGridConfig* ReferenceGrid = GridAsset->Grids[SelectedGrid];
		Grid->PositionOffset = ReferenceGrid->PositionOffset
			+ (bVertical
				? FVector2D(0, ReferenceGrid->Row * GridAsset->TileSize + GridAsset->SpaceBetweenElements)
				: FVector2D(ReferenceGrid->Col * GridAsset->TileSize + GridAsset->SpaceBetweenElements, 0));
	}
	SelectedGrid = GridAsset->Grids.Add(Grid);
	RefreshPreview();
	return FReply::Handled();
}

FReply FInv_InventoryGridAssetEditor::RemoveGrid()
{
	const FScopedTransaction Transaction(LOCTEXT("RemoveGridTransaction", "删除库存子网格"));
	GridAsset->Modify();
	GridDetails->SetObject(nullptr);
	GridAsset->Grids.RemoveAt(SelectedGrid);
	RefreshPreview();
	return FReply::Handled();
}

FReply FInv_InventoryGridAssetEditor::ClearGrids()
{
	const FScopedTransaction Transaction(LOCTEXT("ClearGridsTransaction", "清空库存子网格"));
	GridAsset->Modify();
	GridDetails->SetObject(nullptr);
	GridAsset->Grids.Empty();
	RefreshPreview();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
