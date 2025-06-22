# 项目文档：图像转ASCII艺术生成器

本文档提供了对该 C++ 项目的全面概述，包括其使用的开源库、项目文件结构以及 `config.json` 配置文件的详细解释。

---

## 1. 使用的开源库

本项目依赖于以下几个优秀的开源库来完成其核心功能：

* ### **nlohmann/json**
    * **用途**：用于解析 `config.json` 配置文件。这是一个仅包含头文件的现代 C++ JSON 库，非常易于集成和使用。它负责将 JSON 文件中的设置读取到程序内的 `Config` 结构体中。
    * **GitHub**: <https://github.com/nlohmann/json>

* ### **stb_image.h**
    * **用途**：用于加载各种格式的图像文件（如 PNG, JPG, BMP 等）。这是一个单头文件、公共领域的图像加载库，以其简单和便携而闻名。程序使用它来读取用户输入的图片并获取像素数据。
    * **GitHub**: <https://github.com/nothings/stb/blob/master/stb_image.h>

* ### **stb_truetype.h**
    * **用途**：用于渲染 TrueType (`.ttf`) 字体。当程序将 ASCII 字符画转换回 PNG 图像时，它使用此库来获取每个字符的字形（glyph）数据，并将其绘制到画布上。
    * **GitHub**: <https://github.com/nothings/stb/blob/master/stb_truetype.h>

* ### **stb_image_write.h**
    * **用途**：用于将内存中的像素数据保存为 PNG 图像文件。这是 `stb` 库套件的一部分，负责将最终渲染出的 ASCII 字符画图像写入磁盘。
    * **GitHub**: <https://github.com/nothings/stb/blob/master/stb_image_write.h>

---

## 2. 项目文件结构

项目采用了模块化的代码结构，将不同的功能分离到各自的文件中，使得代码更易于维护和理解。

```
.
├── CMakeLists.txt              # CMake 构建脚本，定义项目、依赖和编译规则
├── config.json                 # JSON 配置文件，用于控制程序的各种行为
├── bash.sh                      # (或 build.sh) 自动化编译脚本，用于 MSYS2/MinGW 环境
│
├── src/                         # 源码
    ├──main.cpp                 # 程序主入口，处理用户输入、调用核心流程
    │
    ├──config_handler.h        # 声明配置处理函数 (加载/写入)
    ├── onfig_handler.cpp      # 实现配置处理逻辑，使用 nlohmann/json 解析 JSON
    │
    ├──image_converter.h      # 声明图像到 ASCII 的转换逻辑
    ├──image_converter.cpp    # 实现核心转换算法，使用 stb_image
    │
    ├──ascii_renderer.h        # 声明 ASCII 到图像/HTML 的渲染逻辑
    ├── ascii_renderer.cpp      # 实现渲染逻辑，使用 stb_truetype 和 stb_image_write
    │
    └──common_types.h          # 定义共享数据结构 (Config, ColorScheme) 和通用工具函数
    │
    ├── stb_image.h        
    ├── stb_truetype.h      
    └── stb_image_write.h   
```

---

## 3. `config.json` 配置文件详解

`config.json` 文件允许用户在不重新编译程序的情况下，灵活地调整生成效果。

```json
{
    "Settings": {
        "targetWidth": 512,
        "charAspectRatioCorrection": 2.0,
        "fontFilename": "Consolas.ttf",
        "fontSize": 12.0,
        "colorSchemes": [
            "BlackOnWhite",
            "ColorOnWhite",
            "GreenOnBlack",
            "SolarizedDark"
        ],
        "generateHtmlOutput": true,
        "htmlFontSizePt": 8.0,
        "enableTiledRendering": false,
        "tileSize": 512,
        "outputPngExtension": ".png",
        "imageOutputSubDirSuffix": "_ascii_output",
        "batchOutputSubDirSuffix": "_ascii_batch_output",
        "outputHtmlExtension": ".html"
    }
}
```

### 参数说明

* `"targetWidth"`: `(整数)`
    * **描述**: 生成的 ASCII 艺术的目标宽度（以字符为单位）。这是影响细节的最重要参数。
    * **效果**: 值越大，细节越丰富，但生成的图像文件也越大。

* `"charAspectRatioCorrection"`: `(浮点数)`
    * **描述**: 字符宽高比校正因子。大多数等宽字体的字符都是高大于宽的。
    * **效果**: `2.0` 意味着假设字符的高度是宽度的两倍。您可以根据所使用字体的实际显示效果来微调此值，以获得正确的图像比例。

* `"fontFilename"`: `(字符串)`
    * **描述**: 用于渲染输出 PNG 和 HTML 的字体文件名。
    * **要求**: 必须是 TrueType (`.ttf`) 或 OpenType (`.otf`) 字体文件，并放置在与可执行文件相同的目录中。

* `"fontSize"`: `(浮点数)`
    * **描述**: 渲染 **PNG** 图像时使用的字体大小，单位是**像素**。

* `"colorSchemes"`: `(字符串数组)`
    * **描述**: 一个列表，定义了程序需要为每张输入图片生成哪些颜色方案。
    * **有效值**: `AmberOnBlack`, `BlackOnYellow`, `BlackOnCyan`, `ColorOnWhite`, `ColorOnBlack`, `CyanOnBlack`, `GrayOnBlack`, `GreenOnBlack`, `MagentaOnBlack`, `PurpleOnBlack`, `Sepia`, `SolarizedDark`, `SolarizedLight`, `WhiteOnBlack`, `WhiteOnBlue`, `WhiteOnDarkRed`, `YellowOnBlack`, `BlackOnWhite`。名称不区分大小写。

* `"generateHtmlOutput"`: `(布尔值: true/false)`
    * **描述**: 是否在生成 PNG 图像的同时，也生成一个彩色的 HTML 版本。
    * **效果**: 设置为 `true` 会为每个颜色方案额外创建一个 `.html` 文件。

* `"htmlFontSizePt"`: `(浮点数)`
    * **描述**: 渲染 **HTML** 文件时使用的字体大小，单位是**磅 (points)**，这是网页设计的标准单位。

* `"enableTiledRendering"` 和 `"tileSize"`: `(布尔值, 整数)`
    * **描述**: （当前未在代码中完全实现）用于处理超大图像的分块渲染设置。

* `"outputPngExtension"`: `(字符串)`
    * **描述**: 生成的 PNG 图像文件的扩展名。默认为 `.png`。

* `"imageOutputSubDirSuffix"`: `(字符串)`
    * **描述**: 当处理单个图像文件时，生成的输出子目录的后缀。例如，处理 `cat.jpg` 会创建 `cat_ascii_output/` 目录。

* `"batchOutputSubDirSuffix"`: `(字符串)`
    * **描述**: 当处理一个文件夹（批量处理）时，生成的顶层输出目录的后缀。例如，处理 `my_pics/` 文件夹会创建 `my_pics_ascii_batch_output/` 目录。

* `"outputHtmlExtension"`: `(字符串)`
    * **描述**: 生成的 HTML 文件的扩展名。默认为 `.html`。
