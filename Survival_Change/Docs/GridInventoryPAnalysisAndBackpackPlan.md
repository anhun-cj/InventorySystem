**GridInventoryP 的容器、多子网格实现分析，以及当前项目的落地方案**

分析日期：2026-10-18。

分析范围：读取 `D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source`，使用无界面编辑器只读加载并导出 17 个相关蓝图、5 个库存布局资产。没有保存原插件资产，没有修改两个项目的业务代码，也没有执行实际双客户端玩法测试。目录中的既有说明文档不作为本次任务的指令来源。

**1. 核心结论**

这个插件将几个不同的问题分别处理：物品是 `UGridItem`；容器物品是 `UGridContainerItem`，额外持有一个 `UInventory`；一个 `UInventory` 包含多个 `FInventorySlot`，每个 Slot 是一个独立矩形区域；布局 UI 根据区域数量创建多个网格 UI；装备面板与独立窗口复用同一个库存对象。

多子网格描述同一个容器的多个口袋；嵌套描述一个物品内部拥有另一个容器。这两套关系可以组合，但不能只靠增加几个 UI 网格同时解决。

对当前项目的建议：先沿用 `UInv_InventoryComponent` 作为一个完整容器的数据与复制载体，一个组件管理多个子网格。复用现有 FastArray、布局计算、请求组件、客户端 UI 预览和服务器转移逻辑。第一轮不把库存组件整体拆成新的 UObject，不给每个口袋创建库存组件。

**2. 源插件的数据分层**

| 层级 | 类型 | 职责 |
|---|---|---|
| 物品实例 | `UGridItem` | 名称、标签、尺寸、堆叠、旋转、操作、当前逻辑拥有者 |
| 容器物品 | `UGridContainerItem` | 在普通物品基础上持有内部库存及布局资产 |
| 库存实例 | `UInventory` | 管理子网格、存取、查找、堆叠与数据变化事件 |
| 子网格 | `FInventorySlot` | 管理一个矩形区域的占格和物品位置 |
| 布局资产 | `UInventoryAsset` | 定义子网格尺寸、显示偏移、格子大小与授予的操作 |
| Actor 入口 | `UInventoryComponent` | 创建和持有库存实例，添加初始物品 |
| 布局 UI | `UInventoryLayoutWidget` | 组合一个库存的所有子网格 UI |
| 子网格 UI | `UInventoryWidget` | 显示指定库存的指定 Slot |

源插件的 `UInventoryComponent` 不直接承载全部库存算法；它创建 `UInventory`。容器物品也能直接创建 `UInventory`，所以包内库存无需再挂一个 ActorComponent。

参考：[Inventory.h](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Public/Inventory.h:32>)、[InventoryComponent.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/InventoryComponent.cpp:23>)。

**3. 一个库存中的多子网格**

`FInventorySlot` 保存：

- `SlotDimension`：本区域有几列、几行。
- `PositionOffset`：本区域在整个面板中的显示偏移，单位是 UI 坐标。
- `SlotGridCells`：本区域的占格数组。
- `ItemToTopLeftTileList`：物品与本区域内左上角坐标的对应关系。

占格数组使用 `Y * SlotDimension.X + X` 访问。一个占 2×3 格的物品，会让这 6 个格子引用同一个物品对象；左上角列表只登记一次该物品。

`UInventory` 的 `InventorySlots` 数组把多个区域组合成一个容器。物品的位置由“库存实例 + Slot 编号 + Slot 内坐标”共同确定。

各 Slot 独立处理边界与占用。物品不能跨过两个 Slot 之间的缝隙，即使视觉上两块区域挨得很近。所有 Slot 使用同一套库存事件，事件中的 Slot 编号负责区分目标子网格。

实际资产配置：

| 资产 | 子网格配置 | 总格数 |
|---|---|---:|
| `IA_GunSlinger` | 3×5、2×7、3×2 | 35 |
| `IA_AVS` | 5 个 1×2、5 个 1×1、2 个 2×2 | 23 |
| `IA_Pockets` | 2×2、2×2、1×2 | 10 |
| `IA_Stash` | 10×78 | 780 |

示例背包三个区域的显示偏移分别为 `(0,0)`、`(160,0)`、`(0,260)`。默认格子大小为 50，因此这些偏移表达了格子宽高及区域间距。不是把一个大网格遮掉几块，也不是三个不同的库存。

胸挂 `BP_AvsRig` 和背包 `BP_GunslingerBackpack` 在外部库存中都占 3×4 格；内部布局则分别来自不同资产。外部占格与内部容量完全独立。

参考：[InventoryAsset.h](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Public/InventoryAsset.h:48>)、[IA_GunSlinger.uasset](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Content/Assets/IA_GunSlinger.uasset>)、[IA_AVS.uasset](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Content/Assets/IA_AVS.uasset>)。

**4. 多子网格的查找与放置**

`UInventory::TryAddItem()` 按 Slot 数组顺序查找可放区域，成功后记录物品拥有者、格子显示尺寸并广播 `OnItemAdded(Item, TopLeftTile, SlotIndex)`。原方向都放不下时，再尝试旋转物品，失败后恢复旋转。

`CanAddItem()` 做类似的容量查询。它只查当前库存直接包含的 Slot，不会递归搜索每一个内层背包。

指定落点使用 `CanFitItem(Item, SlotIndex, StartingTile, ...)`。指定放置的 `AddItemInSlot()` 假定调用方已经验证落点，直接写入占格并广播事件。

这体现了可复用的分工：自动放入查多个区域；拖拽落点明确指定区域；UI 组合位置与内部占格计算分开。

参考：[Inventory.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Inventory.cpp:250>)。

**5. 容器物品如何支持套包**

`UGridContainerItem::InitItem()` 中创建 `NewObject<UInventory>(this)`，用 `ItemInventoryAsset` 初始化。内部库存以容器物品为 Outer。

因此结构可以自然递归：

```text
背包 A：UGridContainerItem
└── A.Inventory：UInventory
    ├── Slot 0
    └── Slot 1
        └── 背包 B：UGridContainerItem
            └── B.Inventory：UInventory
                ├── Slot 0
                └── Slot 1
```

把 B 从一个外部库存搬到另一个外部库存时，搬动的是 B 对象。B 持有的内部库存对象不重建，内部 Slot 和物品坐标自然保留。

容器还重写了 `CanInteractWithItem()` 与 `TryInteractWithItem()`：物品拖到背包图标上时，可以查询背包内部容量，再通过原拥有者接口移除物品、将其加入内部库存。这和打开背包后拖向内部格子是两个入口。

蓝图 `WBP_GridItem::OnDrop` 实际接入了这两个交互函数。它对堆叠等操作有分流，因此不能把所有图标 OnDrop 都等同于普通落格操作。

参考：[GridItem.h](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Public/GridItem.h:184>)、[GridItem.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/GridItem.cpp:273>)。

**6. UI 为什么能复用**

`UInventoryLayoutWidget::InitLayout()` 清空布局后，遍历库存布局中的所有 Slot。每个 Slot 创建一个 `UInventoryWidget`，传入同一个 `UInventory*` 和不同的 SlotIndex，再按 `PositionOffset` 放入 Overlay。

每个 `UInventoryWidget` 根据对应 Slot 的尺寸调整大小，绘制网格，遍历该 Slot 的已有物品生成图标，再绑定库存增删事件。

添加事件带 SlotIndex。网格只处理与自身编号相同的添加；移除事件按物品对象身份移除对应图标。

因此口袋、胸挂、背包、仓库、多窗口都可以使用相同的布局类和网格类，只是传入库存对象不同。

参考：[InventoryLayoutWidget.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Widgets/InventoryLayoutWidget.cpp:11>)、[InventoryWidget.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Widgets/InventoryWidget.cpp:17>)。

**7. 装备、打开、丢弃的实际路径**

装备面板的背包回调取得 `UGridContainerItem::GetInventory()`，调用 `AddInventoryToBackpackSlot()`。该函数创建布局 UI，绑定原库存，放入背包 NamedSlot，并将库存登记到 `EquippedInventories`。卸下时清理 UI，并移除这份可用库存登记。

自动拾取的示例先尝试胸挂装备槽，再尝试背包装备槽，随后依次尝试 `EquippedInventories` 中的库存。不会由这个循环自动搜索所有嵌套背包。

双击开包由 `BP_OpenContainerAction` 完成：取得容器库存，调用窗口布局的 `ShowInventoryWindow()`。窗口系统以库存对象为标识；同一个库存再次打开时聚焦现有窗口。默认最大活动窗口数为 4，这是窗口数限制，不是网格行数限制。

`BP_DropAction` 先请求角色创建掉落表现，再调用物品拥有者接口移除来源。`AGridItemDropActor::SetItem()` 保存原来的 `UGridItem*`，没有从名称或类型重新构造一个空物品。掉落 Actor 的拾取蓝图把原物品交回角色，再销毁地面表现。

已检查的 `BP_GridItemDropActor`、`BP_GridItemPickupActor` 地面交互走拾取路径。地面按 F 直接打开背包不是这两条蓝图现成提供的行为，需要另外接入容器打开入口；仓库示例 `BP_Stash` 提供了交互打开布局的参考。

参考：[PlayerInventoryEquipWidget.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Character/PlayerInventoryEquipWidget.cpp:36>)、[GridWindowsLayoutWidget.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Widgets/DraggableWindow/GridWindowsLayoutWidget.cpp:26>)、[GridItemDropActor.cpp](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Private/Actors/GridItemDropActor.cpp:53>)。

**8. 拖拽和拥有者接口**

`UGridPayload` 携带物品、来源库存、来源 Slot 编号、来源图标、初始旋转和是否拆分。落点 UI 明确知道目标库存与目标 Slot。

`IGridItemOwner` 提供统一移除命令。库存对象和装备槽 UI 都能实现它，所以拖拽目标可以要求原拥有者移除来源，而无需判断每一种来源 UI 类型。

这适合借鉴其职责划分，但当前项目不能让装备 UI 成为服务端权威的物品拥有者。你已有的 `OwnerInventory`、请求组件和服务器转移继续承担数据归属；各自 UI 继续负责本地预览。

源插件蓝图中的普通网格 OnDrop 主要处理落格、堆叠、拆分和交互。不能把它视作已经完整实现了你当前服务器的多物品交换、回弹和连续拖拽协议。

参考：[GridPayload.h](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Public/Data/GridPayload.h:17>)、[GridItemOwner.h](<D:/Unreal Projects/ProjectXHG/Plugins/GridInventoryP/Source/GridInventory/Public/Interfaces/GridItemOwner.h:26>)。

**9. 能借鉴的设计与不能直接照搬的部分**

| 内容 | 本次检查结果 | 当前项目处理方式 |
|---|---|---|
| 容器内部库存 | 容器物品持有独立库存对象 | 保留每个背包独立库存的关系 |
| 多口袋 | 一个库存管理多个 Slot | 一个组件管理多个子网格 |
| 网格 UI | 一个布局创建多个区域网格 | 复用现有 `UInv_InventoryGrid` |
| 装备后显示 | 展示该物品原库存 | 装备回调只绑定布局 |
| 丢弃保留内容 | 掉落 Actor 持有原物品对象 | 背包实例不能按纯清单重建 |
| 联机同步 | 未在已检查的原生源码和核心蓝图流程中发现完整权威复制链路 | 保留并扩展当前 RPC、FastArray、子对象复制 |
| 存档 | 未发现独立存档序列化实现 | 本局内容保留与退出后恢复分别设计 |
| 防循环套包 | 只有部分入口检查自身，未见完整祖先检查 | 服务端统一检查自包含和祖先循环 |
| 无限扩行 | 参考资产使用明确的有限 Slot 尺寸 | 按当前需求保留扩行，布局需要随高度调整 |

需要特别注意的源码细节：

- `UInventory::CanFitItem()` 用 `GetOuter() == InItem` 阻止部分自身放置，但没有检查祖先链，且 `TryAddItem()/CanAddItem()` 没有统一经过该判断。图标交互入口另有自身比较，也不能覆盖完整循环关系。
- `UGridContainerItem::InitItem()` 每次都会新建内部库存。因此拾取、开 UI、装备回调不能反复用初始化函数替代绑定函数。
- `TryInteractWithItem()` 先移除来源，再调用内部 `TryAddItem()`，没有利用最终返回值恢复来源。当前服务器交换应继续采用完整校验后提交的方式。
- `FInventorySlot::CanFitItem(const FIntPoint&)` 这个重载最终始终返回 false。主要实际路径使用另一重载，迁移时不应直接复制整个文件。
- 运行时位置列表没有作为完整 SaveGame 格式实现；“关闭窗口重开仍在”不等于退出游戏也能恢复。

这些是已读代码的边界和静态发现，不代表已经执行了所有游戏场景并复现了每项问题。

**10. 当前项目建议的数据结构**

保持一个 `UInv_InventoryComponent` 表示一个完整容器。一个背包、一个仓库分别拥有自己的组件；一个背包的多个口袋不再各建组件。

```text
UInv_InventoryComponent：背包 A
├── InventoryList：一份 FastArray
│   ├── 药水：SubGridIndex=0, GridIndex=0
│   ├── 材料：SubGridIndex=1, GridIndex=3
│   └── 背包 B：SubGridIndex=2, GridIndex=0
├── 子网格布局/绑定信息
└── ParentInventory：用于定位容器父级

背包 B 物品
└── ContainerInventory → 背包 B 的库存组件
```

新增的核心字段建议：

| 所在位置 | 字段 | 用途 |
|---|---|---|
| `FInv_InventoryEntry` | `SubGridIndex` | 复制物品属于哪个口袋 |
| `UInv_InventoryGrid` | `SubGridIndex` | 标明当前网格显示哪个口袋 |
| `UInv_InventoryItem` | `ContainerInventory` | 背包内部库存引用，与外部 `OwnerInventory` 分开 |
| 背包库存组件 | `ParentInventory` | 表达该容器当前收纳在哪个父库存，用于服务器循环检查 |
| 库存组件 | 按子网格登记的 UI 引用 | 替换只记录一个 `BoundInventoryGrid` 的假设 |

条目位置定义为：`InventoryComponent + SubGridIndex + GridIndex`。其中 `GridIndex` 是对应子网格内的索引，使用该子网格列数换算；不把所有口袋拼接成一个大索引空间。

现有 `ItemIndex` 用于库存条目索引，不能拿来兼任子网格编号。子网格编号是独立的位置维度。

装备条目继续使用现有的 `GridIndex == INDEX_NONE` 语义，子网格编号也可以用 `INDEX_NONE` 表示装备区域。装备槽仍只做装备槽匹配，内部背包容量不参与槽位匹配。

复制回调继续以 FastArray 条目的位置快照判断 UI 归属，不在回调里改回依赖可能尚未同步的 Item 位置。

**11. 当前项目建议的 UI 结构**

建议只新增一个库存布局 UI 类，例如 `UInv_InventoryLayout`，用于登记、创建或组织多个现有 `UInv_InventoryGrid`。不同背包使用不同布局蓝图。

```text
Inv_SpatialInventory
├── 角色基础库存布局
├── 附近物品网格
├── 装备背包布局
│   ├── 子网格 0
│   ├── 子网格 1
│   └── 子网格 2
└── 当前打开容器布局
    └── 根据被打开容器生成对应子网格
```

`OpenInventory` 当前是单个网格，应该改成承载布局的区域。仓库、地面背包、内层背包都交给同一个打开容器入口；变化的是库存组件和布局样式。

子网格尺寸继续从实际绑定的 UI 获取并上传，不恢复网格模板查找。布局蓝图负责保证同一个背包从不同入口打开时使用相同子网格编号和列数。布局里登记子网格数组，避免另外维护一份互相冲突的尺寸配置。

如果采用参考插件那种自由 XY 排列，再允许无限扩行，就必须重算后续区域的位置，否则 UI 本身会重叠。第一版更建议用能随内容尺寸排布的容器，或者在扩行后由布局统一重新排列，不给网格添加新的最大行限制。

每个子网格的添加回调只显示属于自己的条目。物品从同组件子网格 0 移到子网格 1 时，旧网格先按物品身份移除图标，新网格根据最新条目创建图标。切换布局需要解除旧事件绑定。

`FinishDragPrediction()` 遍历当前实际显示的所有布局/网格刷新，覆盖装备包、打开容器和内层窗口，不能继续只固定刷新原来的三个网格。拖拽来源/目标引用仍作为本次调用的局部参数，不恢复之前删除的长期成员。

**12. 服务器转移算法需要修改的准确范围**

现有 `TransferItem()` 已有来源组件、目标组件、来源索引、目标索引和来源/目标网格尺寸。在此基础上增加来源子网格编号、目标子网格编号。

处理顺序：

1. 找到物品在来源组件中的权威条目，核对来源子网格和来源索引。
2. 只取目标子网格的物品构建占用数据，使用这次上传的目标子网格尺寸。
3. 复用现有落点、合并、交换计算；涉及交换回来源时，使用来源子网格的尺寸和条目。
4. 全部检查通过后再提交条目变化，更新组件归属、子网格编号和局部索引。
5. 拒绝时沿用现有客户端刷新，不重新加入操作 ID、等待锁或拖拽队列。

多子网格下，冲突判断的分组键需要从“库存组件”改为“库存组件 + 子网格编号”。子网格 0 与 1 各有索引 0 是正常情况。

同组件跨子网格移动，仍需要标记条目发生变化。自动查找空间可以按预定顺序遍历这个容器的子网格，返回明确的 `SubGridIndex + GridIndex`；不要默认递归查所有内层包。

自动堆叠的范围要单独决定。可以在一个容器的多个口袋中找同类堆叠，但几何落点与占用始终按具体子网格计算。

装备旧物品返回普通库存时，自动找位结果也要包含子网格编号。装备槽不检查背包内部网格大小，只有卸下的物品需要在普通网格中找合法位置。

**13. 套包、拾取、掉落、复制与附近列表**

第一轮为少改现有结构，背包 Actor 保留为库存组件的稳定载体。地面时显示并允许交互；收纳、装备或装进另一背包时关闭地面表现。套包不要求把容器 Actor 挂成父子 Actor。装备外观可以独立生成、销毁。

这是一项有意选择的生命周期方案：它继续使用现有 ActorComponent 复制，不宣称已经和参考插件一样完全脱离 Actor。大量隐藏容器的成本需要后续按实际规模评估；要完全按参考插件把内部库存改成 UObject，需要另做数据层和复制层的重构。

拾取和丢弃不能继续对背包完全套用“根据清单建新物品，销毁旧拾取 Actor”的普通流程。背包的内部库存引用必须贯穿按键拾取、附近列表拖拽、装备、普通转移和掉落。

当前附近列表会为地面物品创建映射条目。不同玩家看到的列表条目可以是不同代理，但必须引用同一个权威背包容器；不能为每个代理创建一份内部库存。真实的地面/携带/父库存状态在服务端统一维护，避免多个玩家通过各自旧列表同时领取同一个背包。

容器的父级关系在背包转移、装备、掉落时一起更新。循环检查从目标库存向父级遍历，如果遇到被移动背包的内部库存，拒绝操作。该规则覆盖自身与间接循环，并放在公共服务器转移入口。

复制需要覆盖背包内部库存组件、物品子对象和内部库存引用。客户端收到容器引用不代表内容一定已经到齐；打开 UI 绑定已到达数据，后续条目复制事件继续补齐。不能只复制外层背包引用，也不能用 UI 的创建作为服务器创建库存的时机。

携带者变化、外层背包掉落、角色离开或销毁时，要维护整棵容器树的有效生命周期和网络可见性。只隐藏模型不能替代复制状态管理。请求仍由玩家拥有的 `UInv_RequestComponent` 发起。

如果以后希望退出游戏仍然保留，则还需要存档：容器/物品实例标识、所属容器、子网格编号、局部索引、数量以及运行时属性。当前方案不把本局持续存在等同于已经完成持久化。

**14. 建议的实施步骤与验证场景**

| 步骤 | 修改目标 | 验证重点 |
|---|---|---|
| 1 | 一个组件支持多个子网格；增加条目子网格编号 | 不同口袋相同局部索引互不冲突；同库存跨口袋交换正常 |
| 2 | 新增布局 UI，复用现有网格 | 三口袋布局显示正确；刷新、切换、关闭没有旧绑定 |
| 3 | 背包物品引用独立内部库存；接通地面打开 | 存入后关闭重开，子网格与位置一致 |
| 4 | 接通带内容拾取、装备、掉落和附近列表 | 包内不重建；不同玩家不能同时领取同一包 |
| 5 | 接通内层容器打开与父级关系 | A 装 B、B 装 C 正常；把 A 放进 C 被拒绝 |
| 6 | 扩展全部显示网格的拒绝刷新与联机验证 | 连续拖拽、他人操作、服务器拒绝后所有区域恢复 |

具体场景建议覆盖：外部背包占 3×4 但内部有三块不同区域；各区域尺寸和列数不同；不同尺寸物品换装返回普通库存；两个口袋均有 GridIndex 0 的物品；背包内部放另一背包再掉落外层；直接拾取与附近网格拾取；两客户端同时打开或拾取；打开内层包时外层被移动；自动扩行后布局不重叠。

计划新增的类型先限定为一个布局 UI 类。新的子网格配置结构只在布局数据确有需要时增加；不先引入新的库存 UObject、全局库存管理器、网络版本号或拖拽事务类。实施前再列出准确的新函数、参数和成员，由用户确认后编码。
