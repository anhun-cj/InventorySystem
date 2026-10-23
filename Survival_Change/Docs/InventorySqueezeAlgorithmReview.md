# 库存挤压算法检查与待修改记录

记录日期：2026-10-19。

状态：用户于 2026-10-19 确认修改，已在 `CheckHoverPosition()` 中改为直接检查二维落点范围。挤压公式保持不变。

## 核心算法检查结果

检查文件：`Plugins/Inventory/Source/Inventory/Private/Widget/Inventory/Spatial/Inv_InventoryGrid.cpp`。

涉及 `CheckHoverPosition()`、`CheckHoverItemMoveItems()`、`CalculateMoveItemPosition()`。

当前算法依赖以下前提：

- 拖拽物品原来完整位于来源网格内。
- 原有物品之间没有重叠。
- 新落点完整位于目标网格内。
- 被挤物品完整包含在拖拽物品的新落点区域内，当前由 `CheckHoverItemMoveItems()` 判断。
- 物品计算尺寸与 UI 占用尺寸一致；本次检查针对当前单矩形网格，不涉及未来多子网格的间隙和索引设计。

在这些前提下，未发现当前挤压公式造成越界或重叠的反例：

- 跨库存：按相对位置平移到来源物品腾出的区域。
- 同库存且原位置与新位置不相交：平移到原区域。
- 同库存且同行或同列移动、区域相交：沿移动轴偏移原物品的完整宽度或高度。
- 同库存且斜向移动、区域相交：围绕重叠区域中心做中心对称，将新区域映射到原区域。
- 同一次平移或中心对称保留被挤物品之间的分离关系。

数值枚举验证：将当前同库存公式转写为临时 JavaScript，在 6×6 网格中枚举拖拽物品宽、高分别为 1～4 的合法位置和符合前提的被挤物品。

- 单物品情况：54,720 个，未发现移出原区域或与拖拽物品新区域相交。
- 原本互不重叠的被挤物品对：211,944 对，未发现移动后互相重叠。

验证范围为公式转写的数值检查和源码审查，不是实际 UE 拖拽、网络同步或任意尺寸穷举测试。没有新增测试源文件。

## 已修复：二维落点坐标在转为索引前缺少右侧边界检查

位置：`UInv_InventoryGrid::CheckHoverPosition()` 当前开头的边界判断。

修复前先检查坐标非负，再将坐标转换为索引，交给 `IsInGridBounds()` 检查。超出右边界的 X 坐标可能在转换后被解释为下一行的合法位置。

具体例子：8 列、至少 2 行，拖动 1×1 物品，计算得到 `Position = (8, 0)`。

1. `GetIndexFromPosition()` 得到索引 8。
2. `IsInGridBounds()` 按索引 8 计算，解释成坐标 `(0, 1)`，因此可能通过检查。
3. 随后的 `ForEach2D()` 仍使用 `(8, 0)`，因 X 超出列数而跳过格子。
4. `Result.bHasSpace` 和 `Result.bAllowItemDown` 可能保留为 true。
5. DropIndex 仍为 8，最终请求指向第二行第一格。服务器按实际占用决定接受或拒绝，可能表现为放到意料之外的位置或回弹。

入口依据：`UInv_WidgetUtils::IsWithinBounds()` 当前将右边界算在内部；鼠标恰好落在右边界时，`CalculateHoveredCoordinates()` 对 1×1 物品可能算出 X 等于 Col。

## 实际修改及验证

已在 `CheckHoverPosition()` 中直接检查二维矩形是否在当前网格内：

```cpp
Position.X >= 0 && Position.Y >= 0
    && Position.X + Dimensions.X <= Col
    && Position.Y + Dimensions.Y <= Row
```

实际修改保留原有无效落点处理分支，将两个允许标志置为 false。未改动 `CalculateMoveItemPosition()`、`ForEach2D()` 或服务器校验，未恢复此前删除的客户端重复重叠检查。

数值验证：网格行列分别取 1～10，物品宽高分别取 1～12，落点覆盖网格内外各边界，将修复后的条件与逐格检查矩形占用范围的独立判断比较。共 1,587,600 个组合，无差异。包括右边界上的 1×1、合法最右列、底边界、多格物品贴边及物品大于网格的情况。没有新增测试源文件。

C++ 编译通过，日志为 `Saved/CodexInventoryDropBoundsBuild.log`。

该入口由同库存和跨库存拖拽共用。实际 UE 游戏中的拖拽、换装和网络同步效果尚未进行交互验证。
