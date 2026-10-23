#include "InventoryEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Engine/DataTable.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "DataTableEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Items/Definitions/Inv_GameplayTagTableRow.h"
#include "Items/Definitions/Inv_ItemDefinitionTableRow.h"
#include "Inv_ItemImportLibrary.h"
#include "Grid/Inv_InventoryGridAssetActions.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "InventoryEditor"

namespace
{
	/** 导入按钮复用的标签表查找和提示；不接管原生 DataTable 编辑器。 */
	class FInvDataTableAssetTypeActions
	{
	public:
		static UDataTable* FindTagDataTable()
		{
			FAssetRegistryModule& AssetRegistryModule =
				FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByClass(
				UDataTable::StaticClass()->GetClassPathName(), AssetData, true);
			for (const FAssetData& Data : AssetData)
			{
				if (UDataTable* DT = Cast<UDataTable>(Data.GetAsset()))
				{
					if (DT->GetRowStruct() == FInv_GameplayTagTableRow::StaticStruct())
					{
						return DT;
					}
				}
			}
			return nullptr;
		}

		static void ShowNotification(const FText& Message, bool bSuccess)
		{
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			if (TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info))
			{
				Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
			}
		}
	};
}

class FInventoryEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 打开三类物品表时提供显式导入入口；读取或重导入 CSV 不生成物品资产。
		FDataTableEditorModule& DataTableEditor = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
		FAssetEditorExtender ToolbarDelegate = FAssetEditorExtender::CreateLambda(
			[](const TSharedRef<FUICommandList> Commands, const TArray<UObject*> Objects)
			{
				TSharedRef<FExtender> Extender = MakeShared<FExtender>();
				UDataTable* Table = Cast<UDataTable>(Objects[0]);
				if (!Table->GetRowStruct()->IsChildOf(FInv_ItemDefinitionTableRow::StaticStruct())
					|| Table->GetRowStruct() == FInv_ItemDefinitionTableRow::StaticStruct())
					return Extender;
				Extender->AddToolBarExtension("Asset", EExtensionHook::After, Commands,
					FToolBarExtensionDelegate::CreateLambda([Table](FToolBarBuilder& Toolbar)
					{
						Toolbar.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([Table]
						{
							const bool bSuccess = UInv_ItemImportLibrary::SyncItemAssetsFromDataTable(
								Table, FInvDataTableAssetTypeActions::FindTagDataTable(), true);
							FInvDataTableAssetTypeActions::ShowNotification(bSuccess
								? LOCTEXT("ImportAssetsOk", "物品资产导入完成")
								: LOCTEXT("ImportAssetsFail", "物品资产导入失败，请查看输出日志"), bSuccess);
						})), NAME_None, LOCTEXT("ImportAssets", "导入资产"),
							LOCTEXT("ImportAssetsTooltip", "将当前表格写入物品资产；保留手动配置的图片、装备 Actor 和插槽"),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));
					}));
				return Extender;
			});
		DataTableToolbarHandle = ToolbarDelegate.GetHandle();
		DataTableEditor.GetToolBarExtensibilityManager()->GetExtenderDelegates().Add(ToolbarDelegate);

		GridAssetTypeActions = MakeShared<FInv_InventoryGridAssetActions>();
		IAssetTools::Get().RegisterAssetTypeActions(GridAssetTypeActions.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		if (FDataTableEditorModule* DataTableEditor = FModuleManager::GetModulePtr<FDataTableEditorModule>("DataTableEditor"))
			DataTableEditor->GetToolBarExtensibilityManager()->GetExtenderDelegates().RemoveAll(
				[this](const FAssetEditorExtender& Delegate) { return Delegate.GetHandle() == DataTableToolbarHandle; });

		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools::Get().UnregisterAssetTypeActions(GridAssetTypeActions.ToSharedRef());
		}
		GridAssetTypeActions.Reset();
	}

private:
	FDelegateHandle DataTableToolbarHandle;

	TSharedPtr<IAssetTypeActions> GridAssetTypeActions;
};

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FInventoryEditorModule, InventoryEditor)
