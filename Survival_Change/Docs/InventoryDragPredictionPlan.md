# 库存拖拽 UI 客户端预测与服务端确认方案

保存日期：2026-10-16。

状态：仅保留修改建议，尚未实施。用户要求等后续再修改功能代码。

## 目标

客户端结束拖拽时立即显示移动、交换或合并结果，同时向服务器发送请求。服务器重新检查是否允许执行；允许时确认并校正显示，拒绝时撤销本地预测。

正常情况下，拒绝后物品回到拖拽前的位置。如果物品已被其他操作取走、消耗或移动，则按服务器最新状态恢复显示，避免从旧快照中重新显示已经不存在的物品。

## 当前流程

1. 客户端拖拽时计算红绿格预览。
2. 松开鼠标后发送服务器请求。
3. 服务器检查，通过后修改真实库存。
4. 客户端通过库存同步刷新 UI。

当前服务端 `MoveItemToThisInventory()` 返回 `void`，检查不通过时直接返回，缺少明确发给请求客户端的拒绝回执。

## 建议流程

1. 保留现有拖拽预览。
2. 松开鼠标时记录操作前的显示状态。
3. 客户端根据预览结果立即更新本地显示。
4. 发送带操作编号的服务器请求。
5. 服务器依据当前真实库存检查并执行。
6. 服务器回复操作编号、是否允许，以及最终位置和数量等校正数据。
7. 客户端确认或撤销预测，并与后续库存复制协调刷新。

## 修改建议

### 1. 复用客户端预览

保留 `UInv_InventoryGrid::CheckHoverPosition()` 的红绿格和交换落点计算。它已经能够提供是否有空位、是否允许放下、目标物品列表，以及交换物品返回来源库存的位置。

拖拽结束时根据这份结果预测显示，不要求客户端再次完整执行服务端占位扫描。

### 2. 预测只操作显示数据

本地预测可修改：

- 物品图标的位置。
- 网格显示占位。
- 堆叠数量文本。
- 完全合并后来源图标的隐藏状态。

真实 `InventoryList`、物品 `OwnerInventory`、`GridIndex` 和清单中的堆叠数量继续由服务器修改。

不要直接在客户端调用现有 `MoveItemToThisInventory()` 进行预测。该函数会修改真实库存并触发物品添加、移除等事件；附近物品库存的相关回调还涉及地面 Actor。

### 3. 保存操作前的显示状态

显示记录至少包含：

| 字段 | 含义 |
| --- | --- |
| Item | 对应物品 |
| InventoryGrid | 显示所在网格 |
| GridIndex | 显示位置 |
| StackCount | 显示数量 |

空位移动保存来源位置；交换保存所有参与物品的位置；合并还保存双方的显示数量。无需复制整个物品对象。

### 4. 增加服务器回执

建议让 `UInv_InventoryComponent::MoveItemToThisInventory()` 返回 `bool`，由玩家的 `UInv_RequestComponent` 将结果发送给请求客户端。

回执包含：

- 操作编号。
- 是否允许执行。
- 相关物品最终的位置和数量；必要时包含来源、目标库存的最新显示状态。

操作编号用于匹配本次预测，避免较早的回执处理到后续操作或重新打开的网格。

### 5. 协调回执与库存复制

服务器回执和库存复制可能先后到达。预测期间，现有 `OnItemChanged`、`OnItemRemoved` 等回调如果直接按尚未更新的复制数据重画，会出现图标跳回旧位置后再次跳到新位置的现象。

建议由预测处理统一接管本次操作涉及的显示，收到服务器结果后进行校正，等对应库存数据同步完成，再恢复常规刷新。具体同步完成判定方式留待实施时确定。

网格关闭、解绑或切换容器后，旧操作回执也需要与原来的网格绑定区分，避免刷新新打开的容器。

### 6. 第一版只保留一笔未确认操作

同一组网格在服务器确认前，暂时不接受下一次拖拽。物品放下时仍立即显示预测结果。

这样无需在第一版处理多次连续预测之间的依赖、重放和回退关系。

## 建议实施范围

第一版包含：

- 普通网格中的空位移动。
- 同库存交换。
- 跨库存交换。
- 堆叠合并及剩余数量显示。
- 上述操作的服务端拒绝与显示恢复。

装备、卸下装备、丢弃等其他操作不纳入第一版，后续按用户要求单独确定。

## 主要涉及的代码

以下路径相对于项目根目录：

| 文件 | 预计修改内容 |
| --- | --- |
| `Plugins/Inventory/Source/Inventory/Private/Widget/Inventory/GridSlot/Inv_GridSlot.cpp` | `NativeOnDrop()` 统一进入本地预测入口 |
| `Plugins/Inventory/Source/Inventory/Private/Widget/Inventory/SlottedItems/Inv_SlottedItem.cpp` | `NativeOnDrop()` 统一进入本地预测入口 |
| `Plugins/Inventory/Source/Inventory/Public/Widget/Inventory/Spatial/Inv_InventoryGrid.h` | 显示状态与预测相关接口 |
| `Plugins/Inventory/Source/Inventory/Private/Widget/Inventory/Spatial/Inv_InventoryGrid.cpp` | 保存、应用和撤销显示状态，协调预测期间的刷新 |
| `Plugins/Inventory/Source/Inventory/Public/InventoryManagement/Components/Inv_RequestComponent.h` | 操作编号、请求与回执接口 |
| `Plugins/Inventory/Source/Inventory/Private/InventoryManagement/Components/Inv_RequestComponent.cpp` | 本地预测调度、服务器请求与客户端回执处理 |
| `Plugins/Inventory/Source/Inventory/Public/InventoryManagement/Components/Inv_InventoryComponent.h` | 服务端执行结果返回值 |
| `Plugins/Inventory/Source/Inventory/Private/InventoryManagement/Components/Inv_InventoryComponent.cpp` | 保留服务端实际占位检查并返回执行结果 |

附近物品的网格移动也应进入相同的预测与回执入口，避免继续绕过该流程直接调用 `Server_MoveItem()`。

## 沿用用户已明确的设计前提

- 能够打开的物品库存允许取出其中物品，不重新增加容器访问资格或距离限制。
- 网格不会设计到超过 `MAX_int32`，不重新添加这类极限容量判断。
- 保留实际会影响移动结果的边界、占位冲突、交换落点和数量检查。
- 不添加对已确定配置、已由调用入口保证的参数的重复检查。
- 不保留新增测试文件；验证方式及临时文件清理在实施时处理。

## 后续验证重点

- 松开鼠标时，本地立即显示移动、交换或合并结果。
- 服务器接受时显示连续，不先跳回旧位置。
- 服务器拒绝时撤销预测，位置和数量恢复正确。
- 多格物品、多物品交换及部分、完全堆叠合并正确。
- 回执早于或晚于库存复制时均能正确刷新。
- 另一位玩家改变库存导致预览过期时，恢复服务器最新状态。
- 关闭、切换或重新打开容器后，旧回执不影响新的界面。
