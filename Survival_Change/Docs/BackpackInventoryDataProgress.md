# 背包内部库存数据层进度

日期：2026-10-20。

第一阶段完成 `UInv_InventoryItem` 的内部库存数据结构，以及现有 FastArray 和组件复制管理的适配。随后接入装备背包网格显示。现已接通拾取回退、内部存取、交换、换装、拆分与使用的服务器路由，并补充地面库存数据交接和容器循环校验，见文末。实际游戏 UI 及独立客户端复制尚未交互验证。

## 数据结构

`UInv_InventoryItem` 新增三个不可编辑、参与复制的字段：

- `ParentContainerItem`：直接收纳该物品的容器物品；为空表示组件顶层库存。
- `InventoryList`：该物品自己的 `FInv_InventoryFastArray`，构造时拥有者为该物品自身；普通物品保持空数组。
- `GridAsset`：该物品内部库存的布局资产；从地面 `ItemComponent.InventoryGridAsset` 记录。

现有 `OwnerInventory` 表示负责管理与复制的库存组件，不再足以单独表达具体库存区域。内部条目变更通知通过物品自身的 `OnItemAdded`、`OnItemChanged`、`OnItemRemoved` 广播；现有组件顶层通知继续保留。

## FastArray 适配

- 现有有参构造的参数从组件推广为 `UObject*`，支持组件和物品两种拥有者。
- 新增唯一的辅助函数 `GetInventoryComponent() const`，已取得用户明确同意。
- 保留现有增删改函数，通过辅助函数取得管理组件。
- 内部数组添加条目时记录父容器；顶层添加及装备时清除父容器。
- 内部数组复制回调只通知所属物品，不向组件广播内部条目的顶层 UI 事件。
- 在现有 `NetDeltaSerialize()` 中恢复当前序列化对象作为数组拥有者，继续使用标准 FastArray 增量序列化，不增加自定义复制版本协议。
- `FInventoryChange` 既有委托声明移到 FastArray 头文件，使组件与物品复用同一种通知类型。

## 管理组件与复制注册

- 现有 `SetInventoryComponent()` 同步更新内部子物品的管理组件。
- 现有 `AddRepSubObj()`、`RemoveRepSubObj()` 对内部子物品递归注册、注销。
- 没有新增类、RPC 或 UI 函数。

## 验证

- C++ 和 UHT 检查通过：`Saved/CodexItemContainerBuild.log`。
- 两个独立临时物品实例的内部数组分别绑定自身、初始为空：通过。
- 45 个插件蓝图编译：通过。
- 结果：`Saved/CodexItemContainerCheckResult.json`，日志：`Saved/CodexItemContainerCheck.log`。
- 尝试验证服务器拾取时，无界面进程在创建测试地面 Actor 的阶段发生除零崩溃，未执行到拾取验证。失败日志保留在 `Saved/CodexItemContainerActorCheckFailed.log`。未保存测试关卡或资产，临时脚本已删除。
- 内部条目增删改、嵌套复制、跨组件迁移和实际游戏 UI 尚未进行运行验证，不能视作已跑通完整背包功能。

## 下一阶段的连接点

以下为数据层阶段的原始连接点；本次已接通的范围和验证结果见文末。地面背包按 F 打开其内部库存、多层容器窗口、存档加载不在本轮实现范围。

1. 服务器请求区分组件顶层数组和具体容器物品内部数组。相同组件下的不同背包不能继续只凭组件相同就当成同一库存。
2. 所有内部操作对正确数组校验来源、位置和占用；服务器防止自身收纳和祖先循环，建立无环容器关系后再通过递归更新管理与复制。
3. 地面组件、库存和装备表现关联同一个背包实例，避免当前按清单重新创建物品的流程丢失内部数组。
4. UI 绑定具体背包的内部数组和物品级通知，接上现有 `BackPackInventory`，并覆盖菜单拖拽获取、清理和回弹刷新路径。

基线：`Saved/CodexItemContainerBaseline.json`，包含本阶段修改前的六个源码文件。

## 装备背包网格显示（2026-10-20）

- 复用菜单已有的 `BackPackInventory` 控件，在 `EquipItem()` 收到装备更新后，根据该物品的 `GridAsset` 重建网格槽并显示。
- 给现有 `BindInventoryEvent()` 增加可选参数 `InContainerItem`；绑定背包时读取物品自身的布局、内部 FastArray 和物品级增删改通知，不占用角色组件的 `BoundInventoryGrid`。
- `InitInventory()` 和现有物品重建函数选择背包内部数组，显示条目记录的位置和数量。
- 卸下对应背包、服务器拒绝后的整体恢复和菜单销毁时解绑；卸下时清理 UI 并隐藏网格，不清空内部数据。
- 本轮仅展示内部库存，保留滚动操作。格子和物品图标暂不参与鼠标命中；落到此区域的拖拽在菜单中取消，避免误进入现有顶层存取或丢弃请求。接入内部库存请求后需解除此限制。
- 复用现有网格槽类，无新增函数或类。源码变更仅涉及 `Inv_InventoryGrid.h`、`Inv_InventoryGrid.cpp`、`Inv_SpatialInventory.cpp`。
- C++ 编译通过：`Saved/CodexEquippedBackpackGridBuild.log`。
- 无界面 Python 命令模式检查通过：45 个蓝图编译、`BackPackInventory` 控件绑定、背包布局 6 列 10 初始行；进程正常退出。日志为 `Saved/CodexEquippedBackpackGridCheck.log`，结果为同名前缀 `CheckResult.json`。
- 首次编辑器模式检查在恢复旧资产编辑页后退出时触发 `ComponentAssetBroker` 断言，保留日志 `Saved/CodexEquippedBackpackGridShutdownFailed.log`；改用不恢复资产编辑页的命令模式后完成验证，没有修改项目配置。
- 未进行实际游戏装备、显示或联机交互验证；未保存或修改蓝图资产。临时验证脚本已删除。
- 本轮源码基线：`Saved/CodexEquippedBackpackGridBaseline.json`。

## 解绑时清空网格槽（2026-10-20）

- 按用户要求，将物品 UI、弹出菜单、高亮状态、WrapBox 子控件和 `GridSlots` 数组的清理统一放在现有 `UnBindInventoryEvent()` 末尾。
- 容器解绑、组件解绑和未绑定状态都执行清理，不再通过提前返回跳过。
- 删除绑定开始时和菜单各解绑调用点重复的清理语句；下一次绑定时按资产重新生成格子。
- 只清理 UI，不清空背包内部库存条目。
- C++ 编译通过：`Saved/CodexGridUnbindClearBuild.log`。实际游戏中的卸下和重新装备尚未交互验证。
- 基线：`Saved/CodexGridUnbindClearBaseline.json`。

## 拾取回退与背包内部操作（2026-10-20）

本节的地面引用缓存为当时实现，已由文末“掉落时交接给地面库存组件”替换。

- `Server_PickupItem()` 先查询角色库存，再按顶层条目顺序尝试已装备且有布局资产的背包。堆叠物品按剩余数量依次处理，放不下的数量留在地面；所有位置与数量由服务器计算。
- 复用 `TransferItem()`，增加来源、目标容器参数；来源校验包含父容器和原格子位置。`MoveItemToThisInventory()` 根据实际容器选择数组与布局资产，组件相同但容器不同仍按跨库存处理。
- 保留服务器落点与交换占用校验；按全部预定落点检查交换后的父级关系，拒绝自身收纳、祖先循环和多物品交换形成的间接循环。
- 复用装备请求增加来源容器参数，包内装备可以装入装备槽，旧装备返回对应来源网格；卸下到背包时使用其资产尺寸检查空间。没有增加新旧装备之间的类型匹配。
- 拆分、使用、移除和丢弃均选择物品真正所属数组，内部条目通知所属背包，顶层通知保持原组件逻辑。拆分在服务器确认新格位空间；附近库存继续使用原地面拆分流程。
- 背包网格解除只读限制，复用现有拖拽、交换和预览。主界面查找及清理 HoverItem 包含背包网格；服务器拒绝后继续复用原整体 UI 刷新。
- `UInv_ItemComponent.InventoryItem` 为不可编辑的服务器缓存。掉落容器保存原背包引用，真实拾取时将内部数组交接到新背包并修正数组拥有者、子物品父级及复制管理组件；附近网格拖入时在原移除回调完成同样交接。原子物品实例和格子索引保留。
- 没有新增 C++ 函数、类或 RPC；扩展现有参数、局部放置记录和缓存成员。没有修改蓝图资产或添加 Tick 轮询。

验证范围：

- C++ / UHT 编译通过：`Saved/CodexBackpackTransferBuild.log`。
- 无界面 Unreal 进程调用现有服务器入口，16 项检查通过：包括满库存拾取回退、父级归属、背包间转移与交换、内部移位、取回顶层库存、跨库存补堆叠及地面余量、缓存内容与位置交接、附近网格拖入容器、背包换装与卸装、内部拆分，以及多物品交换形成祖先循环时不修改数据。
- 45 个插件蓝图编译通过。结果为 `Saved/CodexBackpackTransferCheckResult.json`，日志为 `Saved/CodexBackpackTransferCheck.log`。
- 检查仅创建临时对象并调用服务器本地入口，没有模拟客户端网络、丢包或实际鼠标交互；地面缓存检查使用临时拾取组件，不代表已验证真实掉落 Actor 的完整生成与操作流程。
- 尝试为额外拒绝检查创建原生 PlayerController 时，无界面编辑器的 Actor 工厂发生访问冲突；停止使用该工厂，改由已有 `Server_MoveItem()` 检查循环拒绝分支。失败日志保留为 `Saved/CodexBackpackTransferActorFactoryFailed.log`，最后一轮检查正常退出。
- 临时脚本已删除，未保留测试源码或测试资产。源码基线：`Saved/CodexBackpackTransferBaseline.json`。

## 判断去重与入口分流（2026-10-20）

- 转移请求复用装备请求已有的退出回执写法，按对象、预览、来源状态和目标容器分段提前返回，拒绝时仍执行原 UI 恢复通知。
- 预览不可放置和来源为装备状态的判断提前到两个服务器移动入口；共用放置函数只保留实际数组归属、空间占用和父级循环计算。
- 附近物品拆分先在请求入口分流，数量、库存成员和堆叠检查由附近拆分分支执行一次；删除其被调用前已成立的对象有效性、组件相等和服务器权限检查，并只查找一次对应地面组件。
- 删除拾取后重复设置管理组件，以及已验证服务器调用链中新增条目必定返回有效对象后的重复空值判断。FastArray 本身的创建前置检查保留。
- 丢弃请求不重复判断附近库存，共用丢弃逻辑统一限制；物品有效性在两个服务器入口检查，生成函数继续统一解析并验证角色与世界。
- 装备检查按新装备来源、装备槽匹配、旧装备状态分开提前返回；没有增加新旧装备类型相互匹配。
- 只修改三个现有组件实现文件，未新增函数、类或参数，未修改 UI 和布局资产。
- C++ 编译通过：`Saved/CodexInventoryValidationCleanupBuild.log`。无界面服务器入口检查五项通过：满库存拾取回退、背包拆分、非法拆分数量和索引拒绝、非法移动预览拒绝、有效背包移动；结果为 `Saved/CodexInventoryValidationCleanupCheckResult.json`。实际客户端交互未验证。
- 临时检查脚本已删除。修改前基线：`Saved/CodexInventoryValidationCleanupBaseline.json`。

## 掉落背包的打开界面配置（2026-10-20）

- 根因：`Lvl_ThirdPerson` 中背包实例的库存组件单独配置了 `OpenInventoryClass = WareHouse_C`，但 `Backpack` 蓝图的 `Survival_InventoryComponent` 组件模板为空。掉落按蓝图类生成，不继承关卡实例的覆盖配置。
- 在 `/Inventory/Items/Equipment/Backpack` 蓝图的组件模板中设置同一个 `WareHouse_C`，使运行时生成的背包取得该默认值。仅保存这一份蓝图资产，没有修改 C++、关卡或添加运行时兜底。
- 独立无界面进程重新加载后确认配置仍为 `WareHouse_C`，背包蓝图编译通过；未验证实际游戏中掉落后打开 UI。结果：`Saved/CodexDroppedBackpackOpenClassCheckResult.json`。
- 修改前资产备份：`Saved/CodexDroppedBackpackOpenClassBaseline/Backpack.uasset`。临时脚本已删除。

## 掉落时交接给地面库存组件（2026-10-20）

- 修正此前缓存方案的遗漏：打开地面背包读取的是 Actor 上库存组件的数组，之前仅缓存原物品引用，没有填充该数组，打开后为空。
- `SpawnDroppedItem()` 在生成背包 Actor 后，将原背包内部条目转交给新 Actor 的库存组件，保留每件物品的格子索引。原内部数组清空；直接子物品改为地面组件顶层归属，嵌套内容随管理组件递归更新。
- 直接拾取时，FastArray 现有 `AddEntry(ItemComponent, ...)` 从地面库存组件取回条目，恢复到新背包内部数组；从附近网格拖走时，现有 `OnItemRemove()` 完成同样交接。保留内容物实例、数量和当前格子位置。
- 数组交接按条目复制并重新标脏，地面原数组通过 `ClearAllItems()` 清空，保留各数组自身的复制状态。注销原管理组件下的子对象，再由目标组件注册；地面移除通知仍走原委托。
- 删除 `UInv_ItemComponent.InventoryItem` 缓存成员及相关赋值，不保留两套地面数据来源。背包物品的内部数组和地面组件数组分别在对应状态下持有内容。
- 没有新增函数、类或 RPC。背包 Actor 使用已有库存组件；没有增加组件缺失时的兼容路径。
- C++ / UHT 编译通过：`Saved/CodexGroundBackpackContentsBuild.log`。首次因编辑器启用 Live Coding 被拒绝，用户关闭编辑器后完成编译。
- 无界面 Unreal 进程通过真实 `Backpack` 蓝图与现有服务器入口完成 11 项检查：实际掉落组件持有两件物品、原包清空、地面归属、嵌套内容、打开界面类、地面内部移位、直接拾取保留修改后位置、物品身份和父级恢复、再次丢弃、附近网格取回、嵌套内容再次保留。进程正常退出，零错误；存在两条已有装备槽 GameplayTag 警告。
- 检查使用引擎运行时生成接口创建临时角色，避免编辑器 Actor 工厂；未保存测试关卡或资产。结果：`Saved/CodexGroundBackpackContentsCheckResult.json`。独立客户端网络同步及实际打开 UI 的画面未验证。
- 临时脚本已删除。修改前基线：`Saved/CodexGroundBackpackContentsBaseline.json`。

## H 键打开物品库存（2026-10-20）

- 复用玩家控制器现有 `OpenInventory()`，增加 `bOpenItem` 参数。H 传入 true，打开当前瞄准 Actor 的库存组件；没有该组件时不打开附近搜索。F 传入 false，瞄准拾取物品时使用附近库存，建筑仓库继续使用原库存组件。
- 沿用现有 `RequestComponent.OpenInventory()` 服务器请求和动态库存 UI，没有新增函数、类或 RPC。
- 新增 `OpenItemAction` 配置成员及 `/Inventory/Input/IA_OpenItemInventory` 动作资产，设置到 `Survival_PlayerController` 的默认值，映射 H。
- `IMCDefault` 的默认映射及现有配置覆盖均包含 H 打开物品、F 搜索／原建筑交互；保留 E 拾取和 B 打开角色库存。
- C++ / UHT 编译通过：`Saved/CodexOpenItemInputBuild.log`。独立进程重新加载后，控制器动作引用和默认／覆盖的 H、F 映射六项检查通过，45 个插件蓝图编译通过；实际按键、打开 UI 和客户端交互尚未验证。
- 结果：`Saved/CodexOpenItemInputCheckResult.json`。源码基线：`Saved/CodexOpenItemInputBaseline.json`，旧资产备份：`Saved/CodexOpenItemInputAssetBaseline/`。临时脚本已删除。

## 拖到容器物品图标直接存入（2026-10-20）

- `UInv_SlottedItem::NativeOnDrop()` 对具有 `GridAsset` 的目标物品优先走收纳路径，普通图标仍走原交换逻辑。清除来源 UI 和 HoverItem；装备来源调用现有卸装 UI 与服务器卸装请求。
- 复用 `TransferItem()`，以容器目标和 `INDEX_NONE` 落点表示自动存入。服务器从容器资产和真实数组构建布局，先补堆叠再找空位，按计划调用现有 `MoveItemToThisInventory()`。只合入部分数量时，余量保留在原数组；没有空间则通过原失败回执刷新 UI。
- 附近列表中的容器图标解析到对应地面 Actor 的真实库存组件，避免将内容存入临时代理物品。向已经所属的地面容器拖放不改变数据，并恢复 UI。
- 复用已有卸装入口为装备自动查找容器空位，放不下则保持装备状态。不增加装备堆叠判断或新旧装备类型匹配。
- 保留自身、祖先循环和真实数组检查；共用移动函数拒绝把附近列表的地面背包代理放入其自身 Actor 管理的库存，包括其内部子容器。
- 没有新增 C++ 函数、类、RPC 或参数。增加友元声明以复用已有卸装和地面组件查询函数，没有修改蓝图资产。
- C++ / UHT 编译通过：`Saved/CodexDropIntoContainerBuild.log`。无界面 Unreal 进程调用服务器入口完成 15 项行为检查，覆盖同组件和跨组件存入、多堆叠、部分合入余量、满容器拒绝、嵌套循环拒绝、装备存入及过大拒绝、真实地面库存写入与自身拒绝；45 个插件蓝图编译完成，进程正常退出且无错误。结果：`Saved/CodexDropIntoContainerCheckResult.json`，日志包含已有 GameplayTag 警告及临时反射配置产生的控制台性能警告。
- 没有验证实际鼠标命中、画面和独立客户端网络时序。检查没有保存测试资产或关卡，临时检查脚本已删除。修改前源码基线：`Saved/CodexDropIntoContainerBaseline.json`，额外 UI 头文件基线：`Saved/CodexDropIntoContainerSpatialHeaderBaseline.h`。

## 弹挂接入（2026-10-21）

- 复用用户已添加的 `ChestRigInventory`，初始化悬浮来源和网格父级；装备时按 `GameItems.Equipment.ChestRigs.ChestRig` 绑定弹挂网格，背包继续使用自己的网格，避免两个容器互相覆盖。
- 弹挂网格接入拖拽悬浮查询和清理、菜单销毁解绑、网格内拒绝落点处理，以及服务器拒绝后的整体 UI 恢复。卸装仅清理对应容器网格。
- 双击卸装依次查主库存、弹挂、背包；排除当前要卸下的容器自身。复用现有查询、预览和卸装请求，没有新增 C++ 函数、类、RPC 或成员。
- 新建 `/Inventory/InventoryManagement/Grid/IA_ChestRigInventory`，初始布局 6 列 3 行，沿用已有布局资产样式。用户可在资产中继续调整尺寸。
- 将弹挂布局绑定到 `ChestRig` 物品组件的 `InventoryGridAsset` 和地面库存组件模板的 `GridAsset`；保留已有 `WareHouse` 打开界面、图标、2×2 物品占格、装备 Actor 和 `ChestRig` 插槽设置。
- 将主库存 UI 内现有弹挂装备槽配置为 `GameItems.Equipment.ChestRigs.ChestRig`。服务器拾取、转移、掉落与内容交接复用已有容器逻辑。
- C++ 编译通过：`Saved/CodexConnectChestRigBuild.log`。独立 Unreal 进程重新加载后，17 项配置和服务器行为检查通过，45 个插件蓝图编译完成，进程无错误；保留已有 GameplayTag 警告。覆盖真实弹挂拾取、与背包同时装备、主库存满时向弹挂存入、背包与弹挂互转、满库存卸装拒绝、含物品卸装再装备、真实掉落及地面存取、再拾取保留当前索引。
- 结果：`Saved/CodexConnectChestRigCheckResult.json`。没有模拟实际鼠标、UI 画面和独立客户端网络。未保存检查关卡或测试资产；临时脚本已删除。
- 修改前备份：`Saved/CodexConnectChestRigBaseline.json`、`Saved/CodexConnectChestRigAssetBaseline/`。

## 自动存放优先背包（2026-10-21）

- 按用户要求，将当前自动存放顺序明确为主库存、已装备背包、已装备弹挂；服务器构建容器列表时将背包排在其他容器前面，不受库存条目或装备先后顺序影响。
- 双击卸装使用相同顺序，继续排除卸下容器自身；手动拖拽保持指定目标。
- C++ 编译通过：`Saved/CodexBackpackPriorityBuild.log`。服务器拾取入口四项检查通过：弹挂条目在前仍优先背包、背包满转入弹挂、主库存仍优先、全部满时库存不变。结果：`Saved/CodexBackpackPriorityCheckResult.json`。实际客户端鼠标操作未验证，临时检查脚本已删除。
- 未新增函数、类、RPC 或配置成员。源码备份：`Saved/CodexBackpackPriorityBaseline.json`。

## 网格背景与高度跟随布局（2026-10-21）

- `Survival_InventoryGrid` 背景改为水平和垂直填满外框，背景画刷期望尺寸设为 0，避免原 1362×734 图片尺寸参与撑大布局。
- 网格外框取消蓝图固定高度，保留最大显示高度 600。现有绑定函数和动态加行函数按 `Min(Row * TileSize, MaxDesiredHeight)` 设置实际外框高度，无新增函数、类、成员或 Tick 检查。
- 主库存、背包、弹挂和打开库存的父级 SizeBox 将原固定高度改为相同数值的最大显示高度，使短网格收缩，长网格保留原滚动区域。
- C++ 编译通过：`Saved/CodexGridHeightBuild.log`。独立重新加载五项蓝图布局配置检查通过，45 个插件蓝图编译完成，实际游戏画面未验证。结果：`Saved/CodexGridHeightVerifyResult.json`。
- 备份：`Saved/CodexGridHeightBaseline/`。临时检查和修改脚本已删除。

## 装备区域变化时固定主库存位置（2026-10-21）

- 按用户进一步确认，未装备、装备弹挂、装备背包及卸装时，主库存位置必须保持不变。
- 仅修改 `Survival_SpatialInventory` 蓝图布局：新增 `SizeBox_EquipmentArea`（固定高度 1100）和内部 `VerticalBox_EquipmentArea`，容纳原弹挂标题、弹挂区域及背包区域；主库存继续在外层纵向容器中，位于固定装备区域下方，间隔 10。
- 弹挂、背包外框取消固定高度，保留最大高度 400、600，最小高度 200 与左侧装备槽一致；内部按实际高度排列并顶部对齐。装备区域整体占位保持固定，因此主库存不随装备状态变化。
- 保留用户新增背景、原网格居中配置及装备数据逻辑；未新增 C++ 函数、类或运行时检查。
- 独立 Unreal 进程重新加载后，10 项配置及蓝图编译检查通过，进程无错误；实际游戏画面和鼠标操作未验证。结果：`Saved/CodexInventoryFlowVerifyResult.json`，日志：`Saved/CodexInventoryFlowVerify.log`。
- 修改前蓝图备份：`Saved/CodexInventoryFlowBaseline/Survival_SpatialInventory.uasset`。临时脚本已删除。

## 恢复装备容器网格居中（2026-10-21）

- 修正 `Survival_SpatialInventory` 中弹挂 `SizeBox_2` 的 Overlay 插槽，使其填满背景区域；背包 `Border_134` 的子级对齐改为填满。
- `SizeBox_2`、`SizeBox_4` 内实际网格的插槽设为水平、垂直居中，使不同尺寸网格相对各自背景居中。保留装备区域固定 1100 高的占位及主库存位置。
- 独立重新加载后五项配置检查和蓝图编译通过；实际游戏画面未验证。结果：`Saved/CodexGridCenterVerifyResult.json`。备份：`Saved/CodexGridCenterBaseline/Survival_SpatialInventory.uasset`。临时脚本已删除。

## 拖拽到容器物品时显示可存入颜色（2026-10-21）

- 在现有 `UInv_InventoryGrid::OnTileParametersUpdated` 中，根据鼠标所在格确定目标容器，优先处理存入预览。复用 `FInv_GridLayout::BuildOccupancy`、`Setup`、`Query` 检查目标资产的固定容量和真实内容，不自动扩容。
- 高亮覆盖目标容器整个占格范围；有可接收数量时绿色，无空间或同一容器内部物品拖回容器图标时红色。堆叠容量计入空间，沿用服务器允许部分存入的行为。拖入自身或自身后代容器仍不高亮；原普通网格交换逻辑保留。
- `ClearHoverItem` 复用现有 `UnHighlightSlots` 清除拖拽结束后的颜色。
- `InventoryItem` 新增可复制弱引用 `GroundInventory`。附近列表创建地面容器显示条目时赋真实库存组件，预览读取真实组件的资产与内容；实际归属改变时清除该引用，重复设置相同归属不清除。未新增函数、类或 RPC。
- C++ 编译通过：`Saved/CodexContainerHoverBuild.log`。独立 Unreal 进程检查真实地面弹挂的附近条目引用、附近登记不搬走地面内容、正常拾取物品没有地面引用，并编译 45 个库存蓝图；进程无错误，保留旧 GameplayTag 警告。检查结果：`Saved/CodexContainerHoverCheckResult.json`。实际拖拽颜色和独立客户端网络复制尚未交互验证。
- 修改前四份源码备份：`Saved/CodexContainerHoverBaseline/`。临时检查脚本已删除。

## 精简容器高亮相关条件（2026-10-21）

- 删除容器内容预览循环中的 `GridIndex != INDEX_NONE`：地面容器及物品内部库存只存放网格物品，不包含装备槽条目。
- `FInv_InventoryFastArray::AddEntry(ItemComponent, ...)` 将重复的 `GridAsset` 判断合并为一次，附近条目与真实拾取通过同一个类型分支区分；地面库存组件只查找一次。真实拾取复制内容后，仍对每个子物品更新归属，包含递归子容器。
- 保留会随实际状态变化的边界、目标容器类型、容器循环嵌套、同容器拖回以及归属改变判断。未新增函数、类或成员。
- 编译通过：`Saved/CodexContainerHoverCleanupBuild.log`。真实地面容器登记及含内容拾取五项检查通过，覆盖地面引用、内容保留、转移后索引和子物品归属；结果：`Saved/CodexContainerHoverCleanupCheckResult.json`。实际客户端交互未验证。
- 备份：`Saved/CodexContainerHoverCleanupBaseline/`。临时检查脚本已删除。

## 补充关键库存逻辑注释（2026-10-21）

- 在八份库存源码中新增 46 行中文注释，说明物品管理组件与直接父容器的区别、地面库存引用、FastArray 复制通知、拾取及掉落的容器内容交接、服务器装备和交换校验、客户端容器高亮、滚动条初始状态、装备网格绑定及失败后的 UI 重建。
- 对照修改前文本逐段核验，全部差异均为新增 `//` 注释行，无代码删除、替换或新增语句。本次未运行编译或行为测试。
- 备份：`Saved/CodexInventoryCommentsBaseline.json`；核验结果：`Saved/CodexInventoryCommentsVerifyResult.json`。

## 拾取装备时自动装备到空槽（2026-10-21）

- 在现有 `Server_PickupItem` 中，仅对角色顶层库存处理自动装备。根据装备片段的 `EquipTag` 查询当前已装备条目；对应槽空闲时，复用 `AddEntry(ItemComponent, 1, INDEX_NONE)` 创建装备条目、广播 `OnItemEquipped` 并移除地面拾取物。
- 空槽装备不占普通网格空间；槽已占用时不替换原装备，继续按主库存、背包、其他容器顺序存放。非装备物品和地面容器库存沿用原存放逻辑。
- 复用原容器拾取交接和装备通知，自动装备的背包、弹挂保留内部内容及格位；没有新增函数、类、RPC 或成员，也没有增加装备堆叠判断。
- C++ 编译通过：`Saved/CodexPickupAutoEquipBuild.log`。服务器八项检查通过，覆盖无法放入主网格时自动装备、占用槽不替换并转入容器、背包和弹挂独立装备、容器内容及索引保留、子物品管理归属、无处存放时不改变库存、消耗品存放、地面库存不自动装备。结果：`Saved/CodexPickupAutoEquipCheckResult.json`。实际装备外观、UI 和独立客户端网络未交互验证。
- 备份：`Saved/CodexPickupAutoEquipBaseline.cpp`。临时检查脚本已删除。

## 装备时网格适配资产高度、卸下恢复默认高度（2026-10-21）

- 用户将背包、弹挂外框设为默认固定高度。内部网格绑定时已有按资产行数及 TileSize 计算高度的逻辑，但外层固定高度使背景无法跟随。
- 在 `SpatialInventory` 中绑定现有 `SizeBox_0`（背包外框）、`SizeBox_8`（弹挂外框）。`EquipItem` 绑定内部库存后，清除对应外框的固定高度开关，让资产计算出的内部网格撑开背景；保留最小、最大高度及居中配置。
- `UnEquipItem` 重新启用外框原有 HeightOverride 值；`ClearHeightOverride` 只关闭开关，不修改蓝图默认数值，因此不增加默认高度缓存成员。`FinishDragPrediction` 先恢复默认外框，再由重建的装备适配高度。
- 主库存固定占位及用户蓝图配置保留；未新增函数、类或 Tick 检查，仅新增两个现有控件的绑定成员。
- C++ 编译通过：`Saved/CodexEquipGridHeightBuild.log`。独立进程检查主蓝图编译与绑定、两外框各三轮固定高度关闭及恢复、主库存固定占位，通过四项检查。结果：`Saved/CodexEquipGridHeightCheckResult.json`。未验证实际装备界面画面。
- 备份：`Saved/CodexEquipGridHeightBaseline/`。临时脚本已删除。

## 装备网格外框按装备名称命名（2026-10-21）

- 背包外框 `SizeBox_0` 改为 `SizeBox_BackPack`，弹挂外框 `SizeBox_8` 改为 `SizeBox_ChestRig`；同步更新 C++ BindWidget 成员、调用处和蓝图控件名及 GUID 映射，保留布局、默认高度与装备适配逻辑。
- C++ 编译通过：`Saved/CodexEquipmentBoxRenameBuild.log`。独立重新加载后蓝图编译及控件名称、默认高度检查通过，无错误；结果：`Saved/CodexEquipmentBoxRenameCheckResult.json`，日志：`Saved/CodexEquipmentBoxRenameVerify.log`。
- 备份：`Saved/CodexEquipmentBoxRenameBaseline/`。临时修改及核验脚本已删除。

## 多子网格运行时接通（2026-10-22）

- 按用户确认，子网格模板配置在 `InventoryGrid` 中；外层按资产生成整组，子网格复用现有单网格逻辑。仅新增已批准的四个辅助函数，无新增 C++ 类或 RPC 入口。
- 接通子网格编号复制、UI 通知分发、空间查询、拾取、转移交换、拆分、卸装、容器存入高亮及掉落交接；附近物品保持单网格扩行。
- 默认子模板：`/Inventory/Widget/Inventory/Survival_InventorySubGrid`，继续使用原自定义格子 UI。用户原布局资产不变，多子网格通过其 `Grids` 数组配置。
- 最终 C++ 编译、46 个库存蓝图编译和模板检查通过；14 项服务器数据检查及四项编辑器模拟 Widget 构建检查通过。未验证实际鼠标交互、画面和独立客户端复制。
- 配置、代码职责和验证范围见 `Docs/MultiSubGridInventory.md`；修改前备份 `Saved/CodexMultiSubGridBaseline/`。临时脚本已清理。
