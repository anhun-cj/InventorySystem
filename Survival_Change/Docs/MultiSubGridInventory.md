# 多子网格库存

实现日期：2026-10-22。

## 配置和生成

- 外层继续使用 `/Inventory/Widget/Inventory/Survival_InventoryGrid`，其 `SubGridClass` 已配置为 `/Inventory/Widget/Inventory/Survival_InventorySubGrid`。子模板保留原有格子、物品和悬浮物模板，继续使用用户的网格槽 UI。
- 在角色、背包、弹挂或仓库的 `GridAsset` 中配置 `Grids` 数组。每项的 `Col`、`Row` 决定单个子网格尺寸，`PositionOffset` 决定其在整组布局中的左上角位置。
- 外层 `InventoryGrid` 遍历数组生成子网格。每个子网格绑定指定编号，只生成自己的格子，不再生成下一层子网格。
- 坐标直接使用资产中的 `PositionOffset`，不再乘格子尺寸、不重复添加间距。宽高分别为 `Col * TileSize`、`Row * TileSize`。
- 整组宽高取所有子网格右边和下边的最大值，外层统一滚动和居中。各子网格自身完整显示，不单独滚动。装备网格外框的默认高度和卸装恢复方式保留。
- 附近物品继续单网格和原有自动扩行，不生成子网格。原布局资产仍保留用户配置，没有保存测试用的多子网格资产或关卡。

## 物品位置

- `InventoryItem` 和 `FInv_InventoryEntry` 同时保存 `SubGridIndex`、`GridIndex`。前者是库存中的子网格编号，后者是该子网格内部的局部格位。
- 库存仍只有原来一份 FastArray。内部物品通过 `ParentContainerItem` 表达直接容器，通过 `OwnerInventory` 表达管理和复制组件。
- 旧的单网格条目默认属于子网格 0。装备槽仍以 `GridIndex == INDEX_NONE` 表示。
- 拾取、移动、交换、拆分和卸装写入对应子网格编号；丢弃和再次拾取保留内容条目的子网格编号及格位。
- 当前使用资产数组下标作为编号，已有内容的布局不能仅靠重排数组来迁移物品；本次未实现运行时布局迁移。

## UI 和服务器

- 外层只绑定一次库存通知，物品变动时移除旧显示，再按条目的子网格编号分发到对应子网格。
- 每个子网格复用原有鼠标落点、格子高亮、物品 UI、拖拽和交换预览。
- 服务器请求带来源及目标子网格编号，校验来源编号和格位，按目标资产检查边界。不同子网格中相同局部格位不视为重叠。
- `QueryInventory` 先补整个库存内已有堆叠，再按资产顺序寻找各子网格空位。物品不能横跨两个子网格。
- 自动卸装和拆分通过整组库存寻找空位，再在选中的实际子网格中显示预览。
- 拖到容器图标时查询该容器全部子网格；地面容器继续读取真实地面库存。
- 服务器拒绝时继续复用原 UI 重建逻辑。客户端可以沿用连续拖拽，服务器核对请求中的原始位置。

## 新增函数范围

仅新增用户已同意的四个辅助函数：

1. `UInv_InventoryItem::GetSubGridIndex`。
2. `UInv_InventoryItem::SetSubGridIndex`。
3. `UInv_InventoryGrid::GetSubGrid`：编号取实际子网格，`INDEX_NONE` 取外层整组；附近单网格返回自身。
4. `FInv_GridLayout::QueryInventory`。

未新增 C++ 类或 RPC 入口；现有函数和 RPC 补充子网格参数。新增一个复用现有 C++ 类的子网格蓝图模板。

## 验证

- C++ 编译通过：`Saved/CodexMultiSubGridBuild.log`。
- 46 个库存插件蓝图编译及模板引用检查通过：`Saved/CodexMultiSubGridAssetsVerify.log`。
- 14 项服务器数据流检查通过：`Saved/CodexMultiSubGridCheckResult.json`。覆盖自动拾取、跨子网格移动与交换、旧来源和越界拒绝、优先合并堆叠、拆分、容器自动存入、卸装、禁止套入自身、丢弃和再次拾取。
- 在编辑器模拟环境中实际构建三块不同尺寸、不同偏移的子网格，验证列行数、Canvas 坐标、尺寸和不递归生成；同时检查附近库存仍为单网格。四项结果：`Saved/CodexMultiSubGridUICheckResult.json`。
- 没有进行实际鼠标拖拽、画面截图或独立客户端网络复制验证。
- 修改前源码和相关资产备份：`Saved/CodexMultiSubGridBaseline/`。临时修改、验证脚本已删除。
