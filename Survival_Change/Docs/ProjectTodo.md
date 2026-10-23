# 项目待办

提醒规则：每次 Codex 开始处理本项目的新任务时，简短提醒未完成项。同一次任务不重复提醒，不自动实施待办。

## 未完成

- [ ] 拖拽物品靠近网格可视底边时自动向下滚动，越靠近底边速度越快。
  - 记录日期：2026-10-19。当前仅讨论方案，尚未实现。
  - 在 `UInv_InventoryGrid` 中处理各自的 `ScrollBox`。
  - 只在拖拽期间启动定时器，避免所有网格一直在 Tick 中检查。
  - 鼠标必须在 ScrollBox 可视区域内；用 ScrollBox 几何将鼠标绝对坐标转换为局部坐标。
  - 距离底边 = 可视高度 - 鼠标局部 Y，不使用整个网格内容的底边，也不减滚动偏移。
  - 底部触发区域内采用线性速度：`MaxScrollSpeed * Clamp(1 - DistanceToBottom / ScrollArea, 0, 1)`。
  - 仅在当前偏移小于 `GetScrollOffsetOfEnd()` 时推进，最终偏移不超过末尾。
  - 定时器读取当前鼠标位置；不沿用旧拖拽事件的鼠标坐标。
  - 拖拽离开、结束、取消，以及网格解绑或销毁时停止定时器。
  - 滚动导致布局更新后，复用现有落点与高亮计算，支持鼠标停住时继续更新预览。
  - 不主动增加网格行数，不修改服务器库存逻辑。
  - 实施前仍遵守用户约定：新增函数、类须先说明并取得同意，复用现有逻辑，不添加无意义或重复判断。

## 待完善

- [ ] 按物品类型整理资产配置，只显示对应类型需要的字段，并同步调整导入与清单生成（本轮实施完成，待完善）。
  - 记录日期：2026-10-19。本轮更新日期：2026-10-23。用户要求本轮完成后保留为“待完善”。
  - 已完成三类独立 DataTable 行结构和表格：装备 4 条、消耗品 2 条、普通物品暂空。只使用当前蓝图及场景已实现的六份配置，不恢复此前删除的十份示例资产。
  - 装备仅配置装备效果及可选制作材料；消耗品配置消耗效果及可选制作材料；普通物品不可使用、装备或制作，可配置获取途径，也可以成为其他配方的材料。
  - 装备效果与消耗效果共用对应实际修改器的数值；详情显示描述、对应效果、制作材料或获取途径，无内容时隐藏。当前没有实际制作执行或 GAS 接入。
  - 图标、装备 Actor 和插槽在定义资产选择，表格重导入不覆盖手动资源。装备不显示堆叠配置。原分解数据及逻辑保留，是否彻底删除仍待确认。
  - 标签表已按原生代码中现有的 17 个物品及属性标签整理，删除未注册示例标签，保留现有物品表引用的编号；原标签表路径继续使用，重复标签表已删除。
  - 用户要求的遗留配置清理已完成：装备 Actor 使用现有头盔标签，旧槽模板失效标签清空，拾取蓝图和两份场景物品清单按当前定义重建并保存。独立重载后原有 6 条失效标签警告已消除。
  - 待完善：红药原文字为回复生命，实际修改器却为法力 10，当前按原实际类型保留；现有头盔仍叫斗篷/斗篷1，头盔力量与蓝药法力原配置为 0，后续由用户确认数值和名称；实际物品配方与普通物品待有真实配置时填写。
  - 待验证：详情 UI 实际布局、游戏使用效果及客户端联机交互。图标分辨率与网格 TileSize 的显示一致性可在此阶段继续检查。
  - 验证：C++ 编译成功；12 项独立进程检查通过，包含 46 个库存蓝图编译、三表重导入、资源保留、场景和拾取蓝图绑定、详情片段及标签清理；最终日志 0 错误、0 警告。临时验证对象未保存为资产，临时脚本在交付前删除。
  - 详细配置及待完善范围：`Docs/ItemAssetConfiguration.md`。代码主要位于 `Plugins/Inventory/Source/Inventory/Public/Items/Definitions/Inv_ItemDefinition.h`、`Inv_ItemDefinitionTableRow.h`，以及对应清单构建和 `InventoryEditor/Private/Inv_ItemImportLibrary.cpp`。
  - 验证记录：`Saved/CodexTypedItemConfigBuild.log`、`Saved/CodexTypedItemConfigVerifyResult.json`、`Saved/CodexTypedItemsVerify.log`。备份：`Saved/CodexTypedItemConfigBaseline`。
  - 2026-10-23 继续修改导入流程：CSV 仅更新表格，查看或编辑后点击表内“导入资产”按钮才生成或更新定义；保留资产手动配置的图片、装备 Actor、插槽。已移除接管原生表格编辑器的旧资产动作注册和自动同步委托；最终 C++ 编译成功，6 项导入检查通过，日志 0 错误、0 警告。覆盖 CSV 不改资产、新普通物品行不自动创建资产、显式同步更新字段、连续两次导入保留手动资源、六份物品资产文件保持不变；工具栏按钮尚未人工点击验证。三张现有表已保存 CSV 来源，支持原生重新导入。验证记录：`Saved/CodexManualItemImportCheckResult.json`、`Saved/CodexManualItemImportCheck.log`。
  - 后续继续复用现有逻辑，新增函数或类先告知用户并取得同意，不添加无意义或重复判断。

## 已完成

- [x] 完善双击装备时，对应装备槽已有装备的处理。
  - 记录及完成日期：2026-10-22。
  - 用户确认：旧装备按主库存、已装备背包、弹挂顺序寻找位置，全部放不下则取消；即将装备的物品腾出的占格算空余空间。
  - 客户端先移除新装备的网格占用，再复用现有 `AddItem` 查询各库存全部子网格；无位置则恢复新装备 UI，不发送换装请求。成功时复用现有装备、卸装显示逻辑。
  - 现有 `Server_EquipItem` 增加 `bFindUnequipSpace` 参数，双击换装由服务器重新按上述顺序查询，排除即将装备的物品，找到位置后才修改库存；普通拖拽仍处理指定位置。被卸下的容器不作为自身的存放目标。
  - 未新增函数或类。相关代码：`Plugins/Inventory/Source/Inventory/Private/Widget/Inventory/SlottedItems/Inv_SlottedItem.cpp` 中的 `NativeOnMouseButtonDoubleClick`，以及 `Plugins/Inventory/Source/Inventory/Private/InventoryManagement/Components/Inv_RequestComponent.cpp` 中的 `Server_EquipItem_Implementation`。
  - 验证：C++ 编译通过；五项服务器检查通过，覆盖复用腾出占格、主库存优先、背包回退、弹挂回退、全部无空间时数据不变。记录：`Saved/CodexDoubleClickEquipBuild.log`、`Saved/CodexDoubleClickEquipCheckResult.json`。实际客户端双击、预览及联机回弹尚未交互验证；临时验证脚本已删除。

- [x] 修复 `CheckHoverPosition()` 将右侧越界坐标转换成下一行合法索引的问题。
  - 完成日期：2026-10-19。
  - 已改为直接检查二维落点范围，C++ 编译通过，1,587,600 个数值边界组合检查无差异。
  - 实际游戏拖拽和联机效果尚未进行交互验证。
  - 详细记录：`Docs/InventorySqueezeAlgorithmReview.md`。

## 已取消

- 2026-10-21：物品旋转放置及整组挤压旋转方案。用户要求移出待办，未实施，后续不再提醒。
