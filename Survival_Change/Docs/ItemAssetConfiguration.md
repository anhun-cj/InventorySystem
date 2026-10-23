# 物品资产与独立表格配置

更新日期：2026-10-23。当前状态：待完善。

## 三类表格

| 类型 | DataTable | CSV | 行结构 | 当前行数 |
| --- | --- | --- | --- | --- |
| 装备 | `/Game/Inventory/DT_EquipmentItems` | `Content/Inventory/ImportTemplates/Inv_EquipmentItems.csv` | `FInv_EquipmentItemDefinitionTableRow` | 4 |
| 消耗品 | `/Game/Inventory/DT_ConsumableItems` | `Content/Inventory/ImportTemplates/Inv_ConsumableItems.csv` | `FInv_ConsumableItemDefinitionTableRow` | 2 |
| 普通物品 | `/Game/Inventory/DT_CommonItems` | `Content/Inventory/ImportTemplates/Inv_CommonItems.csv` | `FInv_CommonItemDefinitionTableRow` | 0 |

三类表格共享编号、名称、描述、物品标签和占格字段，但使用独立行结构。普通物品配置堆叠和获取途径；装备配置装备效果、装备槽和可选制作材料，不配置堆叠；消耗品配置堆叠、消耗效果和可选制作材料。普通物品不能使用、装备或制作，但允许成为其他物品的制作材料。

图标在对应物品定义资产上选择；装备 Actor 类和骨骼插槽只在装备定义资产上选择。重新导入表格不覆盖这些手动资源配置。容器布局仍由原来的网格资产及库存逻辑负责。

## 当前物品

仅根据当前拾取蓝图和场景中实际配置生成六份定义，路径为 `/Game/Items/DA_Item_<ItemId>`。

| ItemId | 当前名称 | 类型 | 保留的实际效果 |
| --- | --- | --- | --- |
| 10001 | 斗篷 | 头盔槽装备 | 力量 0 |
| 10002 | 斗篷1 | 头盔槽装备 | 力量 0 |
| 10003 | 标致背包 | 背包装备 | 未配置 |
| 10004 | 普通弹挂 | 弹挂装备 | 未配置 |
| 20001 | 小型能量药水 | 消耗品 | 法力 0 |
| 20002 | 小型生命药水 | 消耗品 | 原修改器为法力 10，见下方待完善项 |

两份场景头盔分别绑定 10001、10002；拾取蓝图默认头盔绑定 10001。背包、弹挂和两种药水的现有拾取组件也已绑定对应定义。普通物品表暂时为空，没有恢复此前删除的十份示例资产，没有新增虚构物品、属性值或制作配方。

## 标签及导入

标签表保留项目原路径 `/Game/Inventory/ImportTemplates/Inv_GameplayTags_Template`，对应同目录 CSV。内容与 `Plugins/Inventory/Source/Inventory/Private/Items/Inv_ItemTags.cpp` 中的 17 个已有物品和属性标签一致，不纳入清单内部使用的 `FragmentTags`。表格负责数字编号映射，不负责注册新的 GameplayTag。

原本本轮已引用的 30001～30007 编号保持不变，补充已有标签使用 30008～30017。旧模板中未注册的 Axe、Sword、Cloaks、Masks、Slot 和 AttackPower 等标签已移除。不要随意重排正在使用的 TagId。ItemId 在三类物品表之间也必须唯一。

CSV 中多个标签使用 `TagId|TagId`，效果使用 `TagId&Value|TagId&Value`，制作材料使用 `ItemId&Count|ItemId&Count`。材料引用目标物品定义，由定义中的名称生成详情文字。空制作材料表示不能制作。

双击打开相应 DataTable，在表格中查看和编辑名称、占格、标签编号、效果及材料；确认后点击工具栏“导入资产”，才创建或更新对应物品定义。移除旧的全局 DataTable 资产动作注册，使用 UE 原生表格编辑器打开，不再通过旧的右键同步入口。

导入、重新导入 CSV、打开或保存表格都不再自动生成物品资产。`ImportCsvAndSync` 保留原 C++ 函数名与签名，蓝图显示名改为“导入物品 CSV 到表格”，现在仅更新表格；`ImportGameplayTagCsv` 仍只导入标签表。CSV 来源路径会写入表格的导入信息，供原生 Reimport 使用。CSV 导入时用 `ItemKind` 参数选择独立表类型，表内不再配置混合类型列。显式导入资产时才通过标签表解析数字编号。

“导入资产”更新表格管理的名称、描述、标签、占格、效果、配方及堆叠等字段；资产上单独选择的 `Icon`、`EquipActorClass` 和 `SocketAttachPoint` 不参与赋值，重复导入也保留。表格管理的字段若直接在资产修改，下次显式导入仍以表格内容为准。

## 详情显示与清单

沿用 `Survival_ItemDescription` 和既有文本叶子控件，增加描述、装备效果、消耗效果、制作材料、获取途径五个可选区域。初始化时折叠，清单包含对应文字片段才显示；没有内容的区域不显示。

装备效果和消耗效果使用各自定义资产中的同一份数值生成实际修改器及详情文字。制作材料展示读取同一份配方引用。本次没有新增 GAS 接入、制作执行或材料扣除系统。

保留原分解数据及导入逻辑，隐藏当前不需要手动配置的字段，未删除分解功能。修正了清单构建时派生片段的构造方式，避免文本与具体效果被切成基础片段。

## 遗留标签清理

旧 `Survival_RedCloak` 装备 Actor 的 `EquipTag` 已改为实际使用的 `GameItems.Equipment.Helmets.Helmet`。两个旧装备槽模板中失效的 `Cloaks.RedCloak` 默认标签已清空。拾取蓝图和两份场景物品清单通过现有定义初始化函数重新生成并保存，移除旧清单内的失效标签。保留原名称、图标、外观资源和布局，不因清理改动用户的资源命名。

## 待完善范围

- 小型生命药水原来的文字写“回复生命值”，但实际修改器类型是 `Inv_ManaPotionFragment`，数值为 10。本轮按原实际类型保留，未擅自改为生命恢复，需要后续确认。
- 现有两份头盔沿用“斗篷”“斗篷1”的名称和旧外观资源；力量 0、蓝药法力 0 同样按已有值保留。是否改名、改数值由用户后续决定。
- 当前实际物品未配置制作配方；普通物品表无实际条目，后续有真实配置再填写。
- 详情 UI 的实际游戏布局、客户端效果和联机流程尚需交互验证。
- 分解配置是否彻底移除仍待用户决定。

## 验证与备份

C++ 编译记录：`Saved/CodexTypedItemConfigBuild.log`。独立进程验证结果及日志：`Saved/CodexTypedItemConfigVerifyResult.json`、`Saved/CodexTypedItemsVerify.log`。覆盖三类独立表、标签表与原生标签对应、六份实际定义及已保存绑定、资源在重导入后保留、可选详情片段、制作材料名称和数量、装备不堆叠、46 个现有库存蓝图编译，以及遗留标签配置重载。

修改前的代码、CSV、主要蓝图和场景备份位于 `Saved/CodexTypedItemConfigBaseline`；遗留标签清理前的额外资产备份位于其 `LegacyTagCleanup` 目录。验证材料和配方使用临时对象，没有保存为游戏资产。临时 Python 检查脚本在交付前删除。

2026-10-23 导入流程补充：取消逐行自动创建资产的编辑器委托桥接，在现有模块启动逻辑中扩展 DataTable 工具栏，不新增类或命名函数。表格内的原始编号、效果和材料字段改为可查看、可编辑。此次修改前备份：`Saved/CodexManualItemImportBaseline`。编译及检查结果在 `Saved/CodexManualItemImportBuild.log` 与后续验证记录中保存。

本次导入流程验证：最终 C++ 编译成功；`Saved/CodexManualItemImportCheckResult.json` 记录的 6 项检查通过，`Saved/CodexManualItemImportCheck.log` 为 0 错误、0 警告。检查包含原生表格填充与显式同步，未人工点击工具栏按钮。六份定义资产的文件哈希在验证前后相同；测试中的名称、资源替换及新增普通物品行均未保存。现有三张表只保存了 CSV 来源信息，测试脚本已删除。
