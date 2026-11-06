# Glance Widgets 协议 V1 事实表

本文说明 Glance Widgets API V1 的请求、响应与渲染规则。

## 1. 请求与响应

Flutter 预览与原生小组件各自请求并解析原始 JSON。服务应按每次请求的 `gw_canvas_width_pt`、`gw_canvas_height_pt` 重新布局；设备与上下文的内容画布可能不同。

请求参数：`gw_protocol=1`、`gw_platform=ios`、`gw_family=small|medium|large`、画布宽高（pt）、`gw_aspect_ratio`、`gw_context`（`app_home|app_preview|widget`）。HTTPS，响应体上限 524,288 bytes。

secret 非空时附带 `X-GlanceWidgets-Timestamp`（毫秒 Unix 时间）与 `X-GlanceWidgets-Signature`：`Base64(HMAC-SHA256(utf8(secret), utf8(timestamp + "\n" + secret)))`。

测试向量：secret `this is secret`，timestamp `1700000000000`，签名 `sSFWELbV2YwjdDQhWZwTcWlX5BWUx5J6TPpsZmuPii0=`。

响应是 JSON 对象，包含根点阵、可选的 `data` 对象和 `layers` 数组。`protocol` 为数字 1（可省略或 null）。根点阵必填；SDK 在仅用图层时补一个空像素点。

```json
{
  "protocol": 1,
  "pixels": [[{"empty": true}]],
  "data": {
    "title": {
      "frame": {"left": 0.08, "top": 0.1, "width": 0.84, "height": 0.3},
      "content": {"text": "Power"},
      "style": {"color": "#FFFFFF", "fontSizeRatio": 0.12}
    }
  },
  "layers": [{"id": "remote:title", "type": "text", "dataPath": "data.title"}]
}
```

## 2. 图层声明与图层数据

`layers` 每项必须且只能有 `id`、`type`、`dataPath`。id 为非空的 `remote:` 前缀字符串，同一响应不得重复；type 为 `text|matrix|curve|image|icon|line|shape`。

路径从 `data` 开始，以点号标识符与非负数组索引访问，如 `data.title`、`data.cards[0]`；`data` 本身也可以。图层读取一个对象，不绑定标量或数组。

| 图层数据字段 | 含义 |
|---|---|
| frame | 整体定位；出现时必须有数字 width、height，可加 left/top/right/bottom |
| zIndex | 整数，越大越靠上，同层级按 id 排序 |
| rotation | 旋转角度，度 |
| transform | `{translateX?,translateY?}`，相对图层自身宽高的平移比例 |
| content | 类型专属内容对象 |
| style | 类型专属样式对象 |

远程图层没有保存配置，frame 必填；远程点阵还必须有 content.pixels。编辑器图层通过同样的对象绑定，可只提供需要更新的字段，无需加入 layers。

声明结构、id 或类型错误使整个响应无效。图层数据错误只影响该层：远程层不绘制，编辑器层回退保存配置。SDK 对已声明的远程节点主动报错，避免静默丢层。仅返回 data 时，SDK 不知道编辑器绑定的类型，不能代替 App 完整校验。

## 3. 编辑器图层的合并

每次响应从保存配置重新计算，响应不写回配置。对象逐字段合并，数组和标量整体替换，frame 整体替换；null 或省略沿用保存值，0、false、空数组都是有效覆盖。布局、内容、样式都能动态更新。

主屏、StandBy、首页组件库始终 API 优先。编辑页部分类型的“当前预览”只决定看保存值还是 API 值，是编辑器偏好而非协议字段。更新 content.icon 时，未同时提供的旧 iconCodePoint、iconFont 会被清除。

## 4. content 字段表

| 类型 | 允许字段 |
|---|---|
| text | text（字符串或有限数字） |
| image | url |
| icon | icon、iconCodePoint、iconFont，或 url；两种来源互斥 |
| matrix | pixels、default、batch、layout，与根点阵同构 |
| curve | mode、points、expression、parameters、domain、yDomain、interpolation、markers |
| line | direction（horizontal/vertical） |
| shape | 空对象 |

节点、content、style 的未知字段均为错误。content 不含来源选择器或嵌套数据路径。图片统一 content.url，可用 HTTPS、data URI 或裸 Base64。图标名称、码位、字体应配套；iconFont 为 material/phosphorBold/devicon/simpleIcons。

## 5. 根点阵与点阵图层

点阵是 `{pixels,default?,batch?,layout?}`。根点阵放响应顶层，图层点阵全部放节点 content，matrix 的 style 不接受字段。

- pixels[row][column] 为非空矩形二维数组，每格为像素对象或 null，总数最多 4,096。
- 像素字段：color、opacity、cornerRadius、rotation、halo、empty。颜色为 #RRGGBB 或 #AARRGGBB（alpha 在前）；SDK 接受简写输入并规范化。
- 默认种子：color #0A2D48、opacity 0.28、cornerRadius 0.18、rotation 0、halo 0。opacity/halo 限制 0–1，cornerRadius 0–0.5；empty:true 不画。
- 优先级 default → batch（按序）→ 显式 pixels。显式像素缺省字段回退 default，不叠加 batch。
- batch 项为 `{pixel,coordinates:[[row,column],...]}`，坐标须在网格内。App 截断数字坐标，SDK 输出整数。
- layout：gapMode=zero/near/default/far/extraFar，对应间隙比 0/.08/.16/.28/.42；edgeInsetRatio −0.45–0.45；cellAspectRatio 0.1–10；columnGapRatio/rowGapRatio 0–10。
- 编辑器节点可只返回 default/layout；batch 必须和 pixels 一起返回。数组整体替换。

根点阵亮像素（opacity > 0.4）有白色 9% 内描边，点阵图层没有。SDK 显式输出像素样式和布局，减少渲染默认值的差异。

## 6. style 与绘制约束

完整字段表见官网图层参考。

| 类型 | 字段 |
|---|---|
| text | color/textOpacity/fontSizeRatio/fontWeight/fontFamily/fontFamilyFallback/letterSpacing/baselineRatio/horizontalAlignment/verticalAlignment/autoWrap/paddingRatio，加通用装饰 |
| image、shape | color，加通用装饰 |
| icon | color/textOpacity/iconGlowEnabled/iconGlowColor/iconGlowOpacity/iconGlowRadius |
| line | color/opacity/strokeWidth/pattern/cap/dashLength/dashGap/gradientEnabled/gradientColors/gradientStops |
| curve | color/opacity/strokeWidth/lineCap/lineJoin/dashLength/dashGap/fillColor/fillOpacity/plotPadding |
| matrix | 空 |

通用装饰：backgroundColor/opacity/gradientEnabled/gradientColors/gradientStops/gradientAngle/borderColor/borderOpacity/borderWidth/cornerRadiusRatio/effectType/maskColor/maskOpacity/vignetteIntensity/readabilityGradient。effectType=none/mask/vignette/readability。

frame 的 left/width 相对内容画布宽，top/height 相对高；非双锚尺寸限制 0.02–1.5。shape 最小 24pt，细条用 line 或 matrix。fontSizeRatio 相对整个内容画布短边（0.02–0.28），paddingRatio 相对图层短边（0–0.3）。文本溢出时可能强制左上对齐，需预留宽度。

curve 支持 samples/function，表达式支持 + - * / ^、sin cos exp log abs sqrt min max、常量 pi；使用 e 时显式传参数。function 应提供 domain 和 yDomain。points 最多 1,024，markers 最多 32，parameters 最多 64，expression 最多 512 字符。marker 偏移 at.offsetPt 以 pt 计。curve 不应用图层 rotation。

line 描边以 frame 中线为中心，round cap 两端各延伸半个线宽。图片需在小组件快照环境验证，示例优先用小尺寸 Base64，仍受响应总大小限制。

所有 SDK 输出须为有限、可 JSON 序列化且对原生整数转换安全的数值。校验可以比 App 更早拒绝不明确的输出，但不能接受 App 当前字段表不允许的图层字段。
