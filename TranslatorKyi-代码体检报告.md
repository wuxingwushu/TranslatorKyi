# TranslatorKyi 代码体检报告

> **审查对象**：`C:\GitHub\TranslatorKyi`（GitHub: https://github.com/wuxingwushu/TranslatorKyi ，分支 `2.0.0`，48 个提交）
> **审查范围**：项目自有代码 72 个文件、约 9600 行；`ImGui/imgui*.cpp`、`stb_image.h`、`vk_mem_alloc.h`、`Tool/rapidxml*.hpp`、`AngelScript/scriptbuilder*`、`scriptstdstring*` 属第三方，不在审查范围内（但与它们交互的调用点都查了）。
> **方法**：六个子系统并行逐行精读 + 仓库级横向扫描，全部关键结论再由主审逐行回读原文件复核（复核与更正记录见最后一节）。
> **限制**：本机**无法编译**（`Environment/` 依赖目录被 `.gitignore` 忽略且不存在），因此报告中没有编译器/运行期验证的结论，所有判断都给出 `文件:行号`，可逐条复查。
> **本次审查未修改任何被审查代码**（唯一改动是设备选择功能本身的移植，见文末说明）。

---

## 摘要

这是一份"能跑、功能相当完整，但缺少安全底座"的代码库。模块划分清楚（`Tool` / `Function` / `ImGui` / `Vulkan` / `AngelScript` / `Opcode`），作者对业务流程的打磨明显（多翻译引擎、OCR、脚本、WebDAV 备份、托盘、开机自启、双语界面），但在下面五件事上几乎是空白：

1. **凭据管理**：真实的坚果云账号密码与百度/有道密钥以明文提交进仓库并推送到 GitHub（已在 git 历史里）。
2. **错误处理分层**：失败路径要么 `exit()` / `abort()`，要么 `throw` 到 `main.cpp` 里唯一的 `catch`——用户看到的是"程序一闪而过"，且**所有清理都被跳过**（设置不落盘、托盘图标残留）。
3. **并发**：全项目**没有一个工作线程**（`std::thread`/`CreateThread`/`std::async` 零出现），而翻译、OCR、WebDAV、截图全是同步阻塞调用，网络又没有超时——一次网络抖动就是"程序永久无响应"。
4. **资源所有权**：curl 句柄、GDI 对象、Windows 句柄、Vulkan 对象全靠手工配对释放，没有 RAII 封装，导致"偶发崩溃/内存泄漏/截图读到脏内存"一类问题成片出现。
5. **配置与本地化**：没有 schema、没有默认值兜底、没有迁移；`Data.ini` 里任何一个键被留空，点一次"保存"就可能让进程退出。

同时要说明：**设备选择（显卡/CPU 软渲染）这套新移植的代码本身是自洽的**，`physicalDeviceMeetsMinimumRequirements()` 与 `isDeviceSuitable()` 共用判据、排序比较器满足严格弱序、CPU/最高性能/最低性能/指定设备四种模式的降级链完整（详见《Vulkan 渲染层》一节末尾的复核）。仓库里更老的问题主要集中在上面五点。

---

## 必须马上处理的 6 件事

| # | 问题 | 位置 | 为什么是"马上" |
|---|---|---|---|
| 1 | 三个凭据已随仓库公开泄露 | `Data.ini:13-15`（坚果云）、`Data.ini:24-25`（有道）、`Data.ini:31-32`（百度） | 任何人都能登录那个坚果云盘、盗用翻译额度（按量计费=真金白银）。**代码怎么改都救不回已泄露的凭据，必须先去后台作废并重新生成。** |
| 2 | 保存设置会把有道密钥覆盖成百度的 | `Variable.cpp:102-103` | `UpdateEntry("YoudaoAPI","Youdao_ID", BaiduAppid)` 写错了变量：用户点一次"保存"，有道 ID/Key 即被百度 ID/Key 覆盖且永久丢失，此后有道翻译必然鉴权失败，且完全静默。 |
| 3 | `Data.ini` 有键值为空时，点"保存"→ 进程退出 | `ini.h:483-486` + `ImGui/Interface.cpp:1371` | `UpdateEntry` 的判据是"当前值长度"而不是"键是否存在"，空值一律抛 `runtime_error`；`SaveFile()` 没有 `try/catch`，异常穿过 ImGui 帧与 `app->run()` 被 `main.cpp:34-41` 接住→弹窗→结束进程。仓库自带的 `Data.ini` 里 `Screenshotkey=`、`WebDav_password=`、`FontFilePath=`、`Language=` 等都是空值，等于踩在雷上。 |
| 4 | 剪贴板读取函数有 5 条路径**没有 return** | `Tool/Tool.cpp:252-276` | `ClipboardTochar()` 声明返回 `std::string`，五次重试全部失败时直接掉出函数末尾（未定义行为，MSVC C4715）。这条路径正是"复制后没反应"以及偶发崩溃的现场；同函数还有 `GlobalLock` 返回值不判空、缺少 `GlobalUnlock`、`OpenClipboard` 失败仍调 `CloseClipboard`。 |
| 5 | 截图/OCR 缓冲区越界写堆 | `Tool/Tool.cpp:328-374` + `Function/tesseract.cpp:21-22` | `screen()` 每次重新读取分辨率并覆盖 `Variable::windows_Width/Heigth`，但只在 `buf == nullptr` 时分配一次（`application.cpp:307` 就是这样调的）；换显示器、改分辨率、DPI 缩放变化之后，行拷贝按新尺寸往旧缓冲区里写。 |
| 6 | 剪贴板/翻译文本拷进定长缓冲没有长度上限（栈、堆各一处） | `application.cpp:259-262`（`eng`/`zhong` 各 1MB，定义在 `ImGui/Interface.h:212-213`）+ `ImGui/Interface.cpp:277-283`、`:248-255`（`char selected_text[10000]`，`memcpy` 长度取自 `int Len = mTextLen - mCursorPos` 无上限，且终止符写在 `[len + 1]`） | 粘贴一段长文本就可能冲掉栈返回地址或相邻成员；与第 5 条同属"长度契约缺失"，是当前**最容易复现**的崩溃路径（Ctrl+V 即可触发）。 |

---

## 按主题看全部严重问题

### A. 安全与凭据

- **[严重]** 明文凭据入库（见上表第 1 条）。
- **[中]** 脚本引擎的暴露面目前是"只有字符串函数"，没有文件/网络/内存原语——这是**优点**；但 `Opcode/D.cpp:1-12` 的 `print(__int64 P)` 把整数当 `std::string*` 解引用（注册代码当前被注释掉，建议连同文件一起删除），`AngelScript/AngelScriptCode.cpp:173-187` 里注释掉的 `LoadingDLL("./Opcode/D.dll")` 死代码一旦被启用，等于把任意 DLL 暴露给 `.as` 脚本。
- **[中]** 配置与日志都在程序目录（相对路径），安装到 `Program Files` 或多人共用一台机器时设置会互相覆盖（详见配置一节对 `Variable.cpp:240-241` 的分析）。

### B. 崩溃与内存安全（日常操作即可触发）

| 问题 | 位置 | 触发条件 |
|---|---|---|
| 非 void 函数掉尾（UB） | `Tool/Tool.cpp:252-276` | 剪贴板打不开或内容不是 `CF_TEXT`（现代应用多只给 `CF_UNICODETEXT`） |
| 空串越界 | `Tool/Tool.cpp:57-71` `StrName`，`AngelScript/FunctionalFunctions.cpp:303-329` `RemoveExcessiveSpaces` | 传入空串 / 输入"全是空格"（`Opcode/Script.as:15` 每轮都调后者） |
| 目录迭代无异常保护 | `Tool/Tool.cpp:73-88` `FilePath` | 缺少 `./TessData`、`./Opcode` 目录（这是删掉目录即可复现的确定性崩溃） |
| 凭据裸指针悬垂（UAF） | `Function/Translate.h:50-54` + `ImGui/Interface.cpp:1304-1307` | 在设置里改一次密钥并保存 → 下一次翻译读已释放内存 |
| 脚本引擎 double free / UAF | `AngelScript/AngelScriptCode.cpp:104-107`、`:128-129` | `./Opcode` 里放一个语法错误的 `.as` |
| WebDAV XML 节点链零判空 | `Function/WebDav.cpp:114-117` | 服务器返回非 207 结构或错误页 XML；`doc->parse<0>()` 的 `parse_error` 也无人接 |
| 配置写回越界 | `Variable.cpp:57-61` | `Data.ini` 的颜色项写超过 4 个数字（循环用输入长度，目标是 `unsigned char[4]`） |
| OCR 缓冲区越界写 | `Tool/Tool.cpp:328-374` + `Function/tesseract.cpp:21-22` | 分辨率/DPI/多屏拓扑变化 |
| 100KB 栈数组 + 无界 `strcat` | `Function/Translate.cpp:120-150`、`:199-229` | 超长文本（约 1.1 万汉字）越界写栈；另 `rand()` 未播种使 salt 每次启动相同 |
| GDI 对象泄漏与违规用法 | `Tool/Tool.cpp:361,369-372` | 每次截图泄漏一张全屏位图（1080p≈8MB）；`GetDIBits` 在位图仍选入 DC 时调用且不检查返回值，截图内容可能是未初始化内存 |
| 缺键即 `std::terminate` | `Variable.cpp:10-63` + `main.cpp:23-24` | `Data.ini` 少一个键（62 处读取只有两项写了默认值，且两行 `ReadFile` 在 `try` 之外） |

### C. 稳定性与可观测性

- **[严重]** UI 线程同步网络请求 + **零超时**：`CURLOPT_TIMEOUT`/`CURLOPT_CONNECTTIMEOUT` 全项目 0 处，11 处 `curl_easy_perform` 只设了写回调 → 一次挂起就是永久卡死（`Function/Translate.cpp`、`Function/WebDav.cpp`、`Function/Hitokoto.cpp:23`）。
- **[中]** 强制退出绕过全部清理：`Interface.cpp:1378,1438`、`Vulkan/Window.cpp:132`（ESC）、`Function/tesseract.cpp:10`、`application.cpp:17` 的 `exit()/abort()` 不会走 `cleanUp()`/`~Window()`/托盘 `NIM_DELETE`/`Variable::SaveFile()`。
- **[中]** 没有 swapchain 重建：`application.cpp:323,380` 的 `result` 从不判断，`Window.cpp:29-32` 的 `mWindowResized` 无人读 → 改窗口大小/换显示器后可能不出画。
- **[中]** 出问题看不见：Release 是 `/SUBSYSTEM:WINDOWS`（无控制台）而工具层/脚本层诊断全走 `printf`；`DebugLog.h` 的 `TRANSLATOR_ENABLE_LOG` 默认 0、`application.cpp:47` 把校验层硬编码成 `Instance(false)`，日志不轮转、`logger` 永不释放。
- **[轻微]** `Vulkan/` 里 45 处 `throw` 的异常路径不做清理（`main.cpp:34-41` 只记日志并退出）。

### D. 配置与本地化

- **[严重]** 见上表第 2、3 条。
- **[中]** 语言文件用相对路径（`Variable.cpp:240-241`）：换工作目录（快捷方式、开机自启）就构造即抛 `"ini file not found."`，是崩溃而不是"界面留白"。
- **[轻微]** `ini.h:585-599` 的 `write_Gai` 是整文件重建：注释、空行、键顺序、`=` 两侧空格在第一次保存后全部消失（键序变字母序），且直接截断覆盖、无原子性（写入中途失败会让下次启动因缺键崩溃）。
- **[通过]** `Language/zh.ini` 与 `eng.ini` 各 82 个键，键集合、段落划分、出现顺序逐行一致；`Variable.cpp` 读取的 62 个键全部存在于 `Data.ini`，没有冗余键——**键集合是自洽的，风险全在"值"**。
- **[轻微]** 硬编码中文实查 126 处，真正漏掉 i18n 的只有 3 处：`Function/Translate.h:40` 的 `{"百度","爬虫","有道"}`（被 `Interface.cpp:334` 当按钮文案）、`Function/Translate.cpp` 的 10 处 `return "错误";`、`Vulkan/instance.cpp:381,383` 的 `CpuSoftwareRenderReason`（会渲染进设置界面）。

### E. Vulkan 渲染层

- **[中]** RenderPass 只有 1 个附件且采样数写死 `VK_SAMPLE_COUNT_1_BIT`（`application.cpp:97-118`），而 ImGui 管线用 `MSAASamples = getMaxUsableSampleCount()`（`application.cpp:82`，桌面 GPU 返回 4/8）——管线与 RenderPass 不符，验证层报错，关掉验证层则是未定义行为。
- **[中]** FrameBuffer 绑 3 个附件（`Vulkan/swapChain.cpp:181-190`）而 RenderPass 只声明 1 个（违反 `attachmentCount` 必须相等的有效用法）。**这条在 HEAD 版本里就已存在**，不是本次移植引入的——程序能跑说明驱动容忍了这次误用。
- **[中]** 同时，`Vulkan/swapChain.cpp:153-173` 在 `sampleCount == 1` 时把 MSAA 图置为 `nullptr`，而 `:183` 无条件解引用它 → 这类设备上必崩（该分支是移植新版代码时**新引入**的）。
- **[中]** 截图纹理路径：`ImGui/Interface.cpp:1535` 用 `R8G8B8A8` 建图、`:1566` 用 `B8G8R8A8` 建视图（校验错误 + 颜色偏蓝）；`:1526/:1620` 的 memcpy 无尺寸校验；`:1634-1642` 命令缓冲不释放；`:1710-1715` `vkFreeMemory` 在 `vkDestroyBuffer` **之前**。
- **[中]** 每帧复用主指令缓冲却不检查在途（`application.cpp:333` 用默认 flag=0，fence 粒度按帧、缓冲按交换链 image 索引）；`Vulkan/commandBuffer.h:38` 给 primary 缓冲也传了非空 `pInheritanceInfo`。
- **[轻微]** 移植残渣：`Vulkan/instance.cpp:608` 的 `pApplicationName="vulkanLession"`、`Vulkan/device.cpp:457-514` 无人调用的 `getComputeCapabilities()`、注释里的 UVDynamicDiagram/粒子系统、`Vulkan/Window.cpp:129-162` 的 W/S/A/D 空分支；整套 `Pipeline`/`Shader`/`Sampler`/`DescriptorSet*`/`DescriptorPool`/`description.h` 的公开接口全仓库零引用（约 800 行可删，ImGui 用自己的一套）；`application.h:13-15` 写成 `#include "VulKan/commandBuffer.h"`（小写 k），在大小写敏感环境必挂。
- **[轻微]** `Vulkan/device.cpp:93` 只用 `VK_PHYSICAL_DEVICE_TYPE_CPU` 判定软件渲染（自报 OTHER/VIRTUAL_GPU 的软件 ICD 会被当硬件）；`Vulkan/instance.cpp:360` 写 `VK_ICD_FILENAMES` 后退出不还原；`Vulkan/device.cpp:178-181` 的"已按自动选择最高性能改用"提示在"自动最低性能"模式下文案错误。

### F. 构建与工程化

- **[严重]** VS 生成器下 Debug/Release 判定完全失效：`CMakeLists.txt:6,28,60,90` 四处用 `CMAKE_BUILD_TYPE`（VS 下永远为空）→ 即使选 Debug 也链接 Release 库，Debug DLL 拷贝也不执行。
- **[中]** `CMakeLists.txt:53` 每次 configure 用仓库模板**覆盖** `build/Data.ini`（用户填的密钥、快捷键全丢）；`CMakeLists.txt:10` 的 `/O3` 是 GCC 写法，MSVC 静默忽略。
- **[中]** 缺 `/utf-8`：源码是 UTF-8，MSVC 却按系统 ANSI 代码页解析，这正是 `main.cpp:2-6` 注释里"换电脑编译就报语法错误"的根因（作者的对策是"中文后面加空格"）。
- **[中]** 没有 LICENSE、没有 CI、没有测试、没有 `.clang-format`；README 的文件结构树仍是旧名 `Translate-miku` / `imgui` / 已删除的 DX11SDK。
- **[中]** 29MB 的 `TessData/*.traineddata` 与 2MB 字体已被跟踪进 git（`.gitignore` 补得太晚，对已跟踪文件无效）。
- **[轻微]** `aux_source_directory`、`/MT` 被注释掉而依赖库多为 `/MT`、`/SUBSYSTEM:WINDOWS` 只在 Release 分支设置、依赖路径无 `if(EXISTS)` 守卫、`angelascript64.lib` 的 ABI 宏未统一、没有 `install`/CPack。

### G. 界面层（`ImGui/Interface.cpp`，1718 行）

| 问题 | 位置 | 后果 |
|---|---|---|
| `Window` 被三处同时"拥有"，正常退出必然二次 `delete` | `main.cpp:30` 创建 → `application.cpp:404`（`cleanUp()` 里 `delete mWindow`）→ `main.cpp:43` 再 `delete mWin`；第三条 `ImGui/Interface.cpp:1374` `delete mWindown` | 正常退出=同一对象析构两次（UB，偶发崩溃/堆损坏）；改字体保存后的重启分支则靠 `exit(0)` 跳过 `cleanUp()`，Vulkan 资源全泄漏 |
| 定长 `char` 缓冲越界写 | `Interface.cpp:250-255`、`:277-283`、`:609-615`（`selected_text[10000]` 无上下界、终止符写在 `[len+1]`）；`InputText` 把剪贴板内容 `memcpy` 进 `SetWebDav_url[128]` 等 128 字节数组 | 复制一段长文本 → 栈/成员缓冲溢出 |
| 动态字符串被当 printf 格式串 | `Interface.cpp:1402`、`:1480`，以及 `:837/965/974/990/1011/1044/1214/1241` 的 `ImGui::Text(...)` | 一言接口返回或语言 ini 里出现 `%` 即越界读栈（崩溃） |
| 纹理上传路径无一处检查失败 | `Interface.cpp:1512-1705` `LoadTextureFromFile`：15 处 `check_vk_result` 被注释、`findMemoryType` 失败返回 `0xFFFFFFFF` 无人判断 | `vkMapMemory` 失败后仍 `memcpy` 到空指针；每次截图泄漏 1 个 `VkCommandBuffer`（全函数无 `vkFreeCommandBuffers`）；末尾无条件 `return true` |
| 字号可被设成 0 → 除零 | `Interface.cpp:993`（`InputFloat` 无下限）→ `:300`、`:1469`、`:1477` | 整数/浮点除零 |
| [已更正·降级为轻微] 每帧重录二级命令缓冲却不显式 `reset()` | `Interface.cpp:139-145`、`:227-230` + `Vulkan/commandBuffer.cpp:28-40`；注意 `Vulkan/commandPool.h:8` 的默认参数**已经**是 `RESET_COMMAND_BUFFER_BIT`，`:14-18` 的 `reset()` 无人调用 | 02 号报告"命令池缺少 RESET 标志"的理由不成立；`vkBeginCommandBuffer` 只禁止从 recording/pending 开始，每帧 `vkWaitForFences` 之后重录合法。仅建议显式 reset 以明确意图（详见 7.9） |
| 数组下标无钳制 | `Interface.cpp:1048-1059`（`BaiduitemsName[Variable::ReplaceLanguage]`）、`:1062-1073`（`LanguageS[LanguageIndex]`，且无空表守卫）、`:334-338`（`TranslateName[mTranslate->mTranslate]`，硬编码 3） | `Data.ini` 里把语言/引擎编号改大即越界读 |
| 切换 OCR 模型用"显式析构 + 显式构造"（不是 placement new） | `Interface.cpp:1320-1326` | 构造函数抛异常就留下一个已析构对象，后续使用即 UAF |

### H. 应用核心与生命周期（`main.cpp`、`application.cpp/h`）

| 问题 | 位置 | 后果 |
|---|---|---|
| `InterFace->eng/zhong` 两个 1MB 定长缓冲的 `memcpy` 没有任何长度上限 | `application.cpp:259-262`；缓冲定义 `ImGui/Interface.h:212-213 char eng[1024 * 1024]; char zhong[1024 * 1024];` | 超长剪贴板/OCR 结果溢出到相邻成员乃至堆；恰好等于 1MB 时结尾 NUL 被挤掉，随后 `ImGui::InputTextMultiline(..., eng, IM_ARRAYSIZE(eng))` 读到缓冲区外 |
| 启动期异常逃出 `try` | `main.cpp:23-26`（`try` 从 `main.cpp:34` 才开始）、`ini.h:302`、`Variable.cpp:240-241` | `Data.ini` 不在 CWD、语言文件缺键 → 两参 `Get` 抛 `std::runtime_error` → 无人接 → `std::terminate`；Release 无控制台（`CMakeLists.txt:113`），用户只看到"双击没反应" |
| 一言间隔与显示时长被硬编码覆盖 | `application.cpp:217`（`> 20000`）、`ImGui/Interface.h:74`（`return 10000;`），而 `Data.ini:3-4` 是 `60000`/`10000` | `Variable::HitokotoTimeInterval/HitokotoDisplayDuration`、ini、设置界面三者都有，用户在界面上改的值被静默忽略（默认配置下间隔差 3 倍） |
| 计时用 `clock()`（进程 CPU 时间）却当墙钟用；`clock_t TranslateTime` 未初始化 | `application.cpp:195/217/219`、`ImGui/Interface.h:139/151/211` | 空闲时每轮只烧掉零点几毫秒 CPU ⇒ 面板"该消失时不消失"（放大数倍）；OCR 打满 CPU 时又提前消失。两个方向都会偏 |
| `vkAcquireNextImageKHR` 返回值被完全忽略；`vkQueuePresentKHR` 的结果是死存储；`mWindowResized` 全仓库无读取点 | `application.cpp:322-329/380`、`Vulkan/Window.cpp:31`、`Vulkan/Window.h:40` | `OUT_OF_DATE / SUBOPTIMAL / SURFACE_LOST` 时 `mImageAvailableSemaphores[mCurrentFrame]` 没被 signal 却被用于提交 ⇒ 该次提交永不完成 ⇒ 下一帧 `mFences[...]->block()` 永久阻塞（界面卡死，不崩溃，更难查）；`getWidth/getHeight`、`StructureSwapChain()` 都暴露好了但没人接线 |
| 热键用 `std::string` 存单字符并按 `[0]` 取 VK 码 | `application.cpp:236/237/269/270/306/311`、`Variable.h:66-67`、`Data.ini:43-46` | 不是越界（空串时 `[0]` 返回结尾 NUL 的引用，`GetKeyState(0)` 恒 0），但依赖"大写字母的 ASCII 恰好等于 VK 码"：改成小写 `d` 会变成 `VK_NUMPAD4`、`"Space"` 会变成 `VK_S` |
| 忙等阻塞主循环 + 固定 `Sleep(5)` 赌剪贴板已更新 | `application.cpp:237-240`、`270-273`（`while + Sleep(20)`）、`245/278/301`（`Sleep(5)`） | 按住热键期间不渲染、不处理托盘消息（"界面像死了"）；源程序慢时读到上一次剪贴板内容 → 偶发翻译错对象 |

> **对第七节 7.8 的补充（核心层报告的独立发现）**：`Vulkan/Window.cpp:129` 的 `processEvent()`（内有 ESC→`exit(0)`）**全仓库没有任何调用点**，也**没有任何 `glfwSetWindowShouldClose` 调用** ⇒ `while (!mWindow->shouldClose())`（`application.cpp:196`）目前无法由用户操作结束，`cleanUp()`（`application.cpp:404`）与 `main.cpp:43 delete mWin;` 事实上**都跑不到**。所以当前真实的退出路径是菜单"退出"（`ImGui/Interface.cpp:1438`）与重启（`:1378`）里的 `exit(0)`——**什么都不释放**；二次释放是"一旦有人让主循环正常返回就立刻命中"的地雷。结论不变（所有权必须单一化），但实施顺序应是"先把退出收敛成主循环返回并统一 `cleanUp()`，顺手拆掉地雷"。

---

## 建议的优化项（不是 bug，但决定这个项目能走多远）

1. **给系统调用加 RAII 薄封装**：`CurlHandle`（含固定超时、状态码校验）、`GdiScreen`（截图，返回 `std::vector<uint8_t>` + 尺寸）、`Win32Handle`（注册表/剪贴板/窗口 DC）、Vulkan 对象的 deleter。本报告里"手工配对失败"类问题（GDI 泄漏、`vkFreeMemory` 顺序、`GlobalUnlock` 缺失）会一次性消失大半。
2. **配置 schema 集中化**：把「段名 + 键名 + 默认值 + 取值范围」做成一张表，读写共用同一份定义（`Variable.cpp:102-103` 那个 bug 就是信息散落三处的直接结果），并加 `ConfigVersion` 支持迁移。
3. **把耗时操作移出 UI 线程**：任务队列 + `std::atomic<bool> mBusy`，主循环保持 60Hz 渲染并显示"翻译中…"。这是全部改进里性价比最高的一条。
4. **错误处理分层**：底层返回错误码/`optional`，中间层决定"重试/降级/上报"，只有真正不可恢复时才结束进程；`cleanUp()` 做成幂等并保证在任何退出路径上调用。
5. **可观测性**：打开日志开关、校验层做成可配置、spdlog 加 `rotating_file_sink`、给界面加一个"诊断"面板（版本、渲染设备、最近错误）、脚本异常打印行号。
6. **纯函数抽出来做单测**：`UrlEncode`/`FromHex`/MD5、`StrName`、`RemoveExcessiveSpaces`、`ini` 读写、`Variable` 的默认值逻辑——这些不依赖窗口和 GPU，测试成本极低。
7. **字符串统一**：`char*` + `strcat` + 魔数长度改成 `std::string`/`std::string_view`，直接消灭整类缓冲区问题。
8. **脚本沙箱**：给 AngelScript 设执行超时/栈上限，并保证任何脚本错误都不影响主程序。

---

## 可以添加的功能

| 建议 | 理由 | 成本 |
|---|---|---|
| 崩溃转储（`MiniDumpWriteDump` + `SetUnhandledExceptionFilter`） | 用户报的崩溃目前只能靠猜；spdlog 已有日志基础，补 dump 即可定位 | 小 |
| 配置迁移/校验（版本字段 + 缺失键补默认值） | 老 `Data.ini` 缺新键时行为不确定 | 小 |
| 版本号 + 自动更新检查（读 GitHub Releases） | 版本只体现在分支名 `2.0.0`，用户不知道自己是不是最新 | 中 |
| GitHub Actions：Windows 上配置 + 编译（缓存 `Environment`） | "能不能编译"现在只能靠人工，移植/编码类问题反复出现 | 中 |
| 明确的编码规范 + `.clang-format`/`.editorconfig` | 仓库混用制表符与空格、UTF-8 中文注释无保护 | 小 |
| 把 `Environment/` 依赖做成 vcpkg.json / Conan 清单 | 目前靠网盘分享预编译包，新人（和 CI）无法一键复现环境 | 大 |
| 翻译历史与生词本（本地 SQLite + 导出 Anki/CSV） | 现有流程是"翻译完复制走"，历史完全不留存；这是同类工具最常用的功能 | 中 |
| 多引擎结果对照/自动择优 | 已经同时接了百度、有道、爬虫三个引擎，界面上却只能一个一个选 | 中 |
| 截屏翻译的选区模式（框选/区域记忆） | 现在是全屏 OCR，长文本与多显示器场景下误差大、也慢 | 中 |
| 全局热键可自定义 + 冲突提示 | 快捷键写死在 `Data.ini`（`MakeUp=18`=Alt、`Screenshotkey=D`、`Choicekey=Q`、`Replacekey=R`），冲突时无提示 | 小 |
| 设置界面顶部显示"本次启用的渲染设备/降级原因" | 设备选择已经写好，但降级原因只在启动瞬间提示一次 | 小 |
| 便携模式（配置随 exe 走 / `%APPDATA%` 二选一） | 现在配置固定写程序目录，绿色版与安装版需求冲突 | 小 |

---

## 分节索引

| 小节 | 覆盖内容 | 主要文件 |
|---|---|---|
| 仓库级与构建系统 | 凭据泄露、工程化缺口、CMake 全部问题、跨子系统横向观察、可添加功能表、复核记录（第七节）、对分项报告"待确认"条目的结案（第八节） | 仓库根目录、`CMakeLists.txt`、`.gitignore`、`README.md` |
| 核心与生命周期 | 程序入口、主循环、初始化/清理顺序、窗口与托盘 | `application.cpp/h`、`main.cpp`、`base.h` |
| 界面层 | 主界面与设置界面的状态同步、交互流程、ImGui 用法 | `ImGui/Interface.cpp/h` |
| 功能层（翻译 / OCR / 网络 / 脚本） | 三个翻译引擎、截图 OCR、WebDAV、一言、`.as` 脚本交互 | `Function/*`、`Opcode/*`、`PileUp.h` |
| 配置与本地化层 | 配置读写、ini 封装、默认值、多语言键一致性 | `Variable.cpp/h`、`ini.h`、`Data.ini`、`Language/*` |
| Vulkan 渲染层 | 实例/设备选择、交换链、RenderPass、管线、截图纹理 | `Vulkan/*` |
| 工具层与脚本引擎 | 剪贴板、截图（GDI）、文件枚举、注册表、AngelScript 生命周期与内建函数 | `Tool/*`、`AngelScript/*` |

> 关于本次的设备选择功能：`Vulkan/` 目录原本是作者另一个项目 PixelClean 的逐字节副本（全是 `Global::` 引用），本次已适配为本项目的 `Variable::` 命名空间，并在设置界面加上与 PixelClean 一致的下拉框（自动最高/最低性能、CPU 软件渲染、逐台识别到的设备、重启生效提示、指定设备未识别时的降级提示）。所有 `Global::`/`PixelClean`/`shaderc`/`GameKeyEnum` 等残留已清零，具体改动见 `Variable.cpp/h`、`ImGui/Interface.cpp`、`Language/{zh,eng}.ini`、`Data.ini`。



---

## 仓库级与构建系统 · 体检报告

> 审查对象：C:\GitHub\TranslatorKyi（GitHub: https://github.com/wuxingwushu/TranslatorKyi，分支 2.0.0，48 个提交）
> 统计口径：本次审查覆盖项目自有代码 72 个文件（约 9600 行）；`ImGui/imgui*.cpp`、`stb_image.h`、`vk_mem_alloc.h`、`Tool/rapidxml*.hpp`、`AngelScript/scriptbuilder*`、`scriptstdstring*` 属第三方，不在审查范围内。
> 说明：本机**无法编译**（`Environment/` 依赖目录被 .gitignore 忽略且不存在），所有结论来自逐行精读，不含编译器验证。

### 一、[严重] 凭据明文提交进仓库（Data.ini）

- 位置：`Data.ini:12-20`（WebDAV）、`Data.ini:24-25`（有道）、`Data.ini:31-32`（百度）
- 现象：**真实可用的账号与应用密钥被跟踪进 git 并推送到 GitHub**：
  - `Data.ini:13` `url = https://dav.jianguoyun.com/dav/`
  - `Data.ini:14` `username = 1779690036@qq.com`
  - `Data.ini:15` `password = ahbhd5pd94e3qpnm`（坚果云应用密码）
  - `Data.ini:24` `Youdao_ID = 1adbf86823e273df` / `Data.ini:25` `Youdao_Key = RO9RM5iece4g5iCbOxjd0hb9EDsvNMFr`
  - `Data.ini:31` `Baidu_ID = 20210925000956550` / `Data.ini:32` `Baidu_Key = os4RtAbGDCDhvgWvGSPu`
- 证据（历史确认）：`git log -S 'ahbhd5pd94e3qpnm' -- Data.ini` 命中 `7735724 添加 坚果云WebDav 功能` —— 说明这些凭据**已经进入 git 历史**，不是本次改动引入的；`Data.ini` 是被跟踪文件（`git ls-files` 可见），因此只要仓库是公开的（`origin` 指向 github.com/wuxingwushu/TranslatorKyi），凭据即为公开泄露状态。
- 影响：他人可直接登录该坚果云盘读写文件；百度/有道密钥可被盗用翻译额度（按量计费的账号会产生真实费用）。
- 建议修法（按优先级）：
  1. **立刻作废这三个凭据**：坚果云后台吊销该应用密码并重新生成；百度翻译开放平台、有道智云后台重置 App Key。这一步必须先做，代码怎么改都救不回已经泄露的凭据。
  2. 把 `Data.ini` 从版本控制里移出：`git rm --cached Data.ini`，并把 `Data.ini` 加入 `.gitignore`，仓库里改放一份 `Data.ini.example`（键齐全、值留空）。
  3. 首次运行时若检测到 `Data.ini` 不存在，由程序自己写入默认模板（`Variable::SaveFile()` 已有写盘能力，加一个"文件不存在则先写入默认值"的分支即可）。
  4. 若要彻底清除历史：用 `git filter-repo --path Data.ini --invert-paths` 重写历史并强推（注意这会改写 48 个提交的哈希，协作者需要重新克隆）。**密钥既然已经泄露，重写历史不能替代第 1 步的作废。**
  5. 用户设置建议改存到 `%APPDATA%\TranslatorKyi\Data.ini`，源码目录只留模板——顺带解决"重新 configure 时设置被覆盖"（见 3.3）。

### 二、[中等] 仓库工程化缺口

#### 2.1 没有 LICENSE
- 位置：仓库根目录（`Test-Path LICENSE` = False）
- 现象：项目引用（`README.md`）与二进制分发都没有许可证声明。
- 影响：默认"保留所有权利"，别人不能合法地使用/修改/分发；同时项目内打包了 `TTF/SmileySans-Oblique.ttf`（得意黑）、`TessData/*.traineddata`（Apache-2.0）等第三方资源，缺少 LICENSE/NOTICE 会让授权状态更含糊。
- 建议：加 `LICENSE`（自选 MIT/Apache-2.0/GPL），并在 `README.md` 里列出第三方组件与各自许可证（ImGui MIT、GLFW zlib、Vulkan Apache-2.0、tesseract Apache-2.0、curl curl、jsoncpp MIT、OpenSSL Apache-2.0、spdlog MIT、AngelScript zlib、得意黑 OFL-1.1）。

#### 2.2 29MB 模型文件已在版本控制内，`.gitignore` 补得太晚
- 位置：`.gitignore:4-8` 忽略了 `TessData/*.traineddata`，但 `git ls-files` 显示它们**仍被跟踪**：`TessData/chi_sim.traineddata`（12.7MB）、`TessData/chi_tra.traineddata`（12.7MB）、`TessData/eng.traineddata`（4.0MB），另有 `TTF/SmileySans-Oblique.ttf`（2.0MB）。
- 现象：`.gitignore` 只管未跟踪文件，对这些已经进入历史的文件无效，仓库体积会永久背着约 31MB。
- 建议：模型文件改为运行时下载或随 Release 附件分发；若确认不再跟踪，用 `git rm --cached` + `git filter-repo` 清理历史。至少要在 README 里说明模型从哪来（当前 README 的"环境构建"只讲了 tesseract 编译，没讲模型从哪获取）。

#### 2.3 没有任何自动化：无 CI、无测试、无格式约定
- 现象：仓库只有 `.git`、`.gitattributes`、`.gitignore` 三个隐藏项，没有 `.github/`（无 CI）、没有测试目录、没有 `.clang-format`；`CMakeSettings.json` 与 `launch.vs.json` 是 VS 专用配置。
- 影响：每次改动只能靠人手点一遍；本次审查期间就发现"改了一个字符导致 `Vulkan/instance.h`、`Vulkan/device.h` 整体损坏"这类问题只能靠 diffstat 对账发现。
- 建议：至少加一个 GitHub Actions 工作流做"配置 + 编译"（Windows runner + 从 Release 缓存 Environment 依赖），把"能不能编译"从人工变成自动；再加 `.clang-format`（`BasedOnStyle: Microsoft` + `UseTab: Always`）统一风格。

#### 2.4 README 与实际结构不一致
- 位置：`README.md:24-50` 的文件结构树
- 现象：树里写的是 `Translate-miku`（旧名）、`imgui`（实际目录是 `ImGui`）、`Environment/DX11SDK`（项目已改用 Vulkan，`CMakeLists.txt:17-26` 里根本没有 DX11 目录）；README 也没提 `.gitignore` 忽略的 `Vulkan/`、`Opcode/`、`DebugLog.h` 等目录与文件。
- 建议：按实际目录重写文件结构树；补"如何编译"（需自备 `Environment/`、CMake ≥3.12、VS2022、Vulkan SDK）、"如何运行"（依赖同目录的 `Data.ini`/`Language`/`TessData`）、以及快捷键表（`Data.ini:43-46` 的 `MakeUp=18`(Alt)、`Screenshotkey=D`、`Choicekey=Q`、`Replacekey=R`）。

#### 2.5 大量文件是"移植进来的兄弟项目代码"，历史与注释对不上
- 现象：`Vulkan/` 目录 38 个文件与作者另一个项目 `C:\GitHub\PixelClean` 逐字节相同（本次会话开头已用 MD5 核对），随后才做的 `Global::`→`Variable::` 适配；`Opcode/`、`Function/` 的注释也大量保留着旧项目措辞。
- 建议：README 或代码头部注明各模块的来源与许可；把已经确认不再使用的移植残留直接删除而不是留在原地。

### 三、构建系统（CMakeLists.txt）

#### 3.1 [中等] Release 优化参数 `/O3` 对 MSVC 无效
- 位置：`CMakeLists.txt:10` `set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O3")`
- 现象：`/O3` 是 GCC 的写法，MSVC 只认 `/O2`（或 `/Ox`）。cl.exe 会报 `D9002: ignoring unknown option '/O3'` 并**静默忽略**，也就是说 Release 实际上仍在用默认优化级别（`/Od` 除非 CMake 的 Release 标志另有设置；`CMAKE_CXX_FLAGS` 与 `CMAKE_CXX_FLAGS_RELEASE` 是两套变量，此处追加的是前者）。
- 建议：删除这一行，改为 `set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")`；或用 `target_compile_options` + 生成器表达式。

#### 3.2 [严重] 多配置生成器下 Debug/Release 判定失效
- 位置：`CMakeLists.txt:6`、`28`、`60`、`90`（四处 `if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))`）
- 现象：`CMAKE_BUILD_TYPE` 只在**单配置生成器**（Ninja/Makefile）下有效；用 Visual Studio 生成器时它是**空字符串**，所以永远走 `else()` 分支：即使你在 VS 里选 Debug，也会链接 `spdlog.lib`/`libcurl.lib`/`angelscript64.lib`/`tesseract53.lib`（Release 版），而 Debug 版库（`spdlogd.lib`/`libcurl-d.lib`/`angelscript64d.lib`/`tesseract53d.lib`）永远不会被用到；`CMakeLists.txt:61-62` 的 Debug DLL 拷贝也不会执行。而本仓库自带 `CMakeSettings.json` 与 `launch.vs.json`（`CMakeLists.txt:52` 还在往 `.vs` 目录拷 `launch.vs.json`），说明作者主用 VS —— 即这个分支**在实际开发中一直是坏的**。
- 影响：Debug 构建要么链接失败，要么混用 Debug/Release CRT 导致堆损坏、`_ITERATOR_DEBUG_LEVEL` 不匹配等难查错误。
- 建议：改用生成器表达式，让配置由生成器决定：
  ```cmake
  target_link_libraries(TranslatorKyi PRIVATE
      vulkanLib textureLib imguiLib ToolLib FunctionLib AngelScriptLib
      vulkan-1.lib glfw3.lib jsoncpp_static.lib libssl_static.lib libcrypto.lib
      leptonica-1.83.1.lib
      $<$<CONFIG:Debug>:libcurl-d.lib spdlogd.lib angelscript64d.lib tesseract53d.lib>
      $<$<NOT:$<CONFIG:Debug>>:libcurl.lib spdlog.lib angelscript64.lib tesseract53.lib>)
  ```
  `link_directories` 同理拆成 `$<$<CONFIG:Debug>:.../lib/Debug>` 形式，并把 `link_directories` 换成 `target_link_directories`。

#### 3.3 [中等] 每次 CMake configure 都会用仓库模板覆盖用户的 Data.ini
- 位置：`CMakeLists.txt:53` `execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})`
- 现象：`cmake -E copy` 是**无条件覆盖**；只要重新 configure（VS 里改 CMakeSettings、删缓存、切配置都会触发），用户在 `build/Data.ini` 里填的百度/有道密钥、WebDAV 密码、快捷键、字体设置**全部被仓库里的模板值覆盖**。而 `execute_process` 没有取 `RESULT_VARIABLE`，失败也不会报错（`CMakeLists.txt:52` 往 `.vs` 拷贝时若该目录不存在就是这种静默失败）。
- 建议：改成"不存在才拷"：`execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ...)` 仍会覆盖；正确做法是用 `configure_file(... COPYONLY)` 配合 `if(NOT EXISTS ...)`，或者干脆让程序自己在首次运行时生成配置（与 1.4 的建议合并），CMake 只负责拷 `Language`/`TTF`/`TessData`/`Opcode` 这些纯数据目录。

#### 3.4 [中等] 缺少 `/utf-8`，中文源码依赖"机器默认代码页"
- 位置：`CMakeLists.txt:4-11` 未设置任何字符集选项；`main.cpp:2-6` 的注释正是这个坑的现场记录：
  ```
  * 换了电脑编译说有语法错误：
  *		目前知道的是：中文输出的问题。
  *		解决方法：在中文结尾后加一个 “空格” 或  字母  。
  ```
- 现象：源码是 UTF-8（本次审查逐个文件确认过编码），但 MSVC 默认按**系统 ANSI 代码页**（简体中文 Windows 上是 GBK/936）解析源码；当 UTF-8 中文字节序列恰好以 `\`（0x5C）等字符结尾/开头时，会吃掉后续的引号或换行，于是出现"换台电脑就报语法错误"，作者的对策是"在中文后面加空格"——治标不治本。同样的隐患还会让 `u8"中文"` 字面量在日志与 UI 里出现乱码。
- 建议：给所有 target 加 `/utf-8`（等价于 `/source-charset:utf-8 /execution-charset:utf-8`）：
  ```cmake
  add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
  ```
  这一条同时能让 3.1 的 `/O3` 警告、以及 `Variable.cpp`/`Language/*.ini` 里既有的少量乱码注释都更容易暴露出来。历史上还有一次专门的提交 `d9254a7 编码 GBK -> UTF8`，说明这个坑反复踩过。

#### 3.5 [轻微] 其他构建细节
- `CMakeLists.txt:13` `aux_source_directory(. DIRSRCS)`：CMake 官方不建议，新增根目录 `.cpp` 不会自动进工程（必须重新 configure），且无法表达"哪些文件属于哪个 target"。建议显式列出，或改 `target_sources`。
- `CMakeLists.txt:84,113` `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup` 只在非 Debug 分支设置 → Debug 构建会额外弹出一个控制台窗口，与 Release 行为不一致（调试时可能是有意为之，但应加注释说明）。
- `CMakeLists.txt:81-82` 的 `/MT`、`/MTd` 被注释掉了，而 `tesseract53.lib`/`leptonica`/`libcurl` 这类静态库通常是 `/MT` 编译的；CRT 混用会在链接期或运行期引发难以定位的问题。建议明确统一为 `/MT`（Release）与 `/MTd`（Debug），或确认这些库确实是 `/MD` 构建。
- `CMakeLists.txt:17-26,29-48` 用相对路径 `./Environment/...` 拼在 `${CMAKE_CURRENT_SOURCE_DIR}` 之后，写法能工作但没有 `if(EXISTS)` 检查，依赖缺失时只会在编译阶段报一堆"找不到头文件"，而不是在 configure 阶段给出清晰提示。建议加一个 `find_path`/`if(NOT EXISTS)` 守卫并 `message(FATAL_ERROR "缺少 Environment/...")`。
- `CMakeLists.txt:107,131` 链接了 `angelscript64(d).lib`，但 `include_directories` 里只有 `${...}/AngelScript/include`；`AngelScript/CMakeLists.txt` 只有 2 行（`aux_source_directory` + `add_library`），库的 ABI/字符集配置（`AS_USE_STD_STRING` 等宏）没有统一，脚本字符串类型容易踩坑。
- 缺少 `cmake --install` / CPack 打包步骤，发布只能手工拷 DLL（README 里让人从百度网盘下 Environment 也印证了这一点）。

### 四、跨子系统的横向观察

#### 4.1 [严重] 全项目没有工作线程：所有耗时操作都跑在 UI 线程上
- 位置：全项目自有代码中 `std::thread`/`CreateThread`/`_beginthread`/`std::async`/`std::future`/`std::condition_variable` **出现 0 次**（对整仓库含第三方代码 grep 过，零匹配）；唯一的并发原语是 `Vulkan/descriptorSet.cpp:13,29` 的 `std::mutex* wMutex`（一个始终为 nullptr 或无人竞争的锁）。
- 现象：`application.cpp:256` 的 `InterFace->mTranslate->TranslateAPI(...)`（内部走 libcurl 同步请求）、`application.cpp:307` 的 `TOOL::screen(buffer)`（GDI 全屏截图）、tesseract OCR 全部在 `mainLoop()` 的同一条线程里同步执行；主循环（`application.cpp:194-228`）每帧只有 `sleep_for(10ms)` + `pollEvents()`。
- 影响：任何一次翻译/OCR 慢下来（网络抖动、大图 OCR），窗口就不再 `pollEvents()` → 界面冻结、Windows 给它标"无响应"，托盘图标点击也失效；`main.cpp:37` 的异常捕获只在 `run()` 返回时才生效。
- 建议：把"翻译 + OCR + WebDAV"三类调用下沉到工作线程（`std::async`/任务队列 + `std::atomic<bool> mBusy`），主循环继续以 60Hz 渲染并显示"翻译中…"；结果通过线程安全队列回主线程。这是本报告里**性价比最高的架构级改进**。

#### 4.2 [中等] 强制退出路径绕过所有清理
- 位置：`ImGui/Interface.cpp:1378`、`ImGui/Interface.cpp:1438`、`Vulkan/Window.cpp:132`（ESC 键）、`Function/tesseract.cpp:10`（tesseract 初始化失败即 `exit(1)`）、`application.cpp:17`（Vulkan 校验失败即 `abort()`）
- 现象：`exit()` 不会展开栈，`Application::cleanUp()`、`~Window()`（`glfwDestroyWindow`/`glfwTerminate`）、托盘图标 `Shell_NotifyIcon(NIM_DELETE)`、`Variable::SaveFile()` 全部被跳过。
- 影响：退出时设置不落盘（用户改了快捷键没保存就退出=白改）；托盘图标残留成"幽灵图标"直到鼠标划过；Vulkan/GLFW 资源未释放（进程结束时 OS 会回收，但调试时会掩盖泄漏）。
- 建议：统一改成"设置 `mShouldClose = true` 让主循环自然退出"，`mainLoop()` 返回后走 `cleanUp()`；`tesseract.cpp` 的初始化失败应向上返回错误并在界面上提示，而不是静默 `exit(1)`。

#### 4.3 [轻微] `windows.h` 以宏污染的方式被引入到几乎所有翻译单元
- 位置：`base.h:12` `#include <windows.h>`，而 `base.h` 又被 `application.h:3`、Vulkan 各头文件、`Tool/Tool.h` 间接引用
- 现象：未定义 `NOMINMAX` 与 `WIN32_LEAN_AND_MEAN`，`windows.h` 会带来 `min`/`max` 宏（与 `std::min/std::max`、`glm` 冲突，典型报错是 `error C2589: '(' : illegal token on right side of '::'`）、`near`/`far` 宏，并拉入 winsock 1.1 等大量无用头文件。
- 建议：在 `base.h` 最顶部（任何 include 之前）加：
  ```cpp
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  ```
  另外 `base.h` 里 `const std::vector<const char*> validationLayers` 定义在头文件里（内部链接，每个 TU 一份副本），建议改成 `inline constexpr const char* kValidationLayers[] = {...}` 或放进 .cpp。

#### 4.4 [中等] 网络请求没有任何超时设置
- 证据：全项目 `CURLOPT_TIMEOUT` / `CURLOPT_CONNECTTIMEOUT` **0 处**；`Function/Translate.cpp`（156、235、285、593）、`Function/WebDav.cpp`（106、168、193、266、327、366）、`Function/Hitokoto.cpp:23` 共 11 处 `curl_easy_perform`，每处只设置了 `CURLOPT_WRITEFUNCTION`。
- 现象：libcurl 默认**不设超时**。连接建立慢（例如 WebDAV 服务器不可达但未立刻拒绝）或服务端把连接挂起时，`curl_easy_perform` 会一直阻塞不返回；由于它是在 UI 线程上调用的（见 4.1），用户看到的就是"点了翻译之后程序永远卡死"，只能从任务管理器结束进程——而且此时设置没有保存（见 4.2）。
- 建议：统一封装一个 `CurlRequest()` 帮助函数，内部固定 `CURLOPT_CONNECTTIMEOUT, 5L`、`CURLOPT_TIMEOUT, 15L`、`CURLOPT_FOLLOWLOCATION, 1L`，超时后用 `Language::` 文案提示"网络超时，请检查网络或代理设置"。

#### 4.5 [中等] 翻译请求 URL 用固定 100KB 栈数组 + 无边界 `strcat` 拼接
- 位置：`Function/Translate.cpp:120-150`（百度）、`Function/Translate.cpp:199-229`（有道）
- 现象：
  ```cpp
  char myurl[100000] = "http://api.fanyi.baidu.com/api/trans/vip/translate?";
  char sign[100000] = "";
  ...
  strcat(myurl, UrlEncode(English).c_str());   // 无任何长度检查
  ```
- 影响：
  1. 同一作用域内两个 100KB 栈缓冲区 = 单次调用占 200KB 栈（MSVC 默认栈 1MB），且每次调用都要 `memset` 这 200KB，纯浪费；
  2. `strcat` 无边界检查，而 `UrlEncode` 会把中文的 3 字节 UTF-8 膨胀成 9 字节 `%XX`，即输入超过约 1.1 万个汉字就会**越界写栈**（缓冲区溢出；`/GS` 栈保护只能拦下一部分，且这类崩溃现场极难定位）；
  3. `char salt[60]; sprintf(salt, "%d", rand());`——`rand()` 未调用 `srand()`，每次进程启动产生的 salt 序列完全相同，百度/有道接口用 salt 防重放的意义被削弱。
- 建议：整段改为 `std::string url = "http://api.fanyi.baidu.com/api/trans/vip/translate?"; url += "appid="; url += mBaiduAppid; ...`（或 `std::ostringstream`，先 `reserve(English.size() * 9 + 256)`）；salt 改为 `std::to_string(std::random_device{}())`；MD5 输出那两处 `char tmp[3]`/`buf[33]` 可直接用 `std::string` 累加。

#### 4.6 [轻微] Vulkan 层以异常报错，但异常路径不做任何清理
- 现象：`Vulkan/` 下有 **45 处** `throw std::runtime_error(...)`（`device.cpp` 6 处、`image.cpp` 9 处、`buffer.cpp` 7 处、`swapChain.cpp` 3 处、`pipeline.cpp` 2 处、`shader.cpp` 3 处等），全部靠 `main.cpp:34-41` 的单个 `try/catch` 兜底；`application.cpp:361` 提交命令缓冲失败、`ImGui/Interface.cpp:95` DescriptorPool 失败也走同一路径。
- 影响：异常被捕获后只是记录日志并退出，**不会**调用 `Application::cleanUp()`、不会 `glfwDestroyWindow`/`glfwTerminate`、不会 `Shell_NotifyIcon(NIM_DELETE)`，也不会 `Variable::SaveFile()`——用户会遇到"程序一闪而过/托盘图标残留/设置丢失"。另外这些 `throw` 多在构造函数里，抛出时同函数中已创建的前序 Vulkan 对象（如 `device.cpp:388` 失败前的 `VkInstance`、`image.cpp` 中已分配的 `VkImage`）不会被释放。
- 建议：`catch` 里补一次 `cleanUp()`/托盘清理；更彻底的做法是让 `cleanUp()` 幂等并且可在任何时刻调用，构造函数失败时按 RAII 顺序释放已获取的资源。

#### 4.7 [轻微] SwiftShader 回退依赖硬编码的 Edge 安装路径
- 位置：`Vulkan/instance.cpp:338-366` `swiftShaderCandidateRoots()` 里写死了 5 条路径 `"C:\\Program Files (x86)\\Microsoft\\EdgeCore\\Optimized"`、`"C:\\Program Files\\Microsoft\\EdgeCore\\Optimized"`、`"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application"`、`"C:\\Program Files\\Microsoft\\EdgeWebView\\Application"`、`"C:\\Program Files\\Microsoft\\Edge\\Application"`，再递归搜索 `vk_swiftshader_icd.json` 并 `SetEnvironmentVariableA("VK_ICD_FILENAMES", ...)`。
- 现象与风险：①Windows 装在非 C: 盘、或 Edge 被组策略/精简版系统移除时，这条 CPU 回退路径直接失效；②依赖"Microsoft Edge 会一直随包提供 SwiftShader"这一非承诺行为；③`vk_swiftshader_icd.json` 在 Edge 里是带版本号的深层子目录，递归扫描在大目录树上可能明显拖慢启动；④`VK_ICD_FILENAMES` 已被 loader 1.3.207 起标记为过时（推荐 `VK_DRIVER_FILES`）。
- 建议：用 `%ProgramFiles%`/`%ProgramFiles(x86)%` 环境变量或注册表 `HKLM\SOFTWARE\Microsoft\EdgeUpdate\Clients` 定位安装根目录；找不到时可提示用户手动指定 `vk_swiftshader_icd.json`，或把 SwiftShader 作为可选的随包组件。

### 六、审查期间的交叉核对（父 agent 亲自复核，含对子报告的一处更正）

以下几条我逐行回读原文件确认过，可直接采信；其中第 2 条是对功能层子报告一处结论的**更正**，最终报告中应按更正后的说法写。

1. **已确认**：`Variable.cpp:102-103` 保存有道凭据时写错了变量——
   ```cpp
   iniData->UpdateEntry("YoudaoAPI", "Youdao_ID", BaiduAppid);
   iniData->UpdateEntry("YoudaoAPI", "Youdao_Key", BaiduSecret_key);
   ```
   而读取端 `Variable.cpp:34-35` 用的是 `YoudaoAppid`/`YoudaoSecret_key`。后果：用户在设置里点一次"保存"，`Data.ini` 的 `Youdao_ID`/`Youdao_Key` 就被**百度**的 ID/Key 覆盖，有道凭据永久丢失（`YoudaoAppid` 自己的值从未落盘），此后有道翻译必然失败。修法：把这两行的实参改成 `YoudaoAppid` / `YoudaoSecret_key`。
2. **更正**：功能层子报告称"`Function/Translate.cpp:120-129` 把原始中文 strcat 进 URL，中文翻译必然失败"——**不准确**。该处的 `strcat(sign, English.c_str())`（`Function/Translate.cpp:127`、`:206`）拼的是 **MD5 签名原文**，百度/有道规范本就要求签名用未编码的原文（代码注释也写明了）；真正的查询参数在两处都做了编码：`Function/Translate.cpp:142` 与 `Function/Translate.cpp:221` 均为 `strcat(myurl, UrlEncode(English).c_str())`。因此"中文翻译必然失败"不成立。**真正缺百分号编码的是 WebDAV**：`Function/WebDav.cpp:91` `CURLOPT_URL = Variable::WebDav_url + path`、`Function/WebDav.cpp:250` `Variable::WebDav_url + Variable::WebDav_File + "/" + File`、`Function/WebDav.cpp:362` 同理，中文文件名/目录名会以原始 UTF-8 字节直接发出去（坚果云等服务器对未编码 URI 可能拒绝或 404），且下载回本地的文件名（`Function/WebDav.cpp:249`）也直接落盘。
3. **已确认**：`Function/tesseract.cpp:21-22` 用 `Variable::windows_Width/Heigth` 作为 OCR 位图尺寸，而这两个全局量是 `Tool/Tool.cpp:335-336` 在 `TOOL::screen()` 里**从桌面窗口矩形覆盖**的；`Tool/Tool.cpp:338-340` 只在 `buf == nullptr` 时按当时尺寸 `new char[...]`。调用点 `application.cpp:307` `buffer = TOOL::screen(buffer);` 只分配一次——分辨率/DPI/多屏拓扑变化后，`Tool/Tool.cpp:363-366` 的行拷贝循环会按新尺寸往旧缓冲区里写，**堆越界**。修法：让 `screen()` 自己持有并管理缓冲区（返回 `std::vector<uint8_t>`），或每次调用都按新尺寸重新分配并同步告知 OCR 侧尺寸。
4. **已确认**：`AngelScript/AngelScriptCode.cpp:173-187` 那段 `LoadingDLL("./Opcode/D.dll")` + `GetDLLFunction("ErrorLog")` 是**注释掉的死代码**，当前不存在"从 DLL 动态取函数指针注册给脚本"的运行时行为（若哪天启用，等于把任意 DLL 暴露给 `.as` 脚本，属于需要专门审查的高危改动）。建议直接删除这段注释代码，避免后人误启用。


### 五、可添加功能建议（按性价比排序）

| 建议 | 理由 | 成本 |
|---|---|---|
| GitHub Actions：Windows 上配置 + 编译（缓存 Environment） | 现在"能不能编译"只能靠人工，移植/编码类问题反复出现 | 中 |
| 崩溃转储（`MiniDumpWriteDump` + `SetUnhandledExceptionFilter`） | 用户报的崩溃目前只能靠猜；spdlog 已有日志基础，补 dump 即可定位 | 小 |
| 版本号 + 自动更新检查（读 GitHub Releases） | 现在版本只体现在分支名 `2.0.0`，用户不知道自己是不是最新 | 中 |
| 配置迁移/校验（版本字段 + 缺失键补默认值） | 老 Data.ini 缺新键时行为不确定；配合 1.4 的"用户目录存配置" | 小 |
| 把 `Environment/` 依赖做成 vcpkg.json 或 Conan 清单 | 目前靠百度网盘分享预编译包，新人（和 CI）无法一键复现环境 | 大 |
| 明确的编码规范 + `.clang-format`/`.editorconfig` | 仓库混用制表符与空格、UTF-8 中文注释无保护 | 小 |

### 七、父 agent 对关键结论的逐行实测复核（补充/更正子报告）

以下每条都是父 agent 亲自回读源码（必要时用 `git show HEAD:...` 对照历史版本）确认的，最终报告中应按这里的说法写。

#### 7.1 [更正] Vulkan 层子报告的第 1 条"首帧永久挂起"不成立

- 子报告称"`application.cpp:187` 创建 Fence 用默认 `signaled=false`，`:317` 立刻 `block()` ⇒ 首帧永久挂起"。
- 实测：`Vulkan/fence.h` 的构造签名是 `Fence(Device* device, bool signaled = true);` —— **默认值是 `true`**（HEAD 版与当前工作区完全一致，已用 `git show HEAD:Vulkan/fence.h` 与读取工作区双向核对）。
- 且 `application.cpp:315-383` 的顺序是：等待本帧 fence（`:317`）→ `acquireNextImage` → 重录命令缓冲 → `resetFence()`（`:359`）→ `vkQueueSubmit`（`:361`）→ `present`，这是 Vulkan 的标准写法（先等在途帧、提交前复位）。**首帧不会卡死，该条应删除。**

#### 7.2 [确认] RenderPass 采样数与 ImGui 管线不符（MSAA 半途而废）

- `application.cpp:82` `init_info.MSAASamples = mDevice->getMaxUsableSampleCount();`
- `application.cpp:97-107` RenderPass **只注册了 1 个附件**，且 `finalAttachmentDes.samples = VK_SAMPLE_COUNT_1_BIT`；`:112-118` 只有 1 个 color attachment reference，从未调用 `setResolveAttachmentReference()`；而 `application.cpp:94` 的注释写着"0：最终输出图片 1：Resolve图片（MutiSample） 2：Depth图片" ⇒ RenderPass **是按 3 个附件设计的，实现只写了 1 个**。
- `Vulkan/device.cpp:438-455` `getMaxUsableSampleCount()` 取 `framebufferColorSampleCounts & framebufferDepthSampleCounts` 的最高位，桌面 GPU 上返回 4 或 8 ⇒ ImGui 管线的 `rasterizationSamples`（`ImGui/imgui_impl_vulkan.cpp:833`）与 RenderPass 附件的采样数 1 不符（验证层报错；关掉验证层属未定义行为，驱动通常容忍）。
- **结论性修法（二选一）**：(a) 补齐 RenderPass 的 MSAA + resolve + 深度附件（SubPass 要调 `setResolveAttachmentReference()`，并把 MSAA/深度图的布局转换与 `VkSubpassDependency` 补全）；(b) **直接取消 MSAA**——`init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;`、RenderPass 保持单附件、FrameBuffer 只绑 1 个 view、不再创建 `mMutiSampleImages`/`mDepthImages`。对一个纯 2D 的 ImGui 界面，(b) 更简单且没有画质损失（ImGui 自带抗锯齿）。

#### 7.3 [确认·历史遗留] FrameBuffer 绑了 3 个附件，RenderPass 只有 1 个

- `Vulkan/swapChain.cpp:181-185` `std::array<VkImageView, 3> attachments = { mSwapChainImageViews[i], mMutiSampleImages[i]->getImageView(), mDepthImages[i]->getImageView() };`，`:190` `frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());`（= 3），调用点 `application.cpp:56`。这违反 VUID-VkFramebufferCreateInfo-attachmentCount-00816（attachmentCount 必须等于 RenderPass 声明的附件数）。
- `git show HEAD:Vulkan/swapChain.cpp` 显示 HEAD 版**同样是 3 个附件**、同样无条件创建 MSAA/深度图 ⇒ **不是本次设备选择移植引入的，属长期存在的误用**；程序一直能跑，说明当前驱动容忍了它（且验证层是关着的，见 05 报告）。
- 处理方式与 7.2 合并：要么把 RenderPass 补成 3 附件，要么把这里改成 1 附件。

#### 7.4 [确认·移植新增分支] `sampleCount == 1` 时空指针解引用

- `Vulkan/swapChain.cpp:153-173`：只有 `sampleCount != VK_SAMPLE_COUNT_1_BIT` 才创建 MSAA 图，否则 `mMutiSampleImages[i] = nullptr;`（`:171`），而 `:183` **无条件**调用 `mMutiSampleImages[i]->getImageView()` ⇒ 在 `getMaxUsableSampleCount()` 返回 `VK_SAMPLE_COUNT_1_BIT` 的设备（部分软件/虚拟 GPU）上必然崩溃。
- HEAD 版是无条件创建、不存在这个分支 ⇒ 该分支是**移植 PixelClean 新版 `swapChain.cpp` 时新引入的**（虽然桌面独显上返回 4/8 所以没暴露）。按 7.2(b) 取消 MSAA 即可一并消除。

#### 7.5 [确认] 工具层与配置层两条最严重项

- `Tool/Tool.cpp:252-276` `ClipboardTochar()` 声明返回 `std::string`，但 `while (ClipboardBoll > 0)` 里 5 次尝试全部失败时（`OpenClipboard` 失败 `:256-261`、`GetClipboardData` 返回 NULL `:264-269`）只 `continue`，循环结束后**直接掉出函数末尾，没有任何 return** ⇒ 未定义行为（MSVC 会报 C4715；实际多返回未初始化的 `std::string`，析构时崩溃）。附带缺陷：`:259`/`:267` 在 `OpenClipboard` 失败后仍调用 `CloseClipboard()`；`:271` `std::string CharS = (char*)GlobalLock(hmem);` 未判断 `GlobalLock` 返回 NULL；取到指针后**缺少 `GlobalUnlock(hmem)`**。
- `ini.h:480-488` `UpdateEntry` 用 `if (!_values[section][name].size())` 判断"键是否存在"，判据实际是**当前值的长度**：值为空串（`Data.ini` 里写成 `Key=`）与键不存在无法区分，两种情况都抛 `runtime_error("key '...' not exist in section '...'.")`；`:490-499` 的 vector 重载同理。这正是设备选择任务里 `VulkanDeviceName` 需要单独打补丁的原因（`Variable.cpp` 现在按"值是否为空"在 `InsertEntry`/`UpdateEntry` 之间选择），其余 60+ 个键仍是同样裸奔的状态（见 04 报告第 2 条）。

#### 7.6 [确认] 功能层两条被点名的严重项，以及一处需要收敛的说法

- **凭据裸指针悬垂（确认）**：`Function/Translate.h:50-54` 用 `const char* mBaiduAppid; const char* mBaiduSecret_key; const char* mYoudaoAppid; const char* mYoudaoSecret_key;` 保存凭据，`Function/Translate.h:14,17` 的 `SetBaiduAppID(const char* appid) { mBaiduAppid = appid; }` 只存指针（非拥有）；`ImGui/Interface.cpp:1304-1307` 在点"保存"时重新赋值 `Variable::BaiduAppid = SetBaiduID;` 等 `std::string` —— 密钥长度通常超过 15 字符（如 `cos4RtAbGDCDhvgWvGSPu`），赋值会**释放旧堆缓冲并重新分配**，此后 `mTranslate` 里那几个 `const char*` 就是悬垂指针，下一次调用在 `Function/Translate.cpp:126,140,205,219` 的 `strcat(sign, mBaiduAppid)` / `strcat(myurl, mYoudaoAppid)` 处读到已释放内存。修法：这 4 个成员改成 `std::string`，或在保存后重新 `SetBaiduAppID(Variable::BaiduAppid.c_str())`。
- **WebDAV 节点链零判空（确认）**：`Function/WebDav.cpp:114-117`：`root->first_node("d:multistatus")` 之后 `for (...)` 里直接 `node->first_node("d:propstat")->first_node("d:prop")->first_node("d:getcontenttype")->value()`，任何一环返回 nullptr 即崩溃（服务器返回非预期结构、错误页 XML、或 207 之外的状态码时都可能触发）；`:111` `doc->parse<0>(&response[0])` 未捕获 rapidxml 的 `rapidxml::parse_error`，畸形 XML 会一路冒到 `main.cpp:34-41` 弹窗并结束进程。
- **需要收敛的说法**：03 报告称"`doc` 是 `new` 后不 `delete`"——不准确，`Function/WebDav.cpp:131` 有 `delete doc;`（正常路径会释放）。真正的漏洞是：解析抛异常或中途 `return` 时泄漏，以及上面那条未捕获异常。最终报告按此写法。

#### 7.7 [更正] 工具层子报告第 8 条后半段"开机自启判断写反"不成立

- 子报告称"`SetModifyRegedit` 返回 true 被当失败 ⇒ 取消勾选反而写 `HKCU\...\Run`"。
- 实测 `Tool/Tool.cpp:90-116`：函数**成功返回 true、失败返回 false**，且 `if (Bool)` 分支写 `RegSetValueEx`、`else` 分支 `RegDeleteValue` —— **注册表行为与勾选状态一致，没有写反**。
- 真正的缺陷是 `ImGui/Interface.cpp:1366-1368` 的**日志条件反了**：
  ```cpp
  if (TOOL::SetModifyRegedit("TranslatorKyi", Variable::Startup)) {
      TOOL::logger->error("SetModifyRegedit(): Error");
  }
  ```
  即"成功时记 error、失败时一声不吭"。修法：改成 `if (!TOOL::SetModifyRegedit(...))`。
- `Tool/Tool.cpp:57-71` `StrName` 的另外两条确认无误：①`for (size_t i = Str.size() - 1; i > 0; i--)` 在 `Str` 为空串时 `Str.size() - 1` 等于 `SIZE_MAX`，随即 `Str[i]` 越界（UB）；②循环只认反斜杠 `'\\'`，遇到正斜杠路径（如 `ImGui/Interface.cpp:1347` 里那种写法）不会更新 `xieI`，返回值是被截断的路径片段而不是文件名，`Interface.cpp:746/748` 的字面量比较随之失配、`Index` 静默停在 0。

#### 7.8 [确认] `Window` 对象被三处代码同时"拥有"，正常退出路径必然二次释放

界面层报告把这条列为它的第 1 条严重项，我逐行核对了所有权链：

- 唯一的创建点：`main.cpp:30` `VulKan::Window* mWin = new VulKan::Window(app->mWidth, app->mHeight, 0, 0);`
- `application.cpp:28` `mWindow = w;`（只是保存引用，注释掉的 `:40` 才是原设计里的 `new`），而 `Application::run()` 的末尾是 `application.cpp:32-33`：`mainLoop();` → `cleanUp();`，`Application::cleanUp()` 在 `application.cpp:404` 执行 `delete mWindow;`
- 回到 `main.cpp:43` 又执行 `delete mWin;` —— **同一个对象被 delete 两次**。`Window::~Window` 会 `glfwDestroyWindow()` + `glfwTerminate()`，第二次析构在已释放内存上进行（UB），实际表现为"退出时偶发崩溃/堆损坏"，Debug 下会直接触发堆断言。修法：只保留一处所有权（建议 `main.cpp` 用 `std::unique_ptr` 持有，`cleanUp()` 里不再 delete 窗口，`Application` 只留裸观察指针）。
- 第三条路径：`ImGui/Interface.cpp:1374` `delete mWindown;`（`mWindown` 来自 `Interface.cpp:15` `mWindown = Win;`）在"改了字体/字号后保存"的 `updata` 分支里删掉同一个窗口，紧接着 `ShellExecute` 重启自身并 `exit(0)`（`:1375-1378`）。因为是 `exit(0)` 立刻结束，这条路径**没有**二次释放，但**整个 `cleanUp()` 被跳过**（Vulkan 的 Instance/Device/SwapChain/Surface 全不回收），属于"重启即泄漏"；而且 `delete` 一个自己不拥有的对象本身就是所有权错误，应改成"设置退出标志 + 让 `run()` 正常返回"。

---

#### 7.9 [更正 界面层#6] 命令池**带** `RESET_COMMAND_BUFFER_BIT`，"缺少 RESET 标志"不成立（降级为轻微）

- 界面层报告的第 6 条写的是"命令池无 `RESET_COMMAND_BUFFER_BIT`，却每帧对同一二级缓冲重新 `begin()`，违反 initial-state VUID"。**该理由不成立**：`Vulkan/commandPool.h:8` 的构造函数签名是 `CommandPool(Device* device, VkCommandPoolCreateFlagBits flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);`，默认值就是 RESET 标志，而 `ImGui/Interface.cpp:143 ImGuiCommandPoolS[i] = new VulKan::CommandPool(mDevice);` 用的是默认参数 ⇒ **池是带 RESET 标志创建的**。
- 代码确实从不显式重置：`ImGui/Interface.cpp:227-230` 每帧直接
  `ImGuiCommandBufferS[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, info);` → `ImGui_ImplVulkan_RenderDrawData(...)` → `->end();`，
  全文件没有一处调用 `reset()`，而 `Vulkan/commandPool.h:14-18` 已经写好了 `void reset() { vkResetCommandPool(...); }`（**目前无人调用**）。
- 但这不构成"违规"：`vkBeginCommandBuffer` 的约束只禁止从 **recording / pending** 状态开始（`VUID-vkBeginCommandBuffer-commandBuffer-00049`），并要求"池未带 RESET 标志时缓冲必须处于 initial 状态"（`VUID-vkBeginCommandBuffer-commandBuffer-00050`）——本项目的池带 RESET 标志，且每帧提交后都 `vkWaitForFences`（`application.cpp:317`）保证上一帧执行已结束，所以"每帧重录同一二级命令缓冲"是合法且常见的写法（Sascha Willems 的 secondary buffer 示例即如此）。
- **建议保留为轻微改进**：在 `Interface.cpp:227` 之前显式调用 `ImGuiCommandPoolS[i]->reset();`（复用已有的 `reset()`），让意图明确、不依赖状态推断，也顺便让这个已实现却闲置的接口派上用场。

---

### 八、对分项报告里"待确认"条目的结案

分项报告中共 6 处标了"待确认"，其中 5 处不需要编译就能定论；本节是主审逐行回读后的结案，**与分项报告正文冲突时以本节为准**。

#### 8.1 [更正 配置层] "全局量没有初始化器"不等于"读到不确定值"

- `Variable.cpp:169-226` 那一批（`windows_Width`、`BaiduAppid`、`HitokotoFontSize`、`MakeUp`、`FontSize`、`Startup`、`ScreenshotColor[4]` …）全部是**命名空间作用域的定义**，具有静态存储期，在 `main()` 之前一定被零初始化：`int` = 0、`bool` = false、`std::string` = 空串、`unsigned char[4]` = 全 0。所以"读取不确定值"这个结论不成立。
- 真正的风险是"**0 / 空串被当成有效配置**"：`FontSize = 0` 会让 `kuangshu / int(FontSize)` 整数除零（已列入 G 表）；`ReplaceLanguage = 0`、`Model = 0` 会静默选中列表第 0 项；`Language = ""` 会让 `Language::ReadFile("")` 去开 `./Language/.ini` 并失败后继续跑。修法应从"补初始化器"改为"**给每个键定义合法范围并在读取时钳制**"（这也是配置层报告第 3 条建议）。

#### 8.2 [结案 工具层 待确认①] `SetModifyRegedit` 每次保存都会调用，且读到的是最新值

- `ImGui/Interface.cpp:1366` 位于"保存"按钮处理体的顶层，前面的 `if` 只与字体/字号相关（`:1353`、`:1362`），**不是**"仅当开机自启变化时"才调用 ⇒ 每次保存都会执行一次写/删注册表键，行为幂等，无副作用。
- `Variable::Startup` 是**直接绑定**到复选框的（`ImGui/Interface.cpp:991 ImGui::Checkbox(Language::Startup.c_str(), &Variable::Startup);`），不存在局部副本，所以 `:1366` 传的是当前最新值。
- 因此这一条唯一确认的缺陷仍是**日志条件反了**：成功记 `error`、失败静默（见 7.7）。

#### 8.3 [结案 工具层 待确认④] `Tool.h` 的 `extern const` **不构成链接问题**

- `Tool/Tool.cpp:1` 先 `#include "Tool.h"`，头文件 `Tool/Tool.h:61-62` 的 `extern const int number;` / `extern const double miao_time;` 已把这两个名字声明为**外部链接**；随后同一 `namespace TOOL` 内的定义 `Tool/Tool.cpp:379 const int number = 60;` 与 `Tool/Tool.cpp:380 const double miao_time = (number + 1) * 1000;` 就是同一个实体的定义，定义与声明一致。结论：**能正常链接，不是雷**（若把定义挪到不包含 `Tool.h` 的 TU，才会退化成内部链接 + 链接失败）。
- 同一节里另外两条仍然成立：`Tool/Tool.h:41/43` 的注释与函数对调（"string 转 wstring"挂在 `ws2s` 上）；`Tool/Tool.h:21-22` 只声明 `Converter<T>` 而定义在 `Tool/Tool.cpp:21-33`，任何其他 TU 实例化它都会链接失败（当前无人调用）。

#### 8.4 [结案 配置层 待确认④] 语言文件永远不会被程序写回

- 全仓 `write_Gai` 只有一处调用：`Variable.cpp:162 inih::INIWriter::write_Gai(IniPath, *iniData);//保存`，而 `IniPath` 是 `FilePath.h:3` 里的宏（`"Data.ini"`）⇒ `Language/zh.ini`、`Language/eng.ini` **只读不写**。
- 所以 `eng.ini:30` 的 `Confirm_= Confirm`（`=` 前少空格）、`RenderDeviceItem_` 的全/半角括号差异、`eng.ini` 的 `RenderDeviceMissing_` 值里带双引号，都不会被"下次保存破坏"，属于**纯一致性问题**。反过来也说明：**新增语言键必须手工同时改 zh 与 eng 两份**，程序不会帮你补。

#### 8.5 [确认·从"待确认"升级为确定] 截图纹理的图像格式与视图格式不一致

- `ImGui/Interface.cpp:1535 info.format = VK_FORMAT_R8G8B8A8_UNORM;`（创建 `VkImage`）与 `ImGui/Interface.cpp:1566 info.format = VK_FORMAT_B8G8R8A8_UNORM;`（创建 `VkImageView`）不一致，且该图像未使用 mutable format ⇒ 违反 `VUID-VkImageViewCreateInfo-image-01762`。
- 缓冲里的数据来自 GDI 截图（`Tool/Tool.cpp` 的 `screen()` 输出 BGRA，`ImGui/Interface.cpp:1520-1526` 只设 `Channels = 4`），所以**正确修法是把 `:1535` 改成 `VK_FORMAT_B8G8R8A8_UNORM`**，而不是改视图。无校验层时属于"未定义但常见驱动容忍"，可视表现是红蓝通道互换。

#### 8.6 [结案 配置层/工具层 的线程类待确认] 当前**不存在**跨线程访问

- 全仓 `std::thread` / `CreateThread` / `std::async` / `std::mutex` **零出现**（横向扫描结论）；`ImGui/Interface.cpp` 里的 `RunFunction` 调用、`Vulkan/instance.cpp:258-287` 与 `:381-383` 对 `Variable::VulkanDetectedDevices` / `CpuSoftwareRenderReason` 的写入、以及 ImGui 的读取，全部在同一个 GLFW 主线程上。
- 结论：这两条"待确认"**降级为轻微/将来式**——当前无竞态；但一旦引入工作线程（这是本报告推荐的第 1 项架构改进），`AngelScriptCode.h:13-18` 的懒汉单例与 `Variable` 命名空间的裸全局量都**必须先加同步**。

#### 8.7 [结案 配置层 待确认⑤] `cmake -E copy` 会无条件覆盖输出目录里的 `Data.ini`

- `CMakeLists.txt:53 execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})`：`cmake -E copy` 在目标同名文件存在时**直接覆盖**（只有目标目录不存在或复制失败才报错），所以"在输出目录里手改 `Data.ini`、下次 configure 被源码目录版本盖掉"这条**成立**；叠加"`execute_process` 只在 configure 阶段执行"（改了源码 `Data.ini` 不重新 configure，输出目录还是旧的）。
- 实用结论：**源码目录的 `Data.ini` 是唯一真源**；用户在界面上保存改的是运行目录（输出目录）里那份，重新 configure 就会回退——这既解释了"设置莫名恢复默认"，也是"配置应存到 `%APPDATA%`"这条建议的依据。

#### 8.8 [补充·配置层] 两个实例会互相覆盖设置

- `Variable::SaveFile()` 不是"基于内存态写回"，而是**重新解析磁盘上的当前文件**再整体重建（`Variable.cpp:162` → `ini.h:585-599`）⇒ 同一份 `Data.ini` 被两个进程先后保存时，后写者会用自己那份内存设置覆盖前者的全部改动。
- `main.cpp:17-21` 的单实例检查只按窗口标题调用 `FindWindow`，标题一变（或进程已进入重启分支）就挡不住。当前没有任何文件锁/互斥量。

#### 8.9 [结案 核心层 待确认④] "同步对象按 `mCurrentFrame` 轮转"**没有**越界风险

- 分配侧：`application.cpp:179-190 createSyncObjects()` 的循环上界就是 `mSwapChain->getImageCount()`，三个数组 `mImageAvailableSemaphores` / `mRenderFinishedSemaphores` / `mFences` 各生成 `imageCount` 个元素。
- 使用侧：`application.cpp:383 mCurrentFrame = (mCurrentFrame + 1) % mSwapChain->getImageCount();`，取模基数与数组长度**同一个来源**（`getImageCount()`），且 `application.cpp:317/327/342/355/359/360` 全部以 `mCurrentFrame` 索引这三个数组 ⇒ 下标恒在 `[0, imageCount)`，**不存在越界**。
- 用 `mImageAvailableSemaphores[mCurrentFrame]` 同时作为 acquire（`application.cpp:327`）与 submit 的等待信号量（`:342`）也是自洽的：同一帧里 acquire 发信号、submit 等待，且每个槽位在下一次复用前都先过了 `application.cpp:317 mFences[mCurrentFrame]->block()`。
- 因此这一项从"待确认"改为**确认无缺陷**；若要挑毛病，只剩"交换链重建时 `getImageCount()` 变化需要重建这些数组"这一条——而 `StructureSwapChain()` 目前在 `Vulkan/swapChain.cpp:19` 只被构造函数调用、`mWindowResized` 也无人读取（见核心层报告），所以重建路径本身根本不会触发。



---

## 应用核心与生命周期 · 体检报告

**审查对象**：`main.cpp`、`application.cpp`、`application.h`、`base.h`、根 `CMakeLists.txt`、`resource.rc`、`FilePath.h`、`DebugLog.h`、`texture/texture.{h,cpp}`、`texture/CMakeLists.txt`；并按契约读了 `Vulkan/Window.{h,cpp}`、`Vulkan/swapChain.h`、`Vulkan/device.h`、`ImGui/Interface.{h,cpp}`、`Tool/Tool.{h,cpp}`、`Variable.{h,cpp}`、`ini.h`、`Data.ini`（不含 `Vulkan/` 内部实现）。
**方法与限制**：本机 `Environment` 目录不存在（GLFW/Vulkan SDK 缺失）且未安装 cmake，**没有任何编译/运行实测**；全部结论来自逐行精读 + 行号复核（每条引用都用 read/grep 校对过）+ 字节级编码检查。`git diff HEAD` 显示 43 个改动文件全在 `Vulkan/` 下，`main.cpp`、`application.h`、`application.cpp` **与 HEAD 完全一致**——下文绝大多数问题属于既有状态，不是本次移植引入；与移植相关的只有第 5、14 条。存疑处标"待确认"并写明确认条件。
**严重程度**：`[严重]` 崩溃/数据损坏/功能不可用；`[中等]` 特定条件下出错或明显劣化；`[轻微]` 风格与整洁度。

### 一、缺陷与风险

#### [严重] 同一个 `Vulkan::Window` 有三个所有者：能退出的路径必然 double free，实际在用的路径又完全不清理
- 位置：`main.cpp:30`、`main.cpp:43`、`application.cpp:404`、`ImGui\Interface.cpp:1374`
- 现象：`main.cpp` 创建并 final delete 窗口，`Application::cleanUp()` 又 delete 同一指针，设置界面"确认并重启"再 delete 第三次；而 `Window::~Window()` 里是 `glfwDestroyWindow` + `glfwTerminate`，对已释放对象再调一次就是往已回收内存里写。
- 证据：
```
main.cpp:30   VulKan::Window* mWin = new VulKan::Window(app->mWidth, app->mHeight, 0, 0);
main.cpp:43   delete mWin;
application.cpp:28/404   mWindow = w;  ...  delete mWindow;      // 与 main.cpp:43 同一指针
Interface.cpp:1374       delete mWindown;                       // 第三处，随后 exit(0)
Window.cpp:108-110       Window::~Window() { glfwDestroyWindow(mWindow); glfwTerminate(); }
```
- 现象（另一条更现实的路）：主窗口是隐藏窗口（`Vulkan\Window.cpp:63 glfwHideWindow`）；`processEvent()`（`Vulkan\Window.cpp:129`，内含 ESC→`exit(0)`）**在任何 .cpp 里都没有被调用**，全仓库也没有 `glfwSetWindowShouldClose`，所以 `while (!mWindow->shouldClose())`（`application.cpp:196`）无法由用户结束 → `cleanUp()` 与 `main.cpp:43` 事实上都跑不到。用户能用的退出是菜单"退出"→`exit(0)`（`ImGui\Interface.cpp:1438`）与重启（`1378`）；`exit()` 不展开栈，于是 `cleanUp()`、`~ImGuiInterFace`、`~Application` 全被跳过，Vulkan 实例/设备/交换链/描述符池/纹理一个都不释放，全靠进程退出回收。**两条退出路都不对**：一条会 double free（让它可达就立刻命中），一条什么都不清（现在实际走的就是这条）。
- 建议修法：所有权单一化，`Application` 只借用窗口，窗口由 `main` 用 RAII 持有：
```cpp
// main.cpp：不再手写 delete；单实例检测/ini/日志一并移进 try（见第 2 条）
try {
    auto app = std::make_unique<GAME::Application>();
    auto win = std::make_unique<VulKan::Window>(app->mWidth, app->mHeight, 0, 0);
    win->setApp(app.get());  app->run(win.get());
} catch (const std::exception& e) { /* 统一报错，见第 12 条 */ }
```
  同时删掉 `application.cpp:404` 的 `delete mWindow;`，把 `Interface.cpp:1374` 改成"请求重启"标志（`RequestRestart = true`），由主循环退出后统一 `cleanUp()` 再 `ShellExecute` 自己。

#### [严重] 启动期异常逃逸出 `try`：Data.ini / 语言文件缺失或损坏 → 静默崩溃，无任何提示
- 位置：`main.cpp:23-26`（try 从 `main.cpp:34` 才开始）、`ini.h:302`/`344`/`362`、`Variable.cpp:240-241`
- 现象：`Variable::ReadFile`、`Language::ReadFile`、`SpdLogInit` 都在 `try` 之外。`ini.h` 的**两参** `Get` 在文件不存在、section/key 缺失时直接抛异常（只有三参重载返回默认值），而 `Language::ReadFile` 通篇用两参 `Get`；`FilePath.h` 的 `iniData "Data.ini"` 是**相对当前工作目录**的路径。于是只要 Data.ini 不在 CWD（快捷方式"起始位置"被改、从别的目录启动、将来换多配置生成器导致资源不在 exe 旁），启动即抛 `std::runtime_error` → 无人接 → `std::terminate`；Release 又是 `/SUBSYSTEM:WINDOWS`（`CMakeLists.txt:113`），连控制台都没有，用户只看到"双击没反应"。代码自己的注释也承认这是老坑（`main.cpp:2-6`）。
- 证据：
```
main.cpp:23   Variable::ReadFile(iniData);
main.cpp:24   Language::ReadFile(Variable::Language);
main.cpp:34   try {                        // 上面三行在保护之外
ini.h:302     throw std::runtime_error("ini file not found.");
Variable.cpp:241  inih::INIReader iniData = inih::INIReader("./Language/" + FilePath + ".ini");
```
- 建议修法：① 把 `main.cpp:23-26` 整体移进 `try`；② 启动配置一律走**三参** `Get`（缺键取默认值，`Variable.cpp:65-72` 已有先例并写了注释）或给 `ReadFile` 包一层 try 后弹窗提示"配置损坏，已用默认值"；③ 用 `GetModuleFileName` 取 exe 目录做基准，把 `FilePath.h` 的相对路径改成绝对路径拼接。

#### [严重] `InterFace->eng/zhong`（各 1MB 定长缓冲）的 `memcpy` 没有任何长度上限
- 位置：`application.cpp:259-262`，缓冲定义在 `ImGui\Interface.h:212-213`
- 现象：把剪贴板取词结果与翻译结果整段拷进对方的 1MB 数组，既不检查 `size()`，也不给结尾 NUL 留位置。超长剪贴板内容（整篇文档、OCR 结果、脚本返回值）会溢出到相邻成员乃至堆；恰好 1MB 时结尾 NUL 被挤掉，随后 `ImGui::InputTextMultiline(..., eng, IM_ARRAYSIZE(eng))` 会读到缓冲区外。
- 证据：
```
application.cpp:259  memset(InterFace->eng, 0, sizeof(InterFace->eng));
application.cpp:261  memcpy(InterFace->eng, Variable::eng.c_str(), Variable::eng.size());
application.cpp:262  memcpy(InterFace->zhong, Variable::zhong.c_str(), Variable::zhong.size());
ImGui\Interface.h:212-213  char eng[1024 * 1024];  char zhong[1024 * 1024];
```
- 建议修法：加边界检查并给出可见反馈（优于静默截断）：
```cpp
auto copyCapped = [](char* dst, size_t cap, const std::string& src) -> bool {
    const size_t n = src.size() < cap - 1 ? src.size() : cap - 1;   // 至少留一个 NUL
    std::memcpy(dst, src.data(), n);  dst[n] = '\0';  return n == src.size();
};
if (!copyCapped(InterFace->eng, sizeof(InterFace->eng), Variable::eng)) { /* 提示"文本过长，已截断" */ }
```
  更长远的修法是把 `eng/zhong` 换成 `std::string` + `ImGuiInputTextFlags_CallbackResize`（ImGui 官方推荐做法），本条的溢出与下一条的爆栈会一起消失。

#### [严重] 输入回调里对 `eng` 的读写同样无边界检查：栈缓冲 `selected_text[10000]` 可被写爆，且已有一处 off-by-one
- 位置：`ImGui\Interface.cpp:277-283`（主路径）、`ImGui\Interface.cpp:248-255`（off-by-one）
- 现象：`InputTextMultilineText()` 用 `Len = mTextLen - mCursorPos` 从 1MB 的 `eng` 往 10000 字节的栈数组拷贝，`Len` 无上限；翻译框文本超过 10000 字节时按 Ctrl+V 即栈溢出（覆盖返回地址 → 崩溃/可被利用）。`MyText` 回调的 `selected_text[(end - start) + 1] = '\0'` 比拷贝长度多写 1 字节（正确写法 `[end - start]`），选区达 9999 字符就越界 1 字节。两条都在主循环调用链上（`mainLoop → InterFace() → TranslateInterface → InputTextMultilineText`），属本次审查的"取词/输入"路径。
- 证据：
```
Interface.cpp:277  char selected_text[10000];
Interface.cpp:278  int Len = mTextLen - mCursorPos;
Interface.cpp:279  memcpy(selected_text, &eng[mCursorPos], Len);
Interface.cpp:251  selected_text[(data->SelectionEnd - data->SelectionStart) + 1] = '\0';
```
- 建议修法：短期至少 `Len = std::min(Len, (int)sizeof(selected_text) - 1);`、去掉 `+ 1`、把 `selected_text` 换成 `std::vector<char>`(1MB) 或 `static` 缓冲；根治方案同上条（`CallbackResize` + `std::string`）。这条严格说属 ImGui/输入子系统，但它与 `application.cpp:259-262` 是同一块 eng/zhong 契约，应一起改。

#### [中等] `vkAcquireNextImageKHR` 返回值被完全忽略，错误分支会让渲染循环永久卡住；`mWindowResized` 只写不读
- 位置：`application.cpp:322-329`、`application.cpp:380`；`Vulkan\Window.cpp:31`（写者）、`Vulkan\Window.h:40`（定义）
- 现象：① `result` 不判空不判错，遇 `VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR / VK_ERROR_SURFACE_LOST_KHR` 时 `imageIndex` 保持 L322 的初值 0，而 `mImageAvailableSemaphores[mCurrentFrame]`（L327）根本没被 signal；L342 又把它当等待信号量提交 → 该提交永不完成 → L317 的 `mFences[mCurrentFrame]->block()` 下一帧永久阻塞 → 界面卡死（不是崩溃，更难查）。② `vkQueuePresentKHR` 的返回值赋给 `result` 后**从未使用**（死存储），present 失败无人知。③ `windowResized` 回调只置 `pUserData->mWindowResized = true`，**全仓库没有任何读取点**，`SwapChain::StructureSwapChain()` 也只在 `Vulkan\swapChain.cpp:19` 的构造函数里调用——移植过来的 Vulkan 层已把 `getWidth/getHeight`、`StructureSwapChain` 备好，应用层没接线。触发条件：改分辨率/DPI 缩放变化/显示器热插拔会重设隐藏主窗口的 framebuffer 尺寸；主窗口是隐藏的 1×1 窗口，**拖拽改大小的路径不存在**，故日常概率低，但一旦触发就是死等。
- 证据：
```
application.cpp:322  uint32_t imageIndex{ 0 };
application.cpp:323  VkResult result = vkAcquireNextImageKHR( ... &imageIndex);
application.cpp:380  result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);//开始渲染
Vulkan\Window.cpp:31  pUserData->mWindowResized = true;
```
- 建议修法：抽出重建函数，并把 acquire/present 两处返回值都接上分支：
```cpp
void Application::recreateSwapChain() {
    vkDeviceWaitIdle(mDevice->getDevice());
    for (auto* c : mCommandBuffers) delete c;  mCommandBuffers.clear();
    mSwapChain->StructureSwapChain();                       // 已实现的入口
    createRenderPass();  createFrameBuffers(mRenderPass);    // 帧缓冲随 extent 变化
    mCommandBuffers.resize(mSwapChain->getImageCount());
    for (int i = 0; i < (int)mCommandBuffers.size(); ++i) mCommandBuffers[i] = new VulKan::CommandBuffer(mDevice, mCommandPool);
    mWindow->mWindowResized = false;  mCurrentFrame = 0;
}
// render()：acquire 后与 present 后都判 result / mWindow->mWindowResized → recreateSwapChain()；
// if (result != VK_SUCCESS) return;   // 拿不到图像就不要提交（信号量没被 signal，Fence 会永久等待）
```

#### [中等] 计时全部基于 `clock()`（进程 CPU 时间），而配置单位是毫秒墙钟时间 → "显示时间"类设置失真
- 位置：`application.cpp:195`、`217`、`219`；`ImGui\Interface.h:91/101/105/139/151`
- 现象：`clock()` 返回进程消耗的 **CPU 时间**而非墙钟。主循环每轮先 `sleep_for(10ms)`（`application.cpp:198`），空闲时每轮只烧零点几毫秒 CPU，于是 `clock() - TranslateTime > Variable::DisplayTime`（`Interface.h:139`，`DisplayTime=5000`）在墙钟上被放大数倍到数十倍：面板"该消失时不消失"。方向还会反转：OCR/翻译把 CPU 打满时 `clock()` 快于墙钟，面板提前消失。具体倍率取决于每帧真实 CPU 占用，**待确认**（确认条件：能运行后打印每 10 秒墙钟对应的 `clock()` 增量）。另：`clock_t` 在 MSVC 是 32 位 `long`、`CLOCKS_PER_SEC=1000`，累计约 24.8 天 CPU 时间后回绕（风险低，附带提及）。
- 证据：
```
application.cpp:195  clock_t HitokotoTime = clock();
application.cpp:217  ... && (clock() - HitokotoTime > 20000)) { ... }
ImGui\Interface.h:139  if ((clock() - TranslateTime) > time && (time != 0)) {
ImGui\Interface.h:211  clock_t TranslateTime;//显示时间      <-- 且未初始化
```
- 建议修法：统一改 `std::chrono::steady_clock`（顺手解决 `TranslateTime` 未初始化）：
```cpp
using Clock = std::chrono::steady_clock;
static Clock::time_point HitokotoTime = Clock::now();
const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - HitokotoTime).count();
if (Variable::PopUpNotificationBool && InterFace->GetInterFaceEnum() == No_Enum
    && elapsedMs > Variable::HitokotoTimeInterval) {           // 见下一条
    InterFace->SetInterFace(HitokotoEnum);  HitokotoTime = Clock::now();
}
```

#### [中等] 配置项被硬编码覆盖：一言间隔固定 20 秒、一言显示时长固定 10 秒
- 位置：`application.cpp:217`（`> 20000`）、`ImGui\Interface.h:74`（`return 10000;`）
- 现象：`Data.ini` 里有一言间隔与显示时长，变量、读写代码、设置界面**全都有**，但主循环与界面超时用的是硬编码数字，用户在 ini 或设置界面改的值被静默忽略。间隔恰好被写成 20000 而 ini 是 60000，差距肉眼可见。
- 证据：
```
Data.ini:3-4      HitokotoTimeInterval = 60000 / HitokotoDisplayDuration = 10000
Variable.h:44-45  extern int HitokotoTimeInterval; extern int HitokotoDisplayDuration;
Interface.cpp:1247-1248  ImGui::InputInt(Language::HitokotoTimeInterval.c_str(), &SetHitokotoTimeInterval);
application.cpp:217  ... && (clock() - HitokotoTime > 20000)) {
ImGui\Interface.h:74   return 10000;
```
- 建议修法：`application.cpp:217` 改用 `Variable::HitokotoTimeInterval`；`Interface.h:74` 返回 `Variable::HitokotoDisplayDuration`（单位随上一条一起改成毫秒墙钟）。

#### [中等] 热键用 `std::string` 存单字符并按 `[0]` 取 VK 码：不是越界，但只在大小写/命名恰好巧合时成立
- 位置：`application.cpp:236/237/269/270/306/311`；`Variable.h:66-67`；默认值见 `Data.ini:43-46`（`MakeUp=18 Screenshotkey=D Choicekey=Q Replacekey=R`）
- 核实结论（逐条回答"是否越界/是否初始化"）：
  - **不是越界**：`std::string::operator[](0)` 在空串时返回指向结尾 NUL 的引用（C++11 起读取行为有定义），只会让 `GetKeyState(0)` 恒为 0 → 热键静默失效，不会崩。
  - **依赖巧合**：大写字母 ASCII 恰好等于 VK 码（`'D'=0x44=VK_D`），默认配置能用；设置界面也刻意 `toupper` 兜住（`Interface.cpp:1311-1312`）。
  - **手改 ini 即失效或串键**：小写 `d` → `0x64=VK_NUMPAD4`（小键盘 4），`"F1"` → `'F'=VK_F`，`"Space"` → `'S'=VK_S`。
  - `MakeUp` 是 `int`（`Variable.h:65`），用 `GetKeyState(...) < 0` 判高位"按下"是**正确写法**（`GetKeyState` 低位是 CapsLock 类开关状态，不适用于按下判定），这两处不是 bug。
- 证据：
```
application.cpp:306  if ((GetKeyState(Variable::Screenshotkey[0]) < 0) && ((GetKeyState(Variable::Screenshotkey[0]) < 0) != mButton)) {
application.cpp:311  mButton = (GetKeyState(Variable::Screenshotkey[0]) < 0);
Interface.cpp:1311   Variable::Screenshotkey = toupper(SetScreenshotkey[0]);
Variable.h:66        extern std::string Screenshotkey;//截图
```
- 建议修法：ini 存**虚拟键码**（`Screenshotkey = 68`）或键名表，成员改 `int`；过渡期至少 `if (k.empty()) return;` + `toupper(static_cast<unsigned char>(k[0]))` + 只接受 `[A-Z0-9]`，并在设置界面提示合法取值。

#### [中等] 忙等阻塞主循环，并且用固定 `Sleep(5)` 赌剪贴板已更新
- 位置：`application.cpp:237-240`、`270-273`（`while + Sleep(20)`）、`245`、`278`、`301`（`Sleep(5)`）
- 现象：① 用户按住组合键的整段时间，主循环被这个 `while` 独占，循环体只做 `pollEvents + Sleep(20)`：不渲染、不处理 `InterFace()` 的自动隐藏、托盘消息也得不到及时处理 → "按住热键界面像死了"。② `CtrlAndC()` 后固定 `Sleep(5)` 就认为剪贴板已更新（注释自己写着"等待上面的内容复制到剪切板上"），源程序慢/内容大/跨进程时会读到**上一次的内容或空串** → 偶发翻译错对象。③ `Sleep` 是墙钟而计时用 `clock()`（第 6 条），两种时间语义混用。
- 证据：
```
application.cpp:237  while ((GetKeyState(Variable::MakeUp) < 0) || (GetKeyState(Variable::Choicekey[0]) < 0)) {
application.cpp:238      mWindow->pollEvents();
application.cpp:239      Sleep(20);
application.cpp:245  Sleep(5);//等待上面的内容复制到剪切板上
```
- 建议修法：三段热键逻辑改成**非阻塞状态机**（`enum class HotkeyState { Idle, WaitRelease, WaitClipboard }`，每帧推进），主循环保持渲染；剪贴板用序号轮询替代定时赌：
```cpp
const DWORD seq0 = GetClipboardSequenceNumber();
TOOL::CtrlAndC();
for (auto dl = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
     GetClipboardSequenceNumber() == seq0 && std::chrono::steady_clock::now() < dl; ) {
    mWindow->pollEvents();  std::this_thread::sleep_for(std::chrono::milliseconds(2));
}
```

#### [中等] 初始化失败路径跳过 `cleanUp()`：异常从 `initVulkan/initImGui` 逃出时资源全泄漏、销毁顺序也被打乱
- 位置：`application.cpp:30-33`（无 RAII 的顺序调用）、`application.cpp:75`（`.value()`）、`ImGui\Interface.cpp:72-97`（描述符池失败抛异常）
- 现象：`run()` 里 `initVulkan(); initImGui(); mainLoop(); cleanUp();` 顺序执行，任一环节抛异常就直接跳到 `main.cpp:37` 的 catch，而 `~Application()` 是 `= default`（`application.h:32`）什么都不做：已 new 的 `Instance/Device/WindowSurface/SwapChain/RenderPass/CommandPool` 全部泄漏；更糟的是 catch 之后 `main.cpp:43` 仍会 `delete mWin` → `glfwTerminate()` 先跑，而 Vulkan 对象还持有该窗口的 surface（Vulkan 要求先销毁 swapchain/device 再销毁 surface）。`init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();` 在队列族缺失时抛 `std::bad_optional_access`，正是这条路径。
- 证据：
```
application.cpp:30-33  initVulkan(); initImGui(); mainLoop(); cleanUp();
application.cpp:75     init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();
application.h:32       ~Application() = default;
Interface.cpp:72-97    描述符池创建失败: throw std::runtime_error("Error: initImGui DescriptorPool 生成失败");
```
- 建议修法：`run()` 自己兜住异常并在重抛前调用 `cleanUp()`，`cleanUp()` 逐指针判空/置空做到幂等：
```cpp
void Application::run(VulKan::Window* w) {
    mWindow = w;
    try { initVulkan(); initImGui(); mainLoop(); } catch (...) { cleanUp(); throw; }
    cleanUp();
}
```
  并把 `.value()` 换成显式分支：`if (auto q = mDevice->getGraphicQueueFamily()) init_info.QueueFamily = *q; else throw std::runtime_error("显卡不支持图形队列");`。

#### [中等] `check_vk_result` 直接 `abort()`：Release 下没有任何可见输出（静默退出）
- 位置：`application.cpp:11-18`（经 `application.cpp:84` 作为 `CheckVkResultFn` 交给 ImGui 后端）
- 现象：ImGui/Vulkan 后端报错时只往 `stderr` 打一行然后 `abort()`；Release 是 `/SUBSYSTEM:WINDOWS`（`CMakeLists.txt:113`）没有控制台，`fprintf` 无处可见，`DebugLog.h:1` 的 `TRANSLATOR_ENABLE_LOG` 又是 0，用户只看到"程序突然消失"；此时 `TOOL::logger` 甚至可能还没初始化（`main.cpp:26` 才初始化）。
- 证据：
```
application.cpp:11-18  static void check_vk_result(VkResult err) {
                           if (err == 0) return;
                           fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
                           if (err < 0) abort();
                       }
```
- 建议修法：先落盘日志再决定是否退出，并给用户可见提示：
```cpp
static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    if (TOOL::logger) TOOL::logger->error("[vulkan] VkResult = {}", (int)err);
    if (err < 0) {
        char buf[128]; std::snprintf(buf, sizeof buf, "Vulkan 致命错误: VkResult = %d", (int)err);
        MessageBoxA(NULL, buf, "TranslatorKyi", MB_OK | MB_ICONERROR);
        spdlog::shutdown();  std::exit(EXIT_FAILURE);   // 比 abort() 可控
    }
}
```

#### [中等] CMake：Debug 没有 `/SUBSYSTEM:WINDOWS`（必然多一个控制台窗口），非 Debug 分支的 `/O3` 是 MSVC 不认识的选项
- 位置：`CMakeLists.txt:6-11`、`84`、`90-113`
- 现象：① `set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O3")` —— MSVC 只有 `/O1 /O2 /Ox`，`/O3` 被 `cl` 忽略（D9002），"Release 用 O3"的意图落空（RelWithDebInfo 实际靠 CMake 默认 `/O2`，故不是性能事故，但日志有噪音且 `CMAKE_CXX_FLAGS` 对所有配置生效）。② `LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"` 只写在 **else（非 Debug）分支**（L113），L84 那份还被注释掉 → Debug 链接成控制台程序，多出黑窗口，行为与 Release 不一致。③ `add_executable`（L75）显式列了 `resource.rc` 是对的，但没加 `WIN32` 关键字（与第 15 条相关）。
- 证据：
```
CMakeLists.txt:10   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O3")
CMakeLists.txt:84   # set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
CMakeLists.txt:113  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
```
- 建议修法：优化级别用 CMake 标准手段，子系统设置移出 if/else：
```cmake
if(MSVC)
  add_compile_options(/utf-8 /W4)
endif()
# add_executable 之后，与配置无关：
set_target_properties(TranslatorKyi PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
# 需要指定优化：target_compile_options(TranslatorKyi PRIVATE $<$<CONFIG:Release>:/O2>)
```

#### [中等] 缺少 `/utf-8`：无 BOM 的 UTF-8 中文源码依赖本机 ANSI 代码页，且已有两个文件的中文注释被写坏
- 位置：`CMakeLists.txt:4`（只设标准，无任何编码选项）；受损文件 `texture\texture.h:4-7`、`texture\texture.cpp:3-4`
- 现象：所有受审文件都是**无 BOM 的合法 UTF-8**（逐字节验证）。没有 `/utf-8` 时 MSVC 按系统 ANSI 代码页解释源码：非中文代码页机器上中文注释的字节会被拆开解释，尾字节可能与后面的引号/反斜杠/换行拼成非法记号——这正是 `main.cpp:2-6` 注释里记录的"换了电脑编译说有语法错误 / 在中文结尾后加一个空格或字母"的症状。另外 `texture/texture.h` 偏移 68 起是 `2F 2F EF BF BD EF BF BD EF BF BD EF BF BD CD BC C6 AC`（`//` + 4×U+FFFD + 残余字节），`texture/texture.cpp` 偏移 70 起是 `EF BF BD C4 BA EA B6 A8 EF BF BD EF BF BD`——这两个文件的中文注释**已被不可逆地替换成替换字符**，说明历史上确实被 ANSI 编码的编辑器保存过一次。
- 证据：
```
CMakeLists.txt:4        set(CMAKE_CXX_STANDARD 17)
main.cpp:2-6            /* 换了电脑编译说有语法错误：目前知道的是：中文输出的问题。
                          解决方法：在中文结尾后加一个 “空格” 或  字母  。 */
texture\texture.cpp:3   #define STB_IMAGE_IMPLEMENTATION//<EF BF BD>...
```
- 建议修法：`if(MSVC) add_compile_options(/utf-8) endif()`；重写 `texture/texture.{h,cpp}` 里已损坏的注释；给仓库加 `.gitattributes` 明确换行策略，避免再次被来回转换。

#### [中等] `TEXT(e.what())`：工程一旦定义 `UNICODE` 就编译不过
- 位置：`main.cpp:38`
- 现象：`TEXT(x)` 是在 `x` 前加 `L` 的宏，参数 `e.what()` 是表达式而非字面量，展开为 `L e.what()`（`Le.what()`）——**只在未定义 `UNICODE` 时才能编译**。当前 `add_executable`（`CMakeLists.txt:75`）没加 `WIN32`、也没有 `/DUNICODE`，走 ANSI 分支勉强能过；任何人加 `WIN32`/`-DUNICODE`、在 VS 里把字符集改成 Unicode，或把这两行按模板抄到别处，立刻编译错误。
- 证据：
```
main.cpp:38  MessageBoxEx(NULL, TEXT(e.what()), TEXT("main"), MB_OK, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
CMakeLists.txt:75  add_executable(TranslatorKyi  ${DIRSRCS} resource.rc)   // 无 WIN32，无 /DUNICODE
```
- 建议修法：宏不用于表达式，明确指定 A/W 版本并做一次 UTF-8→宽字符转换；同时把 `TEXT("main")`、英文 `MAKELANGID` 统一成中文提示：`MessageBoxA(NULL, e.what(), "main", MB_OK);`

#### [中等] `resource.rc` 里的图标 ID 从未定义，编译后大概率不是 exe 的默认图标（待确认）
- 位置：`resource.rc:1`（全仓库没有任何 `#define IDI_ICON1`）
- 现象：.rc 中未定义的标识符会被 rc.exe 当**字符串名字**，即图标以名字 `"IDI_ICON1"` 注册而非序号；Windows 在资源管理器中显示 exe 图标时取"序号最小的图标资源"，因此该图标很可能不被采用。另外托盘图标是运行时从文件加载的（`Vulkan\Window.cpp:103 LoadImage(NULL, TEXT("product.ico"), ..., LR_LOADFROMFILE)`），依赖工作目录里有 `product.ico`。**待确认**：需实际编译后用资源查看器/资源管理器核对，或读 PE 资源表。
- 证据：
```
resource.rc:1          IDI_ICON1 ICON "product.ico"
Vulkan\Window.cpp:103  nidApp.hIcon = (HICON)LoadImage(NULL, TEXT("product.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
```
- 建议修法：`#define IDI_ICON1 101` + `IDI_ICON1 ICON "product.ico"`（唯一图标即最小序号），托盘图标改 `LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1))`，摆脱工作目录依赖。

#### [轻微] 单实例检测用窗口标题字符串，且已有实例不会被激活
- 位置：`main.cpp:17-21`；标题来源 `Vulkan\Window.cpp:47 glfwCreateWindow(mWidth, mHeight, "TranslatorKyi", ...)`
- 现象：`FindWindow(NULL, "TranslatorKyi")` 匹配的是"任何"同名标题窗口而非本程序实例（隐藏窗口也能被 FindWindow 找到，所以可用靠巧合）；第二次启动只弹一句英文 `Software Started`，标题却是 `Error`，也不激活已有实例。
- 证据：
```
main.cpp:17-20  if (FindWindow(NULL, "TranslatorKyi")) {
                    MessageBoxEx(NULL, TEXT("Software Started"), TEXT("Error"), MB_OK, ...);
                    return FALSE; }
```
- 建议修法：`CreateMutexW(NULL, TRUE, L"TranslatorKyi_SingleInstance")` + `GetLastError() == ERROR_ALREADY_EXISTS` 判重，配合 `RegisterWindowMessage` 通知旧实例（或至少把提示语改成中文"已在运行，请查看托盘图标"）。

#### [轻微] 托盘图标没有 `NIM_DELETE`，托盘窗口句柄也从不销毁
- 位置：`Vulkan\Window.cpp:74-105`（`SystemTray()` 内 `hwnd`、`nidApp` 都是局部变量）
- 现象：全仓库搜不到 `Shell_NotifyIcon(NIM_DELETE)` 与 `DestroyWindow`，退出（尤其 `exit(0)`）后图标可能残留在通知区域，`"My Window"` 隐藏窗口一直存在；句柄未存成成员，事后也无法清理。
- 证据：
```
Vulkan\Window.cpp:104  Shell_NotifyIcon(NIM_ADD, &nidApp);   // 之后没有任何 NIM_DELETE
```
- 建议修法：把 `HWND mTrayHwnd; NOTIFYICONDATA mNid;` 提为 `Window` 成员，在 `~Window()` 里先 `Shell_NotifyIcon(NIM_DELETE, &mNid); DestroyWindow(mTrayHwnd);`（顺序在 `glfwTerminate()` 之前）。

#### [轻微] `exit(0)` 散落在 UI/窗口过程里
- 位置：`ImGui\Interface.cpp:1438`（托盘菜单"退出"）、`ImGui\Interface.cpp:1378`（重启）、`Vulkan\Window.cpp:132`（processEvent，当前无调用点）
- 现象：`exit()` 不展开栈，直接从渲染/消息回调里终止进程：`cleanUp()`、`~ImGuiInterFace`（描述符池、ImGui 上下文、tesseract、翻译器）全部跳过，行为也无法测试（第 1 条的三所有者问题正是这种写法掩盖出来的）。
- 证据：
```
Interface.cpp:1437-1438  if (ImGui::Button(Language::Exit.c_str())) { exit(0); }
```
- 建议修法：统一成 `std::atomic<bool> RequestQuit{false}` / `RequestRestart{false}`，由主循环在帧边界响应并走 `cleanUp()`。

### 二、性能与资源

- **每轮固定 `sleep_for(10ms)`**（`application.cpp:198`）：`Sleep` 受系统时钟粒度影响（默认约 15.6ms），实际轮询可能只有 ~64Hz；空闲时 UI 无需高频刷新，开销可接受，但建议改 `glfwWaitEventsTimeout(0.01)`，既省 CPU 又能在按键期间保持刷新（配合第 9 条的状态机改造）。
- **忙等期间完全不渲染**（`application.cpp:237-240/270-273`）：按住热键 → 界面冻结、自动隐藏计时被挤压，这是"手感卡顿"的主因，比帧率问题更影响体验。
- **每帧重录命令缓冲**（`application.cpp:333 → createCommandBuffers(imageIndex)`）：每次 `render()` 重录主命令缓冲与 ImGui 命令缓冲；对当前"仅在界面变化时才渲染"的量级可忽略（如将来改持续渲染，只需重录受影响的 imageIndex）。**待确认**，不影响现有结论。
- **同步对象组织**：`mImageAvailableSemaphores/mRenderFinishedSemaphores/mFences` 按 `getImageCount()` 建、按 `mCurrentFrame` 轮转（`application.cpp:179-190`、`383`），与 Vulkan 教程的 frames-in-flight 写法一致；但 present 对 `renderSemaphore` 的等待不受 fence 保护，属二进制信号量复用的边界情形，仅极高频连续重绘才有理论风险——**待确认**，无实测证据，不建议现在改。
- **截图缓冲从不释放**：`application.h:37 char* buffer = nullptr;` 只在 `application.cpp:307` 赋值，`cleanUp()` 不管它 → 一次截图就把整屏位图留在堆上（4K 约 33MB）。
- **`TOOL::screen()` 复用旧缓冲**（`Tool\Tool.cpp:328-374`：仅当 `buf == nullptr` 才 `new char[windows_Heigth * windows_Width * 4]`，同时每帧从 `GetDesktopWindow` 刷新尺寸）→ 分辨率变化/多屏不同尺寸时新位图更大即越界写。应与上一条一起改成 RAII（`std::vector<char>` + 记录容量，尺寸变化即重新分配）。
- **桌面尺寸只取主桌面矩形**（`application.cpp:22-26 GetWindowRect(GetDesktopWindow())`）：多显示器或副屏在左/上方时 `re.right/re.bottom` 与实际虚拟桌面不一致，影响一言定位（`Interface.cpp:1478` 用 `windows_Width * HitokotoPosX`）与截图尺寸；建议用 `GetSystemMetrics(SM_XVIRTUALSCREEN/SM_CXVIRTUALSCREEN)` 系列。
- **核对过、确认没有问题的项**（避免重复排查）：`LoadTextureFromFile` 开头即 `RemoveTexture`（`Interface.cpp:1512-1515`），重复截图不累积纹理；`ImGuiCommandBufferS/ImGuiCommandPoolS`、`mTesseract`、`mTranslate` 在 `~ImGuiInterFace`（`Interface.cpp:162-183`）里都释放了。

### 三、可读性与可维护性

- `[轻微]` **`bool mButton;`（`application.h:69`）没有初始化**，而 `application.cpp:306` 第一次就读取它（`(GetKeyState(...) < 0) != mButton`）→ 首次按下截图热键是否触发取决于不确定值（UB）。改 `bool mButton{ false };` 即可。同类还有 `clock_t TranslateTime;`（`ImGui\Interface.h:211` 未初始化，当前正常路径会先被 `SetInterFace` 赋值，但依赖调用顺序很脆弱）——用第 6 条的 `steady_clock::time_point` + 初值一并解决。
- `[轻微]` 类型/语义不符：`Screenshotkey/Choicekey/Replacekey` 是 `std::string` 但语义是单个 VK 码（`Variable.h:66-67`）；`MakeUp` 表达不出"组合键修饰键"（`Variable.h:65`）；`int UpdateTheScreen = false;`（`ImGui\Interface.h:122`）用 int 当 bool；`int kuangshu = 200;`（`Interface.h:208`）是像素宽但含义不明。
- `[轻微]` 魔法数字散布：`application.cpp:217` 的 `20000`、`ImGui\Interface.h:72/74` 的 `5000/10000`、`Interface.h:135-175`（`BeginWindowSizeX=280` / `BeginWindowSizeY=148`）的命中判定尺寸、`application.cpp:245/278/301` 的 `Sleep(5)`。至少命名（`kHitokotoIntervalMs` 之类）并集中一处，更好的是读 ini（见第 7 条，硬编码会盖掉用户配置）。
- `[轻微]` 半迁移痕迹应清理或注明原因：`application.cpp:29 //initWindow();` 与 `application.cpp:39-42` 的空 `initWindow()`；`application.cpp:78/80` 被注释的 `init_info.DescriptorPool/MinImageCount`（真正赋值在 `Interface.cpp:99-100`，注释留在原地会让人以为没设）；`CMakeLists.txt:84` 被注释的 `set_target_properties`；`Vulkan\Window.cpp:45/57/58` 的注释代码。
- `[轻微]` 注释与代码矛盾：`Vulkan\Window.cpp:43 glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);//是否禁止窗口改变大小`（实际允许）、`Vulkan\Window.cpp:44 GLFW_DECORATED, GLFW_FALSE);//窗口显示`（实际无边框）、`main.cpp:19` 提示语 `"Software Started"` 配标题 `"Error"`；`Tool\Tool.cpp:177 UnicodeToUtf8` 实际是按 `setlocale(LC_ALL, "chs")` 把 GBK 转 UTF-8（`Tool.cpp:143-170`），名字与行为不一致（调用方 `application.cpp:247` 容易误读），且这两个转换函数**全局修改 locale** 又不检查 `mbstowcs_s/wcstombs_s` 的返回计数，建议改用显式 `MultiByteToWideChar(CP_ACP/CP_UTF8)`/`WideCharToMultiByte`。
- `[轻微]` include 大小写混用（`application.h:4-12` 用 `Vulkan/...`，`application.h:13-15` 用 `VulKan/...`，实际目录名 `Vulkan`）：Windows 上无害，但这正是移植带来的风格分歧（`Vulkan\Window.cpp` 本次改动也只改了 include 大小写），在大小写敏感的 CI/跨平台构建上会直接失败。
- `[轻微]` **`mWidth{1}/mHeight{1}`（`application.h:27-28`）必须补注释**。它看着像笔误（次轮取证才排除）：`main.cpp:30 → Vulkan\Window.cpp:47 glfwCreateWindow(1,1,"TranslatorKyi",...)` 创建隐藏的 1×1 主窗口（`Vulkan\Window.cpp:63 glfwHideWindow`），真正界面由 ImGui 多视口（`Interface.cpp:30 ImGuiConfigFlags_ViewportsEnable`、`Interface.h:180 g_MinImageCount = 3`）另开平台窗口呈现；`Vulkan\swapChain.cpp:279-281 chooseExtent` 在 Win32 上直接返回 `capabilities.currentExtent`（即客户区 1×1），故主交换链也是 1×1——这是**设计意图而非缺陷**，请写进注释留住这个知识（并说明 `application.cpp:52-53` 让 `mWidth/mHeight` 变成 1 是预期行为）。
- `[轻微]` 重复代码：三段热键分支（`application.cpp:236-267 / 269-304 / 306-310`）的"忙等 + 取剪贴板 + 还原剪贴板"结构几乎相同，可抽 `std::string CaptureSelection();` + `void RestoreClipboard(const std::string&);`，第 9 条的状态机改造可在此一并做。
- `[轻微]` const 正确性：`ImGui\Interface.h:114 const VkCommandBuffer GetCommandBuffer(int i, VkCommandBuffer_InheritanceInfo info);` 返回 const 值无意义（`VkCommandBuffer` 是句柄，const 会被丢弃），且该函数会修改 `ImGuiCommandBufferS[i]`，本身不该是 const 成员；`texture\texture.h` 构造函数参数 `char* data` 应为 `const char*`，`Application::buffer` 同理。

### 四、可添加的功能建议（与本子系统相关、可落地）

1. **统一退出/重启流程 + 可靠单实例**：`CreateMutexW` 判重 + `RegisterWindowMessage` 激活旧实例；托盘菜单与设置界面只置 `RequestQuit/RequestRestart` 标志，由主循环在帧边界返回 → `cleanUp()` → `main` 返回；删掉散落的 `exit(0)` 与三处 `delete Window`。收益：double free 与"什么都不释放"两种退出同时消失，退出路径变得可测。
2. **一个 `steady_clock` 计时服务**：替换 `clock()`（`application.cpp:195/217`、`ImGui\Interface.h:91/101/105/139/151`），所有时长从 ini 取值（`DisplayTime`、`HitokotoTimeInterval`、`HitokotoDisplayDuration`），设置界面标注单位；顺带消灭 `TranslateTime` 未初始化。
3. **输入/取词链路加长度上限与可见反馈**：`eng/zhong` 改 `std::string` + `CallbackResize`，或拷贝统一走第 3 条的 `copyCapped`，超限提示"文本过长，已截断"；一并消灭 1MB 溢出、`selected_text[10000]` 爆栈、粘贴越界三个问题。
4. **把耗时工作挪出主循环**：`TranslateAPI` 目前在键盘回调里同步执行（`application.cpp:256`），OCR/网络卡住即界面停摆；加工作线程 + 结果队列（或 `std::async`），主循环只做事件与渲染，完成后 `SetInterFace(TranslateEnum)`。
5. **启动自检与可诊断性**：ini/语言文件/`TessData` 模型/字体路径启动时逐项校验，缺失则用默认值并汇总提示（而不是抛异常穿过 `main.cpp:23-26`）；`check_vk_result` 与所有 catch 统一走 spdlog + `MessageBoxA`；把 `DebugLog.h:1` 的 `TRANSLATOR_ENABLE_LOG` 提升为 CMake option（现恒为 0，Release 现场无日志）；可选加 `SetUnhandledExceptionFilter` 写 minidump。
6. **资源与工作目录解耦**：`FilePath.h` 的相对路径（`Data.ini`、`./Language/*.ini`、`product.ico`、`TessData`）统一基于 `GetModuleFileName` 的 exe 目录解析；CMake 资源拷贝改为 post-build 拷到 `$<TARGET_FILE_DIR:TranslatorKyi>`，换多配置生成器时资源仍与 exe 同目录——同时消除第 2 条的崩溃触发条件。

### 五、最推荐的 3 项改进（按性价比排序）

1. **一次性解决"三所有者 + 退出不清理"**：`Application` 只借用窗口、`main` 用 `unique_ptr` 持有；删 `application.cpp:404` 的 `delete mWindow;`，`Interface.cpp:1374` 改标志位，`ImGui\Interface.cpp:1438` 与 `Vulkan\Window.cpp:132` 的 `exit(0)` 收敛为主循环退出 + `cleanUp()`。约 30 行改动，同时消灭 double free、全量泄漏与不可测的退出路径。
2. **统一 `steady_clock` 计时 + 时长全部读 ini**：替换 `application.cpp:195/217`、`ImGui\Interface.h:139/151` 的 `clock()`，并把 `application.cpp:217` 的 `20000`、`Interface.h:74` 的 `10000` 换成 `Variable::HitokotoTimeInterval / HitokotoDisplayDuration`（`Data.ini:3-4` 早已存在、设置界面也能改）。约 20 行改动，修掉"设置无效"与"显示时长失真"两个用户可感知问题，顺带消灭 `TranslateTime` 未初始化。
3. **输入/取词长度上限 + 启动配置兜底**：`application.cpp:259-262` 与 `ImGui\Interface.cpp:277-283` 统一走带边界检查的拷贝；`main.cpp:23-26` 移进 `try`，ini/语言文件缺失或坏键走三参 `Get` 默认值并弹一次中文提示；相对路径改为基于 exe 目录。约 40 行改动，消灭栈/堆溢出与"双击没反应"两类最难现场定位的问题。

### 六、核对说明（静态推断 vs 需编译/运行验证）

- **纯静态精读即可确认**（证据是代码结构本身）：三所有者 double free 与 `exit(0)` 跳过清理；启动期异常在 `try` 之外；`application.cpp:259-262` 与 `ImGui\Interface.cpp:277-283` 无长度检查；acquire/present 返回值未处理；`mWindowResized` 全仓库无读取点、`StructureSwapChain()` 只在构造函数里调用；`application.cpp:217` 与 `Interface.h:74` 的硬编码数字；热键 `string[0]` 取 VK 码；`Sleep(5)` 赌剪贴板；`bool mButton;` 未初始化；`main.cpp:38` 的 `TEXT(e.what())` 在 `UNICODE` 下不可编译（宏展开规则）；`/O3` 不是 MSVC 选项；`/SUBSYSTEM:WINDOWS` 只在非 Debug 分支；`resource.rc` 的 `IDI_ICON1` 未被定义。
- **需要编译验证（本机无 Environment，做不到）**：`/O3` 被忽略的确切形式（D9002）；Debug 是否真的多出控制台窗口；`texture/texture.{h,cpp}` 里已损坏的注释是否会引发语法错误（注释内容不影响编译，但同一文件里被 ANSI 解释的**中文字符串字面量**会）；`.rc` 里字符串名图标是否被资源管理器采用。
- **需要运行时验证（已标"待确认"）**：① `clock()` 与墙钟的实际倍率（每 10 秒墙钟打印一次 `clock()` 增量即可确认，取决于每帧真实 CPU 占用）；② 隐藏的 1×1 主窗口/主交换链是否需要"客户区最小尺寸"补偿；③ `logs/` 目录不存在时 `spdlog::basic_file_sink_mt("logs/Error.txt", false)` 是否自动建目录（取决于所链接的 spdlog 版本，仓库内无 spdlog 源码）；④ 同步对象按 `mCurrentFrame` 轮转在极高频连续重绘下是否存在 present 等待与 fence 复用的边界问题。
- **与协调方已有结论的对照**：第 1 点（三所有者 + `ShellExecute` 重启后 `exit(0)` 跳过 cleanUp）与我的独立核对完全一致，另补充：`Vulkan\Window.cpp:4` 的 `GAME::Application* mAppcpp;` 是非静态全局变量，而**全仓库没有任何 `glfwSetWindowShouldClose` 调用**，`while (!mWindow->shouldClose())`（`application.cpp:196`）无法由用户结束——即 `cleanUp()` 与 `main.cpp:43 delete mWin;` **当前事实上都跑不到**，double free 只在"有人让主循环正常返回"后才会命中，现在的真实退出路径是"什么都不释放"。第 2 点（失败要么 exit/abort、要么 throw 到 `main.cpp:34-41` 的唯一 catch）确认无误，另补充 `abort()` 在 Release 下没有可见输出（stderr 无控制台）。



---

## 界面层 · 体检报告

审查范围：`ImGui/Interface.cpp`（1718 行）、`ImGui/Interface.h`（265 行）、`ImGui/GUI.h`、`ImGui/CMakeLists.txt`；为确认契约另读了 `application.h/.cpp`、`main.cpp`、`Variable.h/.cpp`、`Tool/Tool.h`、`Function/Translate.h`、`Function/Hitokoto.cpp`、`Vulkan/commandBuffer.*`、`Vulkan/commandPool.cpp`、`Vulkan/Window.*`、`AngelScript/AngelScriptCode.*`、`Data.ini`、`Language/*.ini`。
方法：本机无法编译，全部结论来自逐行精读与跨文件交叉核对；行号为本次阅读时的真实行号。文中 `[待确认]` 表示需要运行时验证，并给出确认方法。

### 一、缺陷与风险

#### [严重] 1. Window 对象存在三个删除者：正常退出路径重复释放，重启路径绕过全部回收
- 位置：`ImGui/Interface.cpp:1373-1379`、`application.cpp:404`、`main.cpp:43`（所有权来源：`application.cpp:28` / `application.cpp:87` / `Interface.h:182`）
- 现象：`Interface.h:182` 的 `mWindown` 只是别人传进来的裸指针，界面层并不拥有它，却在 1374 行 `delete mWindown;`。正常退出（窗口关闭 → `application.cpp:196` 的 `while (!mWindow->shouldClose())` 结束 → `application.cpp:33 cleanUp()`）时 `application.cpp:404` 删一次，回到 `main.cpp:43` 又删同一个对象 → 二次释放；`Vulkan/Window.cpp:108-110` 的析构里是 `glfwDestroyWindow` + `glfwTerminate`，二次调用后果不可控。重启分支则相反：1374 行删完立即 `exit(0)`，`cleanUp()` 与 `main` 的 delete 都不执行，Surface/SwapChain/Device（`application.cpp:398-403`）全部不回收，靠"进程马上退出"掩盖。
- 证据：
```cpp
// ImGui/Interface.cpp:1373-1379
if (updata) {
    delete mWindown;                       // 界面层删了 Application/main 拥有的对象
    char path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    ShellExecute(NULL, NULL, path, NULL, NULL, SW_SHOWDEFAULT);
    exit(0);                               // 跳过 cleanUp()，也跳过 main.cpp:43
}
```
```cpp
application.cpp:404  delete mWindow;       // cleanUp() 内
main.cpp:43          delete mWin;          // 同一个 Window*（main.cpp:30 创建）
```
- 建议修法：界面层不 delete；把"重启"降级为一个请求标志，由 `Application` 在 `cleanUp()` 之后、`main` 退出前统一重启（`CreateProcess` + 等待旧进程退出）；`Window` 的所有权只保留一处（建议 `Application` 持有，删掉 `main.cpp:43` 的 delete）。
- 待确认：按 Alt+F4 或在任务栏关闭窗口，若进程在 `cleanUp()` 之后崩溃于 `glfwDestroyWindow/glfwTerminate`，即证实二次释放。

#### [严重] 2. 剪贴板/选区处理有三处定长缓冲越界（含 128 字节成员数组）
- 位置：`ImGui/Interface.cpp:248-255`、`ImGui/Interface.cpp:277-283`、`ImGui/Interface.cpp:609-615`
- 现象：
  1. `MyText` 回调里 `char selected_text[10000];` 按选中长度 memcpy，终止符写在 `selected_text[len+1]`（应为 `[len]`）；`eng` 是 1MB（`Interface.h:212`），选中 10000 字符即写越界。
  2. `InputTextMultilineText()` 里 `int Len = mTextLen - mCursorPos; memcpy(selected_text, &eng[mCursorPos], Len);` 既没有上界（>10000 溢出），也没有下界（负数转 size_t 会拷贝巨量数据）；随后 `memcpy(&eng[mCursorPos], ClipboardText.c_str(), ClipboardText.size())` 完全不检查 `eng` 剩余容量。
  3. `InputText()`（设置窗口所有输入框共用）把剪贴板内容 memcpy 进 `InputInfo.Text`，而该指针指向的是 `SetWebDav_url[128]` 这类定长数组（`Interface.cpp:636-639`）→ 粘贴长文本必然溢出。
- 证据：
```cpp
// Interface.cpp:250-251（MyText）
memcpy(selected_text, &data->Buf[data->SelectionStart], (data->SelectionEnd - data->SelectionStart));
selected_text[(data->SelectionEnd - data->SelectionStart) + 1] = '\0';   // 越界且下标多 1
// Interface.cpp:277-281（InputTextMultilineText）
char selected_text[10000]; int Len = mTextLen - mCursorPos;
memcpy(selected_text, &eng[mCursorPos], Len);                            // 无上下界
memcpy(&eng[mCursorPos], ClipboardText.c_str(), ClipboardText.size());   // 无容量检查
```
- 建议修法：删掉手写剪贴板逻辑，改用 ImGui 官方回调 API：`data->InsertChars(data->CursorPos, text)` / `data->DeleteChars(...)`，让 ImGui 自己维护容量与光标；万不得已要自己拷贝时用 `std::string` + `min(len, bufSize - 1)` 并显式写终止符。

#### [严重] 3. 字号可被设成 0，导致两处除零（整数除零 + 浮点除零）
- 位置：`ImGui/Interface.cpp:993`（设置入口）→ `ImGui/Interface.cpp:300`、`ImGui/Interface.cpp:1469`、`ImGui/Interface.cpp:1477`（消费点）
- 现象：设置窗口用 `ImGui::InputFloat(..., &Variable::FontSize, 0.1f, 1.0f)` 直接绑定 `Variable::FontSize`，**没有下限**，用户拖到 0（或 0.4）并回车后：`WrapSize = kuangshu / int(Variable::FontSize)` 是整数除零（SIGFPE/UB）；一言窗口的 `HitokotoFontSize / Variable::FontSize` 与 `SetWindowFontScale(...)` 则得到 inf/NaN，窗口宽度算出天文数字。`Data.ini:51` 当前是 16.0，所以默认配置不触发，但只要用户改一次字号就可能复现；`Variable.cpp:51` 读 ini 时也没有校验。
- 证据：
```cpp
// Interface.cpp:993（设置入口，无下限）
ImGui::InputFloat(Language::FontSize.c_str(), &Variable::FontSize, 0.1f, 1.0f);
// Interface.cpp:300（整数除零）
Variable::WrapSize = kuangshu / int(Variable::FontSize);
// Interface.cpp:1477（浮点除零）
ImGui::SetWindowFontScale(Variable::HitokotoFontSize / Variable::FontSize);
```
- 建议修法：把字号换成 `SliderFloat(..., 8.0f, 72.0f, "%.1f")` 或 `InputFloat` + 提交后 `Variable::FontSize = ImClamp(Variable::FontSize, 8.0f, 72.0f)`；在 `Variable::ReadFile` 里同样 clamp（全项目目前只有 `VulkanDeviceMode` 做了范围校验，见 `Variable.cpp:66-72`）。

#### [严重] 4. 把运行期（网络/语言文件）字符串当 printf 格式串
- 位置：`ImGui/Interface.cpp:1402`、`ImGui/Interface.cpp:1480`（一言）；`ImGui/Interface.cpp:837/965/974/990/1011/1044/1214/1241`（语言文件）
- 现象：`ImGui::Text(Hitokoto.c_str())` 中 `Hitokoto` 来自 `https://v1.hitokoto.cn` 的 JSON（`Function/Hitokoto.cpp:10-34`），是**网络返回内容**；`ImGui::Text(Language::X.c_str())` 中语言串来自 `./Language/*.ini`，而该目录里任何 ini 都能被 `TOOL::FilePath` 扫描并选中。只要内容里出现 `%s`/`%n`/`%d`，就会按栈上不存在可变参数解析 → 崩溃或内存泄漏（`%n` 可写内存）。ID 冲突的风险同样存在（相同文本 = 相同 ID）。
- 证据：
```cpp
// Interface.cpp:1402
ImGui::Text(Hitokoto.c_str());                                   // 网络数据当格式串
// Interface.cpp:837
ImGui::Text(Language::BackupsFolder.c_str());                    // ini 数据当格式串
```
- 建议修法：统一改为 `ImGui::TextUnformatted(s.c_str())` 或 `ImGui::Text("%s", s.c_str())`。注意 `Interface.cpp:1142/1158/1164/1176` 等处已经正确写成 `Text("%s", ...)`，属同一文件内的前后不一致。语言字符串里唯一合法的 `%s` 模板（`Language/zh.ini:69-76` 的 RenderDeviceItem_* 系列）已由 `FillDeviceText` + `Text("%s",...)` 正确消费，不要误改。

#### [严重] 5. `LoadTextureFromFile` 的失败路径被整体废弃：会空指针写、每次截图漏一个命令缓冲、且永远返回 true
- 位置：`ImGui/Interface.cpp:1512-1705`（调用点 `application.cpp:308`）
- 现象：
  - 从 1547 到 1701 共 15 处 `//check_vk_result(err);` 全被注释；`findMemoryType`（1499-1509）失败返回 `0xFFFFFFFF` 且无人检查；一旦 `vkAllocateMemory` 失败，`vkMapMemory` 拿不到地址，紧接着的全屏 `memcpy(map, Texturedata, image_size)`（约 8MB）就是空指针写。
  - 1633-1642 从 `ImGuiCommandPoolS[0]` 分配 1 个 primary command buffer 用于拷贝，**从不 `vkFreeCommandBuffers`** → 每次截图泄漏 1 个命令缓冲（池是 `new VulKan::CommandPool(mDevice)`，见 139-145）。
  - 1704 行无条件 `return true;`，任何失败都无法被上层感知。
- 证据：
```cpp
// Interface.cpp:1633-1642
VkCommandPool command_pool = ImGuiCommandPoolS[0]->getCommandPool();
vkAllocateCommandBuffers(mDevice->getDevice(), &allocInfo, &commandBuffer);
// ... 全函数结束都没有 vkFreeCommandBuffers(command_pool, 1, &commandBuffer)
// Interface.cpp:1704
return true;                                  // 失败也返回 true
```
- 建议修法：恢复 `check_vk_result`（或至少检查 `findMemoryType != 0xFFFFFFFF`、`map != nullptr`），失败时按已有顺序逆序清理并 `return false`；`LoadTextureFromFile` 返回 bool，`application.cpp:308` 判断失败则不要 `SetInterFace(ScreenshotEnum)`；函数末尾补 `vkFreeCommandBuffers`。

#### [严重] 6. 每帧重录二级命令缓冲却不重置，命令池也没有 RESET 位
- 位置：`ImGui/Interface.cpp:139-145`（建池）与 `ImGui/Interface.cpp:226-231`（每帧使用）
- 现象：`new VulKan::CommandPool(mDevice)` 用默认参数 0（`Vulkan/commandPool.cpp:6-17`，注释明确写了 RESET_COMMAND_BUFFER_BIT 才是"可单独重置"），而 `GetCommandBuffer()` 每帧对同一个二级缓冲调用 `begin()` → `Vulkan/commandBuffer.cpp:28-40` 内部只调 `vkBeginCommandBuffer`，不做 reset。一个已经 end/提交过的命令缓冲不处于 initial 状态，重录前必须先 reset，否则违反 Vulkan 规范（校验层会报 VUID-vkBeginCommandBuffer-commandBuffer-00049 一类错误），在部分驱动上会直接失败/未定义。
- 证据：
```cpp
// Interface.cpp:227（每帧）
ImGuiCommandBufferS[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, info);
// Vulkan/commandBuffer.cpp:36
if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS) { ... }
```
- 建议修法：`new VulKan::CommandPool(mDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)`，并在 `begin()` 前 `vkResetCommandBuffer(mCommandBuffer, 0)`（或让 `CommandBuffer::begin` 自己带 reset 参数）。

#### [严重] 7. 三处下拉框/列表访问没有下标钳制，空目录或旧配置直接崩
- 位置：`ImGui/Interface.cpp:1048-1059`（替换语言，直接用 `Variable::ReplaceLanguage` 当下标）、`ImGui/Interface.cpp:1062-1073`（界面语言，无空表守卫）、`ImGui/Interface.cpp:334-338`（翻译引擎，硬编码 3）
- 现象：
  - `Variable::BaiduitemsName[Variable::ReplaceLanguage]` 直接用 ini 里的整数当下标，`Variable.cpp:52` 读取时不校验。同一段代码里 `for (n < Variable::BaiduitemsName.size() - 1)` 在空表时是无符号回绕（`size()-1` = SIZE_MAX）。
  - `LanguageS[LanguageIndex]` 前没有 `size() != 0` 判断，而同文件对 `ModelS`（996 行）和 `FontS`（1029 行）都做了守卫 → `./Language` 为空或 ini 缺失时越界。
  - 翻译引擎按钮 `mTranslate->TranslateName[mTranslate->mTranslate]` 中 `TranslateName` 只有 3 项（`Function/Translate.h:40`），自增后用 `> 2` 兜底；`Variable::Translate` 来自 ini 且 `SetTranslate`（`Function/Translate.h:22`）不校验，越界即读非法指针。
- 证据：
```cpp
// Interface.cpp:1048（无钳制）
ImGui::BeginCombo(Language::ReplaceLanguage.c_str(), Variable::BaiduitemsName[Variable::ReplaceLanguage].c_str(), flags);
// Interface.cpp:1050（空表回绕）
for (int n = 0; n < Variable::BaiduitemsName.size() - 1; n++) { ... }
// Interface.cpp:336-338（硬编码 3）
if (mTranslate->mTranslate > 2) { mTranslate->mTranslate = 0; }
```
- 建议修法：所有"ini 整数 → 数组下标"统一 `ImClamp(idx, 0, (int)vec.size() - 1)`（空表则跳过该控件并显示占位文案）；`TranslateName` 改成 `std::vector<std::string>` 或用 `IM_ARRAYSIZE(TranslateName)` 代替字面量 3；循环条件用 `n + 1 < (int)vec.size()`。

#### [严重] 8. 切换 OCR 模型用"显式析构 + 显式构造"，不是 placement new
- 位置：`ImGui/Interface.cpp:1320-1326`
- 现象：`mTesseract->~Tesseract(); mTesseract->Tesseract::Tesseract(ModelS[ModelIndex].c_str());` 只是两次普通调用：对象生命周期在第一次后就已经结束，第二次没有开始新对象的生命周期（placement new 才会），此后对 `mTesseract` 的任何使用在标准语义上都是 use-after-destruction；另外构造若抛异常（`Tesseract` 会 new API、读模型文件），留下的是一个已析构对象而非可用对象，且 `InterFace()` 后续仍会用它做 OCR（`Interface.cpp:485`）。
- 证据：
```cpp
// Interface.cpp:1320-1326
if (Variable::Model != ModelS[ModelIndex]) {
    mTesseract->~Tesseract();
    mTesseract->Tesseract::Tesseract(ModelS[ModelIndex].c_str());
}
```
- 建议修法：`delete mTesseract; mTesseract = new Tesseract(ModelS[ModelIndex].c_str());`（配 try/catch，失败时回退到旧模型并恢复 `Variable::Model`）。

#### [严重] 9. `flags` 被同时当作 InputText 与 Combo 的标志位，Combo 侧是未定义位
- 位置：定义 `ImGui/Interface.h:209`；Combo 使用点 `ImGui/Interface.cpp:906/975/997/1030/1048/1062/1113/1200`
- 现象：`ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways` 在 ImGui 1.89.2 里是 `(1<<10)|(1<<8) = 1280`，却被直接传给 `BeginCombo`。`ImGuiComboFlags` 只定义 bit0-6（`ImGui/imgui.h:1140-1151`），1280 落在未定义位 —— 当前版本既不断言、也无效果，所以不是崩溃，而是**语义误用 + 随 ImGui 升级随时可能变成断言失败**（`ImGui/imgui_widgets.cpp:1623` 已有一处 Combo flags 断言）。同一变量还传到 `InputText`，把"给多行输入框用的 Tab 行为"混进了单行输入框。
- 证据：
```cpp
// Interface.h:209
ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
// Interface.cpp:1048（当 Combo flags 用）
ImGui::BeginCombo(Language::ReplaceLanguage.c_str(), Variable::BaiduitemsName[...].c_str(), flags);
```
- 建议修法：拆成两个常量：`const ImGuiInputTextFlags kTextFlags = ...`（多行输入框专用）与 `const ImGuiComboFlags kComboFlags = ImGuiComboFlags_HeightLargest`（或 0），并删掉单行输入框上的 `AllowTabInput`。

#### [中等] 10. 设置窗口同步块 8 处 memcpy 没有长度校验，目标只有 128 字节
- 位置：`ImGui/Interface.cpp:722-730`、`ImGui/Interface.cpp:733-735`
- 现象：`memcpy(SetWebDav_url, Variable::WebDav_url.c_str(), Variable::WebDav_url.size())` 之类共 8 处（WebDav url/username/password/WebFile、百度 ID/Key、有道 ID/Key），目标都是 `char[128]`；ini 被手改或从 WebDav 恢复来一份超长字符串（`Variable.cpp:6-75` 全无长度校验），打开设置窗口这一步就溢出。`memcpy(SetScreenshotkey, Variable::Screenshotkey.c_str(), 1)` 只抄 1 字节、不写终止符，若上一个进程留下的 static 内容非零则显示脏数据。
- 证据：
```cpp
// Interface.cpp:722-723
memcpy(SetWebDav_url, Variable::WebDav_url.c_str(), Variable::WebDav_url.size());
memcpy(SetWebDav_username, Variable::WebDav_username.c_str(), Variable::WebDav_username.size());
```
- 建议修法：`snprintf(SetWebDav_url, sizeof(SetWebDav_url), "%s", Variable::WebDav_url.c_str());`（或抽一个 `CopyToBuf(dst, src)` 辅助函数），快捷键用 `SetScreenshotkey[0] = Variable::Screenshotkey.empty() ? 0 : Variable::Screenshotkey[0];`。

#### [中等] 11. 恢复/备份按钮不点保存也会写回配置，与"改完按保存"的设计冲突
- 位置：`ImGui/Interface.cpp:886-902`（写回 `Variable::WebDav_*`）、`ImGui/Interface.cpp:941`（`Variable::ReadFile(iniData)`）
- 现象：设置窗口里绝大多数控件改的是"待保存"的 static 副本，按保存（1252-1390）才写回 `Variable`；但 897-900 恢复按钮在打开恢复列表时就把界面上的 WebDav 输入框内容直接写进 `Variable::WebDav_*`，941 行更是整份 ini 重读。结果是"没点保存也生效"与"点了保存才生效"两种行为混在一起，用户无法预期；重读后 `SetBool` 同步块才会在下一帧把界面刷新回来。
- 证据：
```cpp
// Interface.cpp:897-900（未保存即生效）
Variable::WebDav_url = SetWebDav_url;
Variable::WebDav_username = SetWebDav_username;
Variable::WebDav_password = SetWebDav_password;
Variable::WebFile = SetWebDav_WebFile;
```
- 建议修法：恢复流程改成"下载 → 写回 Variable → 立即 SaveFile → 提示重启"，不要在按钮里顺手提交未保存的输入框；或者把这些输入框改成即时生效并去掉"保存"语义，二选一。

#### [中等] 12. 独立（一言）字体下拉永远回到第 0 项：裸文件名与全路径比较
- 位置：`ImGui/Interface.cpp:804-810`（比较）与 `ImGui/Interface.cpp:1271-1284`（保存时才知道是全路径）
- 现象：`Variable::HitokotoFont` 保存的是 `"./TTF/xxx.ttf"` 或 `"C:\\Windows\\Fonts\\xxx.ttf"`（1271-1284 拼接），而 `FontS` 里存的是不含扩展名的裸文件名（`TOOL::FilePath` 扫描结果，740-748）；`if (FontS[i] == Variable::HitokotoFont) SetHitokotoFontIndex = i;` 永不成立 → 每次打开设置窗口都用第 0 项覆盖显示，用户以为自己选的字体丢了，保存后又被写回错误的路径。同类问题见 1362-1364 的保存条件。
- 证据：
```cpp
// Interface.cpp:804-810
for (int i = 0; i < FontS.size(); i++) {
    if (FontS[i] == Variable::HitokotoFont) { SetHitokotoFontIndex = i; }
}
```
- 建议修法：比较时统一去扩展名/统一拼全路径（`("C:\\Windows\\Fonts\\" + FontS[i] + ".ttf") == Variable::HitokotoFont || ("./TTF/" + FontS[i] + ".ttf") == Variable::HitokotoFont`），最好把"字体来源 + 文件名"存成两个字段，而不是一个拼接字符串。

#### [中等] 13. 独立字体空指针 + 保存时静默改写用户设置
- 位置：`ImGui/Interface.cpp:1463`（空指针）、`ImGui/Interface.cpp:1286`（静默改设置）；来源 `ImGui/Interface.cpp:57-64`（字体不判空）
- 现象：`AddFontFromFileTTF` 失败时返回 NULL 且不抛异常（`ImGui/imgui_draw.cpp:2151` 的断言在 Release 下不生效），`HitokotoFont` 就是空指针；随后 `HitokotoFont->CalcTextSizeA(...)`（1463）直接解引用 → 崩溃。注意 `PushFont(NULL)` 本身是安全的（`ImGui/imgui.cpp:7449-7453` 会回退默认字体），崩的是这行手写调用。另外 1286 行 `else Variable::HitokotoTTFBool = true;` 在"没有 TTF 字体可选"时直接把用户的 TTF 开关改成 true，无任何提示。
- 证据：
```cpp
// Interface.cpp:1463
ImVec2 textSize = HitokotoFont->CalcTextSizeA(HitokotoFont->FontSize, FLT_MAX, 0, Hitokoto.c_str());
// Interface.cpp:1286
else { Variable::HitokotoTTFBool = true; }
```
- 建议修法：字体创建后判空（`if (font == nullptr) { Variable::HitokotoFontBool = false; }`）或在使用点 `if (HitokotoFont == nullptr) return;`；1286 行改为在界面上提示"未找到可用 TTF，已使用内置字体"，且只改内存中的界面状态，不写用户配置。
- 待确认：`Data.ini` 当前 `HitokotoFontBool=0`、`HitokotoFont=0`、`HitokotoTTFBool=1`，默认路径不触发。验证方法：把 `Data.ini` 改成 `HitokotoFontBool=1`、`HitokotoTTFBool=0`、`HitokotoFont=./TTF/不存在.ttf` 后启动，观察是否在 1463 行崩溃。

#### [中等] 14. 布局魔法数与一处明显笔误，加上用"全局左键按下"判断窗口缩放
- 位置：`ImGui/Interface.cpp:417-427`（重算尺寸）、`ImGui/Interface.cpp:345-349`（缩放判定）、`ImGui/Interface.cpp:394`、`ImGui/Interface.cpp:517-518`
- 现象：
  - 424-427 行的"宽度不足"兜底分支里，425 行 `RowsNumber = ImGui::GetTextLineHeightWithSpacing();` 把**像素高度当成行数**（该函数返回约 16-20px，而正常值是 3 以上），于是"框宽不够"被当成"要显示 16+ 行"来处理，下一帧 341 行 `SetWindowSize(BeginWindowSizeX, BeginWindowSizeY)` 会把窗口按这个行数重设 → 窗口高度跳变。这一看就是把 `RowsNumber` 和像素值写混了的笔误。
  - 345-349 行用 `GetKeyState(VK_LBUTTON) < 0` 作为"用户正在拖右下角缩放"的判定，鼠标在主窗口任意位置按下都会把 `BeginWindowSizeX/Y` 改成当前尺寸并触发重算。
  - 翻译窗口的按钮列宽 `GetTextLineHeightWithSpacing()*3 + 16`（422）、`*5 + 20`（394）、截图窗口的 `-8`/`+16`（517-518）、菜单定位的 `*3 - 24`（1417）都是硬编码，字体/DPI 一变就错位。
- 证据：
```cpp
// Interface.cpp:417-427（松手后按窗口尺寸反算行列）
RowsNumber = (BeginWindowSizeY - ImGui::GetTextLineHeightWithSpacing()) / (int(ImGui::GetTextLineHeight()) * 2);
if (RowsNumber < 3) { RowsNumber = 3; }
BeginWindowSizeY = (RowsNumber * int(ImGui::GetTextLineHeight()) * 2) + ImGui::GetTextLineHeightWithSpacing();
kuangshu = BeginWindowSizeX - (ImGui::GetTextLineHeightWithSpacing() * 3 + 16);
if (kuangshu < ImGui::GetTextLineHeightWithSpacing()) {
    RowsNumber = ImGui::GetTextLineHeightWithSpacing();   // 像素值当行数（笔误）
    BeginWindowSizeX = ImGui::GetTextLineHeightWithSpacing() * 4 + 16;
}
// Interface.cpp:517-518
ImGui::SetWindowPos(ImVec2(-8, -8));
ImGui::SetWindowSize(ImVec2(Variable::windows_Width + 16, Variable::windows_Heigth + 16));
```
- 建议修法：425 行改为按"期望高度 ÷ 行高"计算（例如 `RowsNumber = max(3, int(BeginWindowSizeY / (GetTextLineHeight()*2)))`）；缩放判定改用 `ImGui::IsItemActive()`/`GetWindowResizeBorderSize` 或记录上一帧尺寸做差值；把 -8/+16/24 这类常量提取为具名常量并按 `GetTextLineHeightWithSpacing()` 推导。

#### [中等] 15. 截图界面的坐标系与 DPI 不自洽：遮罩错位 8px、放大镜取样越界
- 位置：`ImGui/Interface.cpp:517-518`、`ImGui/Interface.cpp:525-559`、`ImGui/Interface.cpp:562-587`
- 现象：
  - 窗口放在 (-8,-8) 且尺寸为 `屏幕+16`，但 525-559 的四个"变暗遮罩"用的是绝对屏幕坐标（`(0,0)`、`(0,y)`、`(x+w,y)` …）→ 整块遮罩与真正的选区错位 8px，被选中的区域边缘会残留半暗。
  - 放大镜取样 UV 用 `(pt.x - 16) / Variable::windows_Width`，当鼠标在屏幕左上角（`pt.x < 16` 或 `pt.y < 16`）时 UV 为负；采样器是 `VK_SAMPLER_ADDRESS_MODE_REPEAT`（`Interface.cpp:1574-1589`）→ 直接把对侧屏幕边缘卷进来（视觉上是"左上角出现右下角画面"）。改成 `CLAMP_TO_EDGE` 即可，但注意遮罩/放大镜本身也要一起改。
  - `Variable::windows_Width/Heigth` 来自 `application.cpp:22-26` 的桌面矩形（物理像素），而 ImGui 的窗口坐标是逻辑坐标（受 `io.DisplaySize`/DPI 缩放影响）→ 在 125%/150% 缩放下，贴图尺寸、遮罩、放大镜、十字线会整体按比例偏移。
- 证据：
```cpp
// Interface.cpp:525-559（遮罩，绝对坐标 + 窗口 -8 偏移）
draw_list->AddQuadFilled(ImVec2(0, 0), ImVec2(Variable::windows_Width, 0),
                         ImVec2(Variable::windows_Width, y), ImVec2(0, y), ImVec4(0,0,0,0.5f));
// Interface.cpp:567-569（负 UV）
ImGui::Image((ImTextureID)mTextureData.DS, ImVec2(64, 64),
             ImVec2((pt.x - 16) / Variable::windows_Width, (pt.y - 16) / Variable::windows_Heigth), ...);
```
- 建议修法：遮罩坐标统一减去窗口原点（或改用 `ImGui::GetCursorScreenPos()` 基准的相对坐标）；采样器改 `CLAMP_TO_EDGE`；在 `ImGui_ImplGlfw` 初始化后按主显示器 DPI 计算 `io.FontGlobalScale`/`windows_*` 缩放因子，并在窗口 `WM_DPICHANGED` 时重算。
- 待确认：在 150% 缩放的显示器上截图，观察遮罩/选区/放大镜是否整体偏移；负 UV 可在左上角 5px 内移动鼠标复现（需要能编译运行的机器）。

#### [中等] 16. 1MB 结果缓冲的拷贝没有截断，且与截图缓冲共用同一块内存
- 位置：`ImGui/Interface.cpp:315-319`、`ImGui/Interface.cpp:500-503`（拷贝无截断）；`application.cpp:307-308` + `Interface.cpp:1518`（共用缓冲）
- 现象：`memset(eng, 0, ...)` 之后 `memcpy(eng, Variable::eng.c_str(), Variable::eng.size())`，没有 `min(size, sizeof(eng) - 1)`；`eng/zhong` 是 1MB 定长数组（`Interface.h:212-213`），译文或 OCR 结果超过 1MB（长文/整页 OCR 时并非不可能）就溢出。同一模式还出现在 `application.cpp:261-262`。另外 `TData`（`Interface.h:232`）指向 `TOOL::screen` 复用的一块堆缓冲，缓冲大小按**第一次**截图时的分辨率申请，`LoadTextureFromFile` 却按当前 `Variable::windows_Width/Heigth`（1521-1526）算大小并 memcpy → 分辨率变化（换显示器、改缩放）后是越界读。
- 证据：
```cpp
// Interface.cpp:315-319
Variable::zhong = mTranslate->TranslateAPI(eng);
memset(zhong, 0, sizeof(zhong));
memcpy(zhong, Variable::zhong.c_str(), Variable::zhong.size());   // 无截断
// Interface.cpp:1521-1526（按当前分辨率算大小，而缓冲是首次截图时申请的）
Channels = 4; Width = Variable::windows_Width; Height = Variable::windows_Heigth;
image_size = Width * Height * Channels;
```
- 建议修法：拷贝统一用 `memcpy(dst, src, min(src.size(), sizeof(dst) - 1))`；`TOOL::screen` 的缓冲改为按需 `realloc`（或由调用方传入尺寸并校验），`LoadTextureFromFile` 增加"缓冲区容量 ≥ 需求"的参数校验。这一条的后半部分与截图/工具层重叠，建议和 Vulkan 审查者对齐后一并改。

#### [中等] 17. 所有耗时操作都在渲染线程同步执行，界面会整段冻结
- 位置：`ImGui/Interface.cpp:315-319`（翻译）、`ImGui/Interface.cpp:485`（OCR）、`ImGui/Interface.cpp:839-885`、`ImGui/Interface.cpp:938`（WebDav 上传/下载）、`ImGui/Interface.cpp:1456-1472`（一言）
- 现象：全仓库没有任何线程（无 `std::thread/CreateThread/std::async`），因此**不存在跨线程数据竞争**（这一项可以放心），代价是网络/OCR 全部在渲染线程同步执行：点"翻译"要等 HTTP 往返（Baidu/Youdao 通常 200ms-2s），OCR 大图更慢，WebDav 备份/恢复可能几十秒。这段时间窗口不重绘、不响应，看起来像卡死；而截图的 `vkDeviceWaitIdle`（1700）又把 GPU 也一起等。
- 证据：
```cpp
// Interface.cpp:315-319（按钮内同步网络请求）
if (ImGui::Button(...)) {
    Variable::zhong = mTranslate->TranslateAPI(eng);
```
- 建议修法：翻译/OCR/WebDav 改为 `std::async`/工作线程 + 完成标志，界面显示"翻译中…"并禁用按钮（ImGui 里用 `BeginDisabled` + spinner 即可）；至少要给出进度提示，避免用户重复点击。

#### [中等] 18. 翻译引擎与模型索引来自 ini 且无校验，`TranslateAPI` 的 default 分支没有返回值
- 位置：`ImGui/Interface.cpp:150-159`（读入）、`Function/Translate.h:24-39`（缺 return）、`ImGui/Interface.cpp:334-338`
- 现象：`mTranslate->mTranslate`（`Function/Translate.h:48`）由 `Variable::Translate` 直读 ini 赋初值，越界时 334 行按钮直接越界索引（见第 7 条）。同时 `TranslateAPI` 的 `switch` `default: break;` 之后没有 `return`，非 void 函数走到这里就是 UB（GCC/Clang 会告警，MSVC 可能返回垃圾 `std::string` → 析构时崩溃）。
- 证据：
```cpp
// Function/Translate.h:24-39
std::string TranslateAPI(std::string English) {
    switch (mTranslate) {
    case 0: return Translate_Baidu(...);
    case 1: return Translate_ReptilesYoudao(...);
    case 2: return Translate_Youdao(...);
    default: break;          // 没有 return
    }
}
```
- 建议修法：`default: return {};`（或抛异常/返回原文），并在 `SetTranslate` 里 `mTranslate = ImClamp(translate, 0, 2)`。

### 二、性能与资源

- **每帧固定开销**：4 处 `FindWindow(NULL, "…")` + `SetWindowPos(HWND_TOPMOST, …)`（`Interface.cpp:433-440`、`591-594`、`1448-1451`、`1490-1493`），每帧 8 次 Win32 调用；每帧 `SetWindowSize`（`Interface.cpp:341`）强制覆盖尺寸。窗口置顶更适合在 GLFW 窗口创建/`shouldClose` 层级做一次。
- **每次打开设置窗口都扫磁盘**：`SetBool` 同步块（`Interface.cpp:717-812`）里 5 次 `TOOL::FilePath`（740-748）遍历 `./TessData|./TTF|C:\Windows\Fonts|./Language|./Opcode`，其中 `C:\Windows\Fonts` 有数千文件；且第 1077-1108 行每帧重建 `RenderDeviceLabels/RenderDeviceItems`（当前指针取用顺序正确，属于"能跑但脆弱"）。建议缓存，只在用户点"刷新"时重扫。
- **每帧字符串拷贝**：`FillDeviceText`（691-716）与设备下拉每帧拼字符串；改为一帧一次或脏标记。
- **截图路径最重**：`LoadTextureFromFile` 每次重建 Image/ImageView/Sampler/UploadBuffer/DescriptorSet（1512-1705）并 `vkDeviceWaitIdle`（1700）；同时存在两处泄漏 —— 每次 1 个 VkCommandBuffer（1633-1642，全函数无 `vkFreeCommandBuffers`）、进程退出时整条纹理链未 `RemoveTexture`（`~ImGuiInterFace` 162-183 没有调用，`RemoveTexture` 只在 1514 行被调用）。`RemoveTexture`（1708-1717）销毁后**不把 `tex_data->DS` 置空**，而 1514 行正是靠 `DS != nullptr` 判断"上次是否已清理"：当前之所以没出事，只是因为 1704 行无条件走到底并重新给 `DS` 赋值；一旦将来在 1514 行之后插入任何失败早退（比如恢复 `check_vk_result`），下次截图就会对同一批已销毁的 Image/ImageView/Sampler 再销毁一次。建议：在 `RemoveTexture` 末尾 `tex_data->DS = nullptr;`（并把其它句柄一并清零），截图前先移除旧纹理，纹理句柄复用，去掉 `vkDeviceWaitIdle` 改用 fence。
- **采样器配置矛盾**：`mipmapMode = LINEAR` 但 `mipLevels = 1`；`maxLod = 1000`；`maxAnisotropy = 1.0f` 却没开 `anisotropyEnable`（1574-1589）。功能上无害，但说明这段是复制粘贴来的，建议明确取舍。
- **静态缓冲**：`eng/zhong` 各 1MB（`Interface.h:212-213`）常驻内存；设置窗口另有约 30 个函数级 `static`（634-694），其中 `std::string Hitokoto`/`vector RecoveryList` 永不释放。占用不大，但让"重开设置窗口"无法真正复位状态。

### 三、可读性与可维护性

- **`SetUpInterface` 单函数 778 行**（`Interface.cpp:632-1409`），内部约 30 个函数级 `static` 充当"界面状态"，是所有"改了没生效/打开就跳回默认值"问题的温床。建议按折叠面板拆成 `DrawWebDavSection()/DrawFontSection()/DrawDeviceSection()/DrawScriptSection()/DrawHitokotoSection()`，并把 static 集中成一个 `SettingsUIState` 结构体（配合一个 `SyncFromVariable(state)` / `ApplyToVariable(state)` 对，替代现在混在一起的 717-812 与 1252-1390）。
- **样板重复**：4 个窗口的 `Begin(NoTitleBar|NoSavedSettings|NoResize|NoMove)` + 每帧 `SetWindowPos/SetWindowSize` + `FindWindow` + `TOPMOST` 是同一套复制 4 遍；建议抽 `BeginOverlayWindow(const char* name, ImVec2 pos, ImVec2 size, ImGuiWindowFlags extra)`。
- **命名与类型**：`flags`（`Interface.h:209`）名字太泛且跨类型混用（见第 9 条）；`kuangshu`（框数）、`RowsNumber`、`kai`、`updata`、`LFontSize` 等拼写/语义容易误解；`BeginWindowSizeX_2` 这种"带下标后缀的成员"含义只能靠上下文猜。
- **`mWindown`（`Interface.h:182`）拼写错误**（应为 mWindow），与实际字段 `application.h:73 mWindow` 只差一个字母，是第 1 条所有权混乱的语言层诱因。
- **死代码**：`GUI.h:12-23` 的 `HelpMarker` 全项目无调用点，而 `Interface.cpp:1179-1191` 手写了等价 tooltip；建议二选一（推荐统一用 HelpMarker，顺带统一 tooltip 样式与延时）。
- **构建配置**：`ImGui/CMakeLists.txt` 只有 3 行，`file(GLOB_RECURSE imgui ./ *.cpp)` 会把 ImGui 官方源码与以后新增的任何 .cpp 一起编进 `imguiLib`，且没有 C++ 标准、没有警告级别、没有 `target_include_directories`。建议显式列源文件、加 `target_compile_features(imguiLib PUBLIC cxx_std_17)` 与 `/W4`。
- **未初始化成员**：`Interface.h:202 bool TranslateBool;`、`Interface.h:204 bool WhoBool;` 没有初值，`InterFace()` 里首次使用前只由 `SetInterFace` 赋值 → 建议全部补 `{ false }`（同文件其它计数器如 `RowsNumber=4` 都有初值，属遗漏）。

### 四、可添加的功能建议（界面层相关，务实可落地）

1. **设置项校验 + 恢复默认值**：给所有来自 ini 的枚举/索引加载入时 clamp（`Variable.cpp` 目前只有 `VulkanDeviceMode` 做了），并在设置窗口加"恢复默认"按钮与"该项非法，已回退"的提示 —— 一次性消灭第 3/7/18 类崩溃。
2. **异步翻译/OCR + 进度态**：把 `TranslateAPI`/`IdentifyPictures` 挪到工作线程，界面用 `BeginDisabled` + spinner + "翻译中…"占位，取消按钮可用 `std::future` 的取消标志实现。体感提升最大。
3. **设置窗口加搜索框与折叠分组**（ImGui 有 `ImGuiTextFilter`）：现在 1500 行控件靠肉眼找，加上 `CollapsingHeader` 分组后可用性会明显改善。
4. **快捷键录制控件**：现在是 2 字节 `InputText`（`Interface.cpp:987-989`）只能存 1 个 ASCII 字符，`toupper(SetScreenshotkey[0])`（1310-1311）对中文/负值还是 UB。建议做一个"按下即录制"的按钮，支持 F1-F12 与组合键，存成 `{int vk; bool ctrl/alt/shift}`。
5. **截图界面增强**：选区尺寸实时标注（"320 × 180"）、按 Shift 锁定正方形/比例、方向键微调 1px、右键取消的提示气泡；这些都不需要新依赖，只是 `draw_list` 文本 + 一处钳制。
6. **翻译结果增强**：一键复制（`TOOL::CopyToClipboard` 已有）、朗读（可用 SAPI）、历史记录列表（下拉最近 N 条）、"重新翻译"按钮，避免用户为了重试而清空输入框。
7. **DPI/多显示器适配**：读取主显示器 DPI（`GetDpiForWindow`）计算缩放并写入 `io.FontGlobalScale`，同时按 DPI 重算 `windows_Width/Heigth`；处理 `WM_DPICHANGED`。这能顺带修掉第 15 条。
8. **字号即时预览**：`Variable::FontSize` 改完不必重启 —— 重新 `AddFontFromFileTTF` + `ImGui_ImplVulkan_CreateFontsTexture` 即可热切换（注意先保留旧图集指针直到帧结束）。
9. **诊断面板**：把 `TOOL::logger` 的内容显示在设置窗口的"日志"页（`ImGui::InputTextMultiline` 只读 + 复制按钮），用户报 bug 时不用去找日志文件。
10. **统一 tooltip/提示**：复活 `HelpMarker`，给每个设置项加一行说明（尤其"独立字体/一言/TTF 来源"这三项，现在完全靠猜）。

---
审查者备注：第 1、3、4、6、7、16 条与 Vulkan/工具层有交界（Window 所有权、`TOOL::screen` 缓冲、`check_vk_result` 被全项目注释），建议与 `01-Vulkan`/`03-工具层` 报告交叉核对后再定修复顺序；本次审查只读，未改动仓库任何文件。



---

## 功能层（翻译 / OCR / 网络 / 脚本） · 体检报告

审查对象：`Function/Translate.*`、`Function/WebDav.*`、`Function/opcode.*`、`Function/tesseract.*`、`Function/Hitokoto.*`、`Function/PileUp.h`、`Function/CMakeLists.txt`、`Opcode/Script.as`、`Opcode/D.cpp`，契约取 `Variable.h`、`Tool/Tool.h`、`AngelScript/AngelScriptCode.h`、`Data.ini`。
方法：逐行精读 + 全仓库 grep 交叉验证（本机 `Environment/` 不存在，无法编译，所有结论均给出可复核的 file:line 或 grep 证据）。
全仓库 grep 证据（两条）：
- `CURLOPT_TIMEOUT|CURLOPT_CONNECTTIMEOUT|SSL_VERIFYPEER|SSL_VERIFYHOST|CURLOPT_FOLLOWLOCATION|CURLOPT_NOSIGNAL|CURLOPT_MAXREDIRS` → **零匹配**（全项目 curl 无任何超时/证书/重定向配置）。
- `std::thread|detach(|async|CreateThread` → 仅命中 `Vulkan/descriptorSet.cpp` 与第三方 `AngelScript/scriptstdstring.cpp` → **业务代码 100% 单线程**。

### 一、缺陷与风险

#### [严重] 所有带参数 URL 均未做百分号编码，中文/空格文本必然请求失败
- 位置：`Function/Translate.cpp:120-129`、`Function/Translate.cpp:199-208`、`Function/WebDav.cpp:91`、`Function/WebDav.cpp:185-190`、`Function/WebDav.cpp:250`、`Function/WebDav.cpp:362`
- 现象：项目里确实写了 `UrlEncode`（`Function/Translate.cpp:54-76`），但只用在爬虫接口一处；百度/有道官方接口把原始 `English` 直接 `strcat` 进 URL，WebDAV 把中文文件名直接拼进 URL。翻译中文时 URL 含未编码的多字节 UTF-8（以及空格），服务端按 RFC 会 400 或截断，表现为"有时能翻、中文一定失败"。
- 证据（关键代码原文）：
  ```cpp
  char myurl[100000];
  char sign[100000]="";
  strcat(myurl, Variable::BaiduAppid.c_str());
  strcat(myurl, English.c_str());        // 未编码的原文直接进入 query
  ```
  `Function/WebDav.cpp:185-190` 同病：`CURLOPT_URL, (Variable::WebDav_url + Variable::WebDav_WebFile + "/" + path + "/" + File).c_str()`。
- 建议修法：统一走一个 `BuildUrl(base, {{"q", text}, ...})` 辅助函数，对所有值（含 WebDAV 路径段）调用现成的 `UrlEncode`；注意百度要求 `q=` 用 UTF-8 且空格按 `%20`（不要 `+`，本项目不要依赖表单语义）。

#### [严重] 全项目 curl 零超时配置，网络一慢即整窗卡死
- 位置：`Function/Translate.cpp:153-156`、`Function/Translate.cpp:269-295`、`Function/WebDav.cpp:98-108`、`Function/WebDav.cpp:161-172`、`Function/Hitokoto.cpp:10-35`
- 现象：libcurl 默认**无超时**，DNS 挂起/TCP 黑洞/服务端不响应时会一直阻塞。而调用点全在 UI 线程：`ImGui/Interface.cpp:315-319`（点"翻译"按钮）、`ImGui/Interface.cpp:485`（截图松手 → OCR + 翻译）、`ImGui/Interface.cpp:717-720`（打开设置面板取一言）、`ImGui/Interface.cpp:1460`（主循环周期刷新，`application.cpp:217` 每 20s 一次）。只要一处卡住，整个悬浮窗失去响应且无法关闭（WM_CLOSE 也在同一循环里处理）。
- 证据：
  ```cpp
  curl_easy_setopt(curl, CURLOPT_URL, &myurl);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TranslateWrite_data);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_perform(curl);            // 无 TIMEOUT / CONNECTTIMEOUT
  ```
- 建议修法：抽出统一的 `CurlEasy` RAII 包装，强制设置 `CURLOPT_CONNECTTIMEOUT_MS`（建议 2000~3000）、`CURLOPT_TIMEOUT_MS`（建议 5000~8000）、`CURLOPT_LOW_SPEED_LIMIT/TIME`（1 字节/10 秒）、`CURLOPT_NOSIGNAL, 1L`；再把这些调用搬到工作线程（见"性能与资源"）。

#### [严重] `Variable.cpp:102-103` 保存时用百度凭据覆盖有道凭据
- 位置：`Variable.cpp:99-103`（保存路径），受害点 `Function/Translate.cpp:199-208`
- 现象：保存配置时有道 API 的 `Youdao_ID`/`Youdao_Key` 被写成百度 appid/secret；`Variable::YoudaoAppid`/`YoudaoSecret_key` 从读入到消亡只存在于内存，**永远不会落盘**。重启后有道翻译必失败（401/签名错误），且用户手填的有道密钥被静默抹掉。
- 证据：
  ```cpp
  iniData->UpdateEntry("YoudaoAPI","Youdao_ID", BaiduAppid);
  iniData->UpdateEntry("YoudaoAPI","Youdao_Key", BaiduSecret_key);
  ```
- 建议修法：改为 `YoudaoAppid` / `YoudaoSecret_key`；并给 `Variable::SaveFile()` 加"写回前校验非空 + 落盘后回读比对"的自检，防止同类赋值错误再次静默通过。

#### [严重] `Translate` 的凭据成员是非拥有裸指针，指向的 `std::string` 一旦重赋值即悬垂
- 位置：`Function/Translate.h:50-54`（4 个 `const char*` 成员）、`ImGui/Interface.cpp:150-159`（首次设置）、`ImGui/Interface.cpp:1304-1307` + `:1381-1385`（保存时重新设置）
- 现象：`SetBaiduAppID(const char*)` 之类只是把 `Variable::BaiduAppid.c_str()` 这个**指向 `std::string` 内部堆缓冲的地址**存起来，不拷贝内容、不持有所有权（`Function/Translate.cpp:19-28` 的构造函数另用 `new const char*[]` 同样只存 `.c_str()` 视图）。而 `Variable::BaiduAppid` 等是全局 `std::string`：在设置面板保存时会被赋新值（`ImGui/Interface.cpp:1304-1307`），容量不足即重新分配 → 之前保存的指针立刻悬垂；后面的 `Translate_Baidu`（`Function/Translate.cpp:126-129`）再 `strcat` 这个悬垂指针就是**释放后使用（UAF）**，轻则把旧密钥/垃圾字节拼进 URL 导致签名错误，重则崩溃。同一条路径在有道凭据上完全一样。
- 证据：
  ```cpp
  // Interface.cpp:1381-1385（每次保存都会重新绑一次，旧指针从此刻起失效）
  mTranslate->SetBaiduAppID(Variable::BaiduAppid.c_str());
  mTranslate->SetYoudaoAppID(Variable::YoudaoAppid.c_str());
  // Translate.cpp:126-129（之后无条件解引用这些裸指针）
  strcat(myurl, Variable::BaiduAppid.c_str());
  ```
  （本条的**取值**是正确的——传的是 appid 而非昵称；风险在**生命周期**。确认方法：在 `Interface.cpp:150-159` 与 `:1381-1385` 各打印一次 4 个成员指针地址，并在设置面板多次修改密钥（尤其是越改越长时），观察地址是否变化而 `Translate` 内保存的仍是旧值。）
- 建议修法：`SetXxxID` 形参改 `const std::string&`，成员用 `std::string` 持有并直接 `return mBaiduAppid.c_str()`；或改成每次调用时从 `Variable::` 现取现用，彻底取消缓存。

#### [严重] `char myurl[100000]` 无界 `strcat`，长文本可打爆栈
- 位置：`Function/Translate.cpp:120-129`（百度）、`Function/Translate.cpp:199-208`（有道）
- 现象：URL 由 4 段 `strcat` 拼成，第 2 段是用户文本，没有任何长度校验。粘贴 5 万个汉字（约 150KB UTF-8，Word 里一次全选很容易超过）即写穿栈上 100KB 缓冲区；`sign[100000]` 同理。这是可稳定触发的栈溢出（Debug 下 `/RTCs` 会报 stack corruption，Release 下可能被静默覆盖）。
- 证据：
  ```cpp
  char myurl[100000];
  char sign[100000]="";
  strcat(myurl, Variable::BaiduAppid.c_str());
  strcat(myurl, English.c_str());          // 长度不可控
  ```
- 建议修法：改用 `std::string` + `reserve`；并在入口 `Translate_Baidu/Youdao` 加 `if (English.size() > 2000) return "文本过长";`（百度/有道对单次请求长度本身也有上限，超限直接本地拒绝比等 414 更快）。

#### [严重] OCR 缓冲区与截图缓冲区尺寸不一致时堆越界
- 位置：`Function/tesseract.cpp:21-22`
- 现象：`pixCreate` 用 `Variable::windows_Width/Heigth`（该值在 `Tool/Tool.cpp:335-336` 由 `screen()` 每次截图时**覆盖**为当前屏幕分辨率），随后 `memcpy(pixGetData(image), data, Height*Width*4)` 从调用方缓冲 `TData` 拷入。而 `Tool/Tool.cpp:338-340` 只在 `buf == nullptr` 时按当时的宽高分配：`if (buf == nullptr) buf = new char[H*W*4];`。**分辨率变化（换显示器、缩放/DPI 变更、远程桌面切换）后 buf 不会重新分配**，此 memcpy 就会从旧的小缓冲区读超量数据（堆越界读）+ 可能写穿目标 pix（若新分辨率更大）。
- 证据：
  ```cpp
  pix* image = pixCreate(Variable::windows_Width, Variable::windows_Heigth, 32);
  memcpy(pixGetData(image), data, Height * Width * 4);   // 无 image/null 检查、无源缓冲长度校验
  ```
- 建议修法：`IdentifyPictures` 增加 `int64_t dataBytes` 形参并 `if (data == nullptr || dataBytes < needed) return {};`；`screen()` 把缓冲区改由 `std::vector<char>` 持有，分辨率变化时 `resize` 并同步更新宽高；`pixCreate` 失败必须提前返回。

#### [严重] `Tool/Tool.cpp:252-276` `ClipboardTochar()` 存在无返回语句的路径（UB）
- 位置：`Tool/Tool.cpp:252-276`，调用点 `application.cpp:241-247`
- 现象：函数用 `int ClipboardBoll = 5; while (ClipboardBoll > 0) { ... }` 重试 5 次；成功分支在循环内 `return`，但 5 次全部失败（剪贴板被其它进程占用、内容非文本、被剪贴板管理器锁住）时循环自然结束，函数**走到末尾而没有 `return`**。`std::string` 返回值未定义 → 崩溃或随机内容，随后被喂给翻译接口。
- 证据：
  ```cpp
  std::string ClipboardTochar() {
      int ClipboardBoll = 5;
      while (ClipboardBoll > 0) { ... return CharS; }
      // ← 无 return
  }
  ```
- 建议修法：函数末尾 `return std::string();`；并让调用方（`application.cpp:241-247`）判空后放弃本次划词而不是继续流程。

#### [严重] `Sleep(5)` 等待模拟 Ctrl+C 不可靠，剪贴板状态被破坏
- 位置：`application.cpp:241-247`（划词）、`application.cpp:274-303`（替换回填），底层 `Tool/Tool.cpp:119-131`
- 现象：`CtrlAndC()` 用 `keybd_event` 连发按下/抬起且**四连发之间没有任何延时**，紧接着只 `Sleep(5)` 就读剪贴板。慢应用（Chromium 系、Office、部分终端）常常在 5ms 内还没把内容写进剪贴板，于是读到**上一次的旧内容**——表现为"翻译结果和选中的词对不上"，是最典型的用户可感缺陷。回填路径同样 `CtrlAndV` + `Sleep(5)` + 立刻还原剪贴板，目标程序可能还没取走数据就被还原，粘贴出空内容。
- 证据：
  ```cpp
  strS = ClipboardTochar();          // 备份
  CtrlAndC();
  Sleep(5);
  Variable::eng = UnicodeToUtf8(ClipboardTochar());   // 可能仍是旧内容
  ```
- 建议修法：不要"发按键 + 定时"猜，改为注册 `AddClipboardFormatListener` 或用 `GetClipboardSequenceNumber()` 轮询等待变化（上限 ~300ms），拿到新序号即返回，超时则放弃本次划词；回填后同样以剪贴板序号或短轮询确认目标读取完成再还原。

#### [严重] 翻译结果写回定长缓冲区无 NUL 终止、且给空结果时不清零
- 位置：`application.cpp:259-262`、`ImGui/Interface.cpp:315-319`、`ImGui/Interface.cpp:500-503`
- 现象：`memset(eng,0,...); memcpy(eng, Variable::eng.c_str(), Variable::eng.size());` —— 没有写终止符。缓冲区是复用的：若上一次结果更长，尾部残留旧字符，下一次渲染/复制会带上垃圾；若 `Variable::eng` 为空（翻译失败返回 `""`，或 `TranslateAPI` 的 default 分支返回未定义值），`memcpy` 长度 0，`memset` 虽清零但依赖调用顺序。同一模式在 3 处复制粘贴。
- 证据：
  ```cpp
  memset(InterFace->eng, 0, sizeof(InterFace->eng));
  memset(InterFace->zhong, 0, sizeof(InterFace->zhong));
  memcpy(InterFace->eng, Variable::eng.c_str(), Variable::eng.size());
  memcpy(InterFace->zhong, Variable::zhong.c_str(), Variable::zhong.size());  // 缺 +1，无 '\0'
  ```
- 建议修法：抽成 `CopyToFixed(char* dst, size_t cap, const std::string& s)`：`n = std::min(s.size(), cap - 1); memcpy(dst, s.data(), n); dst[n] = '\0';`，三处统一替换。

#### [严重] `Translate::TranslateAPI` 的 `default` 分支没有 return
- 位置：`Function/Translate.h:24-39`
- 现象：`switch(Variable::Translate)` 只处理 0/1/2，`default` 直接落空 → 有返回值的函数走到末尾，`ImGui/Interface.cpp:315-319` 拿到未定义值构造 `std::string`（MSVC 下常见为垃圾长度 → 崩溃）。配置文件被手改成 `Translate=3` 即触发。
- 证据：
  ```cpp
  case 2: return Translate_ReptilesYoudao(English);
  default: break;                    // ← 无 return / 无兜底
  ```
- 建议修法：`default: return Translate_Baidu(English);`（并用 `switch` 覆盖全部枚举 + 编译器 `/W4` 打开 `C4715` 警告把它变成可见问题）。

#### [严重] `TessBaseAPI::Init` 失败直接 `exit(1)`，且设置面板里显式析构+重建同一对象
- 位置：`Function/tesseract.cpp:5-12`，触发点 `ImGui/Interface.cpp:1320-1326`
- 现象：切 OCR 语言模型时执行 `mTesseract->~Tesseract(); mTesseract->Tesseract::Tesseract(ModelS[ModelIndex].c_str());`（显式析构 + 就地 placement 构造）。若 `TessData/<model>.traineddata` 缺失或损坏，构造函数里 `fprintf(stderr, ...); exit(1);` —— **整个悬浮窗进程直接消失**，无任何对话框提示；Release 构建还关闭了控制台（`CMakeLists.txt:113` `/SUBSYSTEM:WINDOWS`），stderr 用户根本看不见。
- 证据：
  ```cpp
  if (api->Init("TessData", Model) != 0) {
      fprintf(stderr, "Could not initialize tesseract.\n");
      exit(1);                        // 终止宿主进程
  }
  ```
- 建议修法：构造函数改为不抛不退出，暴露 `bool IsValid() const`；失败时记录 `TOOL::logger->error(...)` 并在 UI 上提示"模型加载失败，请在设置中选择其它语言"，回退到上一个可用模型。

#### [中等] `WebDav_List` 对 rapidxml 节点链零判空，异常响应即崩溃
- 位置：`Function/WebDav.cpp:110-127`（解析与遍历）
- 现象：`root = doc->first_node("d:multistatus")` 之后立刻 `root->first_node("d:propstat")->first_node("d:prop")->first_node(...)->value()`。服务器返回 401/404/507 时，响应体不是 multistatus（坚果云返回 HTML 错误页），`first_node` 返回 `nullptr` → 立即空指针解引用崩溃。此外 `doc` 是 `new` 出来的裸指针，若 `parse` 抛异常则既不 `delete` 也没人捕获。
- 证据：
  ```cpp
  rapidxml::xml_document<>* doc = new rapidxml::xml_document<>();
  doc->parse<0>(&response[0]);
  root = doc->first_node("d:multistatus");
  node = root->first_node("d:propstat")->first_node("d:prop")->first_node("d:getcontenttype");
  T = node->value();                  // 任一环为 nullptr 即崩溃
  ```
- 建议修法：每一级 `first_node` 都判空并 `continue`；先检查 HTTP 状态码（`CURLOPT_HEADERFUNCTION` 或 `CURLINFO_RESPONSE_CODE`），非 207 就带状态码返回错误；用 `std::unique_ptr<rapidxml::xml_document<>>` 或 `parse_error` 的 try/catch 包住解析。

#### [中等] WebDAV 下载/递归下载把服务器返回的绝对 href 再拼一次根前缀
- 位置：`Function/WebDav.cpp:250`（下载 URL 拼接）、`Function/WebDav.cpp:123-127`（列表元素本身是绝对 href）、`Function/WebDav.cpp:296-301`（递归）
- 现象：`WebDav_List` 的 `List[i]` 是服务器返回的 `href`（形如 `/dav/TranslatorKyi/xxx.txt`，已含 `WebDav_WebFile` 前缀），但 `WebDav_Download` 又拼一次：`WebDav_url + WebDav_WebFile + "/" + File` → URL 变成 `/dav/TranslatorKyi//dav/TranslatorKyi/xxx.txt`，404。递归分支 `WebDav_DownloadDirectory(directory + List[i], path + List[i])` 把绝对路径接到相对目录后面，层级越多前缀重复越多。
- 证据：
  ```cpp
  // WebDav.cpp:250
  std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
  // WebDav.cpp:300
  WebDav_DownloadDirectory(directory + List[i], path + List[i]);
  ```
- 建议修法：约定 `List` 一律返回**相对于 WebFile 根**的路径（在 `WebDav_List` 内统一剥掉 `WebDav_WebFile` 前缀，并保留 href 的 URL 编码形态），下载端只做一次拼接；对拼接结果做"不重复出现根前缀"的断言。

#### [中等] URL 解码后再拼 URL：带空格的远程路径必然失败
- 位置：`Function/WebDav.cpp:121`（`urlDecode(hrefNode->value())`）→ `:123`/`:250`/`:362`
- 现象：`WebDav_List` 把服务器 href 里的 `%20` 解码成空格（`Function/WebDav.cpp:43-65`），但后续所有使用点都把它当已编码 URL 直接拼进 `CURLOPT_URL`。坚果云里名为 `My Notes/` 的目录、含空格的文件名，列表能显示、下载/上传全 404 或 400。上传方向 `WebDav_Upload` 直接用本地文件名（含空格/中文）拼 URL，同样问题。
- 证据：
  ```cpp
  List.push_back(urlDecode(hrefNode->value()));      // 解码
  // ... :250 又原样拼回 URL，未重新编码
  ```
- 建议修法：列表里同时保存"显示用解码名"和"请求用原始 href"，或在使用点统一 `UrlEncode` 每个路径段（注意保留 `/`）。

#### [中等] `WebDav_UploadDirectory` 会死循环 / 空目录即失败
- 位置：`Function/WebDav.cpp:212-235`
- 现象：`do { ... continue; ... } while (FindNextFile(hFind, &FindFileData));` —— 分支里的 `continue` 会**跳过 while 的推进表达式**，同一项被无限重试；只有外层 `return` 才能逃出。另外 `FindFirstFile` 失败（`INVALID_HANDLE_VALUE`）只打印，紧接着无条件 `FindNextFile(hFind, ...)` 使用无效句柄。
- 证据：
  ```cpp
  hFind = FindFirstFile((Filepath + "*").c_str(), &FindFileData);
  if (hFind == INVALID_HANDLE_VALUE) { printf(...); }      // 未 return
  FindNextFile(hFind, &FindFileData);                      // 无条件调用
  ...
  do { if (...) { ...; continue; } } while (FindNextFile(hFind, &FindFileData));
  ```
- 建议修法：`FindFirstFile` 失败立即 `return false`；重写为 `while (FindNextFile(...))` 单一入口、去掉 `continue`（用 `if/else` 分支），并跳过 `"."`/`".."`（`FindFirstFile` 通配 `*` 会返回这两项，目前只能靠 `WebDav_Directory` 兜住）。

#### [中等] `WebDav_Delete` / `WebDav_CreateFolder` 与 init/cleanup 不成对
- 位置：`Function/WebDav.cpp:309-337`、`Function/WebDav.cpp:340-376`
- 现象：`WebDav_List`（`:87`/`:139`）与 `WebDav_Upload`（`:155`/`:205`）各自 `curl_global_init` + `curl_global_cleanup`，而 `WebDav_Delete`/`WebDav_CreateFolder` **一个都不调**。`curl_global_cleanup()` 之后再用 curl 属于契约违规（"必须先 init，且 cleanup 之后不得再调用任何 curl 函数"），而这两个函数恰恰在 cleanup 之后被调用（`WebDav_DownloadDirectory` 里 list 完再 create）。
- 建议修法：`main.cpp` 里在 `TOOL::SpdLogInit()` 附近做**唯一一次** `curl_global_init(CURL_GLOBAL_ALL)`，进程退出前 `curl_global_cleanup()`；删除所有函数内的 init/cleanup。

#### [中等] `curl_global_init/cleanup` 每次请求成对执行，连接与 TLS 会话全废
- 位置：`Function/Translate.cpp:269` + `:295`、`Function/WebDav.cpp:87` + `:139`、`Function/WebDav.cpp:155` + `:205`
- 现象：`curl_global_cleanup()` 会销毁全局状态（连接池、DNS 缓存、TLS 会话票据、OpenSSL 全局对象）。目录同步时 `WebDav_List` 会被递归调用 N 次 → N 次 init/cleanup + N 次完整 TLS 握手，坚果云同步明显变慢；另外 `curl_global_init` 本身在多线程下非线程安全（当前单线程侥幸可用，一旦引入工作线程就是隐患）。
- 建议修法：如上进行一次全局 init；进一步用 `CURLSH`（`CURLOPT_SHARE` 共享 DNS 与 SSL session）让多次请求复用连接。

#### [中等] 脚本引擎装载失败后留下悬垂 `engine`/`context`，脚本错误无隔离
- 位置：`AngelScript/AngelScriptCode.cpp:70-78`、`:134-150`、`:80-132`
- 现象：`GetFunction` 失败时执行 `context->Release(); engine->ShutDownAndRelease(); return;` —— 对象已销毁，但成员指针没置空也没重建，之后任何 `RunFunction` 都是在已释放对象上操作；`RunFunction` 里 `context->Prepare(Function)` 失败走同样的破坏式清理；`Execute()` 非 `asEXECUTION_FINISHED` 时只 `std::cout` 打印（Release 下控制台不可见），**且从不清栈/`Unprepare()`**，下一次 `Prepare` 会返回 `asCONTEXT_NOT_PREPARED` 之类错误，脚本从此永久失效。脚本内的任何运行时错误（越界、空指针、除零、无限循环）都会以同样的方式"吃掉"整个引擎，而 `AngelScriptMessage` 只写 `std::cout`（`AngelScriptCode.cpp:56-68`），用户与日志都看不到原因。
- 证据：
  ```cpp
  if (context->Prepare(Function) < 0) {
      context->Release();
      engine->ShutDownAndRelease();     // 悬垂，且无重建
  }
  ```
- 建议修法：失败时只做"局部失败"——`context->Unprepare()` + 返回 `false`，不要销毁引擎；`AngelScriptMessage` 改用 `TOOL::logger->error`；给脚本设置 `asIScriptEngine::SetEngineProperty(asEP_MAX_EXECUTION_TIME)` 或用 `asEP_LINE_CUE`/行回调限制无限循环，避免脚本卡死 UI 线程。

#### [中等] 脚本函数句柄与 `Translate*` 全局裸指针的生命周期没有约束
- 位置：`AngelScript/AngelScriptCode.h:13-18`（懒汉单例）、`AngelScript/AngelScriptCode.cpp:26-30`、`ImGui/Interface.cpp:1328-1334`、`application.cpp:89`
- 现象：三处问题叠加：(1) `delete AngelScriptCode::GetAngelScriptCode()` 之后，`AngelScriptCode.h:32-36` 的三个 public `asIScriptFunction*` 句柄变成了悬垂值，只有下次 `GetAngelScriptCode()` 才重建；(2) `AngelScriptTranslate` 是全局裸指针，`application.cpp:89` 在 `Translate` 对象创建之前就赋值，若脚本在 `Interface` 构造完成前被调用（`GetFunction` 成功即可能）就是空指针解引用；(3) `RunFunction` 每次都用可能已失效的句柄去 `Prepare`。
- 建议修法：单例改用 `std::unique_ptr` + `GetAngelScriptCode()` 返回引用，禁止外部 `delete`（改成显式 `Reload()`）；`AngelScriptTranslate` 改为在 `Interface` 构造完成后设置，并在包装函数里 `if (!AngelScriptTranslate) return {};` 兜底。

#### [中等] opcode 解释器越界读与静默容错
- 位置：`Function/opcode.cpp:107`、`:110-172`
- 现象：`switch (Control_Param[Code[0]])` 用 `operator[]`，未知命令名会被静默插入成枚举值 0（恰好等于 `Bool`）；随后 `Code[1]`…`Code[6]` 全部无 `size()` 校验，脚本文件里写一行 `Set`（缺少参数）就越界读取 `std::vector<std::string>` 元素（UB，Debug 下 `_STL_VERIFY` 直接崩）。`NewDLL` 分支 `LoadLibrary(Code[1].c_str())` 对脚本可控路径不做任何限制，脚本可加载任意 DLL 并调用其导出函数 —— 这本身就是"脚本即任意代码执行"的设计，需要在文档和 UI 上明确告知。
- 证据：
  ```cpp
  switch (Control_Param[Code[0]]) {   // operator[] 插入默认值 0
  case Fenum::Bool:
      if (Code[1] == "True") ...       // Code.size() 未校验
  ```
- 建议修法：`Control_Param.find(Code[0])` + 未命中直接报错返回；每个 `case` 开头 `if (Code.size() < n) { 报错; break; }`；`LoadLibrary` 限制在工作目录 `Dll/` 下并做路径规范化（拒绝 `..`）。

#### [中等] `InitOpcode` 从未被调用，整条 opcode 链路是死代码
- 位置：`Function/opcode.cpp:38-71`（定义）；全仓库 grep `InitOpcode` 仅此一处命中
- 现象：`STu`、6 个 `PileUp`、`Control_Param` 映射表全都没被初始化，`Control_Param` 恒为空 → 即便将来调用 `Opcode()`，所有命令都会落到默认值 0 分支。当前仓库里 opcode 与 `PileUp` 是"看起来存在、实际不生效"的模块，`Opcode/Script.as` 走的是 AngelScript（`Variable::Script` + `AngelScriptCode.cpp:101` 读 `./Opcode/<Script>.as`），两套脚本机制并存且不同源。
- 建议修法：明确取舍——要么在 `main.cpp` 中 `InitOpcode(...)` 并在文档里说明两套脚本的分工，要么删除 `opcode.*`/`PileUp.h` 以免误读。

#### [中等] 每帧/每次识别都重建 leptonica 全屏 pix，OCR 前无降采样
- 位置：`Function/tesseract.cpp:21-24`、调用点 `ImGui/Interface.cpp:485`
- 现象：每次识别都 `pixCreate` 一整屏 32 位图（2560×1440 ≈ 14.7MB）再 `memcpy` 全屏数据、`boxCreate` + `pixClipRectangle` 裁出选区，随后立刻销毁。全屏拷贝 + 分配发生在鼠标松开的那一帧，UI 线程直接停顿；选区通常只占屏幕很小一块，绝大部分拷贝是浪费。
- 建议修法：只拷贝选区所在行（`for (y = y0; y <= y1; ++y) memcpy(dst + ..., data + y*stride + x0*4, (x1-x0+1)*4)`）；或按选区尺寸 `pixCreate(w, h, 32)` 直接构造，省掉全屏 pix 和裁剪两步；TessBaseAPI 已复用（好），可再考虑 `SetVariable("tessedit_pageseg_mode", ...)` 针对单行/单词场景提速。

#### [轻微] `Translate` 构造函数 `new` 出的 `const char*[]` 永不释放
- 位置：`Function/Translate.cpp:19-28`、析构 `:31-34`
- 现象：`Baidu_items = new const char*[Variable::Baiduitems.size()];`（有道同样）在空析构函数里泄漏；`Translate` 在 `Interface` 构造时创建、析构时 `delete`（`ImGui/Interface.cpp:150`/`:162-183`），进程生命周期内只泄漏一次，属于"一次性少量泄漏"，但会污染 ASAN/Dr.Memory 结果。
- 建议修法：成员改 `std::vector<std::string>`，`const char**` 视图按需生成；或补 `delete[] Baidu_items;`。

#### [轻微] `base64_encode` 空输入时长度下溢；`UrlDecode` 的 `assert` 在 Release 失效
- 位置：`Function/Translate.cpp:473`、`Function/Translate.cpp:88`
- 现象：`result->length - 1`（去掉尾部换行）在 `length == 0` 时下溢为 `SIZE_MAX`，若将来被复用（比如 `aes_decrypt` 链路）会构造出巨长字符串。`UrlDecode` 里 `assert(i + 2 < length)` 只在 Debug 生效，Release 下畸形 `%` 序列会读越界。
- 证据：
  ```cpp
  bptr->length -= 1;                    // 若 length==0 则下溢
  assert(i + 2 < length);               // Release 下无效
  ```
- 建议修法：加 `if (bptr->length == 0) return;`；把 `assert` 换成显式 `if (...) { 报错并返回空; }`。

#### [轻微] `PileUp<T>` 用 `delete` 释放 `new T[]`；`MapVariable::Set` 声明返回 `void*` 却无 return
- 位置：`Function/PileUp.h:18-20`、`Function/PileUp.h:73-75`
- 现象：`~PileUp() { delete mPileUp; }` —— 数组必须 `delete[]`，否则 UB（元素析构不执行，`PileUp<std::string>` 直接泄漏字符串缓冲）。`MapVariable::Set` 返回类型为 `void*` 但函数体没有 `return`，任何调用者拿到未定义值（当前无人调用，属埋雷）。
- 证据：
  ```cpp
  ~PileUp() { delete mPileUp; }
  void* Set(std::string name, U V) { mVariable[name] = V; }   // 无 return
  ```
- 建议修法：`delete[] mPileUp; mPileUp = nullptr;`；`Set` 改成 `void` 或 `return &mVariable[name];`。

#### [轻微] `IdentifyPictures` 结果未判空；`IdentifyPictures` 返回 `Text` 依赖 `delete[]` 实现细节
- 位置：`Function/tesseract.cpp:26-32`
- 现象：`char* outText = api->GetUTF8Text();` 之后直接 `Text = outText;` —— 识别无文本/失败时 Tesseract 可能返回 `nullptr`，`std::string(nullptr)` 是 UB（MSVC 直接崩）。`delete[] outText` 与 Tesseract 用 `new[]` 分配的实现约定一致，但属于跨库内存约定，升级 Tesseract 需复核。
- 证据：
  ```cpp
  char* outText = api->GetUTF8Text();
  Text = outText;                       // 未判 NULL
  ...
  delete[] outText;
  ```
- 建议修法：`Text = outText ? outText : "";`；`delete[]` 保留但加注释说明来源（`TessBaseAPI::GetUTF8Text` 文档明示 `new[]`）。

### 二、性能与资源

1. **同步网络 + 单线程 UI**（最影响体感）：`application.cpp:194-232` 每帧 `sleep_for(10ms)`，`ImGui/Interface.cpp:315-319`、`:485`、`:717-720`、`:1460` 全部在同一条线程里发请求。一次翻译 ≈ DNS + TLS + 往返（国内 API 200~800ms，坚果云/一言跨境时 1~3s），期间窗口完全无响应、不重绘。
   - 修法：把翻译/OCR/WebDAV/一言四类请求放进一个 `std::jthread` 工作线程 + `std::future`/任务队列，UI 侧只读不可变结果（`std::atomic<bool> ready` + 双缓冲 `std::string`）；或先做最小改动——`CURLOPT_TIMEOUT_MS` + 按钮置灰 + 显示"翻译中…"转圈。
2. **curl 全局状态反复重建**：见前文 init/cleanup 条目；同步目录时开销随文件数线性放大，应一次 init + `CURLSH` 共享连接。
3. **`WebDav_Directory` 全量列目录做线性比对**（`Function/WebDav.cpp:68-79`）：每上传一个文件就重列一次远端目录，上传 N 个文件 = N 次 PROPFIND。建议批量上传前列一次，用 `std::unordered_set` 缓存已存在的路径。
4. **`WebDav_UploadDirectory` 的 `continue` 死循环**（`:224-235`）：一旦命中，UI 线程永久卡死（无超时、无取消），危害高于普通性能问题。
5. **opcode 分词 O(n²)**（`Function/opcode.cpp:88-95`，内层 `substr` 重建 + 索引复位）：脚本行短，当前影响可忽略；但 `InitOpcode` 未调用，属"未启用模块"，修优先级低。
6. **OCR 全屏 pix 拷贝**（`Function/tesseract.cpp:21-22`）：见前文，单次约 15MB 的分配+拷贝发生在交互帧上。TessBaseAPI 已复用（`ImGui/Interface.cpp:148` 只创建一次，`:1320-1326` 仅在切换模型时重建），这点做对了。
7. **`screen()` 缓冲区只增不减**（`Tool/Tool.cpp:338-340`）：`buf` 首次按屏幕尺寸分配，之后永不释放、分辨率变化也不重分配（同时是"严重"级越界缺陷）。`TData` 同样全项目无 `delete`（`ImGui/Interface.cpp:1512-1518` 把它指向主循环 buffer）。建议改为 `std::vector<char>` 成员，随分辨率 `resize`。
8. **剪贴板轮询写死 5 次**（`Tool/Tool.cpp:252-276`）：`while (ClipboardBoll > 0)` 无 `Sleep`，是"忙等重试"，通常在同一毫秒内耗尽 5 次尝试，等于没有重试。应改为带间隔轮询序列号。

### 三、安全与隐私（密钥、SSL、日志泄露）

1. **[严重] 真实账号密码与 API 密钥明文入库，并被构建系统复制到输出目录。**
   - `Data.ini:13-15`：`url=https://dav.jianguoyun.com/dav/`、`username=1779690036@qq.com`、`password=ahbhd5pd94e3qpnm`（坚果云**应用密码**，可直接读写该账号全部 WebDAV 数据）。
   - `Data.ini:24-25`：`Youdao_ID=1adbf86823e273df`、`Youdao_Key=RO9RM5iece4g5iCbOxjd0hb9EDsvNMFr`；`Data.ini:31-32`：`Baidu_ID=20210925000956550`、`Baidu_Key=os4RtAbGDCDhvgWvGSPu`。
   - `CMakeLists.txt:53`：`execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})` —— 每次构建都把含密钥的 ini 复制到 `bin/`，随打包目录一起分发。
   - 建议：把 `Data.ini` 加入 `.gitignore` 并从仓库历史中移除（`git filter-repo`）、**立刻在坚果云/百度/有道后台吊销并重置上述全部凭据**、仓库只保留 `Data.example.ini`；运行时密钥改存 `%APPDATA%`，用 DPAPI（`CryptProtectData`）加密。
2. **[严重] 百度翻译走 HTTP 明文，凭据与译文全程裸奔。** `Function/Translate.cpp:120` 的 `myurl` 以 `http://` 开头，而 URL 里 `strcat` 了 appid 与 `secret_key`（`:126-129`）。同网段即可抓包拿到密钥（可用来盗刷配额）和所有翻译内容。建议改用 `https://fanyi-api.baidu.com/api/trans/vip/translate`（官方支持 HTTPS）。
3. **[中等] 硬编码密钥与固定凭据出现在源码。** `Function/Translate.cpp:542` `std::string key = "fsdsogkndfokasodnaso";`；`Function/Translate.cpp:570-571` 固定 cookie；`Function/Translate.cpp:281` 硬编码 UA（伪装 Edge/115）。爬虫接口的 key 属公开共享密钥（风险低），但它与"绕过官方接口直连网页版"的实现绑定，随时可能因对方改版失效，且带固定 cookie/UA 有被风控的风险。
4. **[中等] 未显式设置 SSL 校验选项，也未固定 CA。** 全项目 grep `SSL_VERIFYPEER|SSL_VERIFYHOST` 零匹配。libcurl 默认 `VERIFYPEER=1`、`VERIFYHOST=2`，Windows 上走 Schannel 用系统证书库，**因此不构成"校验被关闭"**，但：(a) 依赖构建期后端选择，换成 OpenSSL 后端且未设 `CURLOPT_CAINFO` 时可能变成 `CURLE_PEER_FAILED_VERIFICATION` 静默失败；(b) 无任何证书固定/失败日志，出问题无法诊断。建议显式设置 `CURLOPT_SSL_VERIFYPEER, 1L`、`CURLOPT_SSL_VERIFYHOST, 2L`，JSON/WebDAV 接口开 `CURLOPT_USE_SSL, CURLUSESSL_ALL`（禁止明文回退），失败时记录 `curl_easy_strerror`。
5. **[中等] Release 构建关闭控制台，错误信息全部消失。** `CMakeLists.txt:113` `/SUBSYSTEM:WINDOWS` 使 stdout/stderr 不可见，而多处错误处理只用 `printf`/`fprintf`/`std::cout`（`Function/WebDav.cpp:214-216`、`Function/tesseract.cpp:8`、`AngelScript/AngelScriptCode.cpp:56-68`）。同时 `TOOL::logger`（`Tool/Tool.cpp:7-19`）已有 `logs/Error.txt` 与 `trace` 级文件 sink，功能层却几乎不调用它。建议：功能层错误统一走 `TOOL::logger->error(...)`，并在设置面板加"打开日志目录"。
6. **[中等] 日志可能记录敏感内容（待确认）。** 目前功能层没有把 URL/凭据写进日志的语句（已 grep `Translate.cpp`/`WebDav.cpp` 未见 `logger`/`cout` 打印凭据），但 `WebDav.cpp:91` 的 URL 若将来被纳入日志就会带出用户名。确认方法：`grep -n "logger\|printf\|cout" Function/WebDav.cpp Function/Translate.cpp` 复核；改进方向是加一个 `RedactUrl()` 在打印前抹掉 `user:pass@` 与 query 中的密钥字段。

### 四、可读性与可维护性

1. **巨型函数**：`Translate_Baidu`（`Function/Translate.cpp:101-177`）、`Translate_Youdao`（`:179-255`）各约 75 行且逐行同构（改一处必须改两处）；`WebDav_List`（`Function/WebDav.cpp:82-146`）把"发请求 + 解析 XML + 拼路径 + 交换首尾元素"塞在一起。建议按"构造请求 / 执行 / 解析"三段拆分，百度与有道合并为 `Translate_Official(kind, text)`。
2. **curl 样板重复 6 次**（`Translate.cpp:153-156`、`:565-605`、`WebDav.cpp:98-108`、`:161-172`、`:250-260`、`Hitokoto.cpp:20-30`）：每个调用点都要手写 `curl_easy_init`/一组 `setopt`/`perform`/`cleanup`，于是"漏设超时"能在 6 个地方同时发生——这正是当前缺陷的根因。抽出 `CurlRequest{ url, method, headers, body, timeout }` + RAII 是最有价值的一次重构。
3. **错误处理风格三套并存**：返回中文常量 `"错误"`（`Translate.cpp:159-166`）、返回 `"No Words Present"`（`:109`）、返回空串（`Hitokoto.cpp` 失败分支、`WebDav_*` 部分分支）、以及 `exit(1)`（`tesseract.cpp:8`）。调用方（`Interface.cpp:315-319`）无法区分"网络失败/密钥错误/无结果"，只能把 `"错误"` 当成译文显示给用户。建议定义 `struct TranslateResult { bool ok; std::string text; std::string errCode; }`。
4. **魔法数字与硬编码字符串遍布**：`100000`（`Translate.cpp:120`/`:125`/`:199`/`:204`）、`5`（`Tool.cpp:253` 剪贴板重试）、`5`（`Sleep(5)`）、`17`/`1`（`Interface.cpp:732` `MakeUp == 17`）、语言代码分散在 `Data.ini:26`/`:33` 与 `Translate.h:40`。建议集中到常量头文件，语言代码用表格映射百度/有道各自方言（现在 `Baidu_items` 与 `Youdao_items` 的取值域不同，切引擎时下标语义会错位，见下一条）。
5. **`TranslateName[3]` 与两套 `*_items` 的下标耦合**（`Translate.h:40-43`）：`Baidu_items`/`Youdao_items` 是两个语言数不同的裸数组，界面用同一个下标（`Variable::From/To`）索引两者。`Data.ini:26` 有 11 项、`:33` 有 11 项但取值不同（`fra` vs `fr`），一旦两者数量不一致，`Interface` 的下标可能越界。建议合并为 `struct LangMap { const char* name; const char* baidu; const char* youdao; }` 单表。
6. **死代码与半成品**：`Function/opcode.cpp` 全模块未启用（`InitOpcode` 无调用）；`Function/PileUp.h:42-45` 的 `StructVariable` 全项目无使用；`Function/Translate.cpp:565-611` 的 `translate()` 从未返回译文（`:610 return "";`）且带 `// Parse the JSON response // ...` 未完成注释；`Function/Translate.h` 里 `aes_encrypt/aes_decrypt/get_form_data` 只服务于这个半成品。建议要么完成并接入，要么标注 `// TODO: 未完成` 并移到独立目录，避免审阅者误以为可用。
7. **`Opcode/Script.as` 三份函数高度重复**：`ScreenshotFunction`（`Opcode/Script.as:12-23`）与 `ChoiceFunction`（`:26-37`）逐字相同，`ReplaceFunction`（`:1-9`）仅少了 `WordSeparation` + `Autowrap`。建议抽一个带参数的公共函数，并给 `.as` 加基础注释说明可用 API（`GetInput/SetOutput/TranslateAPI/...` 来自 `AngelScriptCode.cpp:161-225`）。
8. **脚本路径依赖工作目录**：`AngelScript/AngelScriptCode.cpp:101` 的 `"./Opcode/" + Variable::Script + ".as"` 在从别的 cwd 启动（快捷方式"起始位置"为空、任务栏固定、被其它程序 ShellExecute）时读不到文件，且没有任何错误提示；也没有热重载（改完脚本必须重启）。建议用 `GetModuleFileName` 推导 exe 目录，并记录脚本文件 mtime，变更时自动重建模块。

### 五、可添加的功能建议（务实可落地）

1. **统一 `CurlClient` 层（收益最高，一次解决 5 类缺陷）**：RAII 句柄 + 强制超时（连接 3s / 总 8s / 低速 1B·10s）+ `NOSIGNAL` + 显式 SSL 校验 + `CURLINFO_RESPONSE_CODE` 后置检查 + `curl_easy_strerror` 映射为中文错误码 + 可选重试（仅对 429/5xx/超时做指数退避，最多 2 次，避免重试风暴）。所有 6 个调用点改走它。
2. **异步化 + 结果缓存**：把翻译/OCR/WebDAV 放到工作线程，UI 显示"翻译中…"占位与取消按钮；同时加一层 `unordered_map<key, result>`（key = 引擎 + from + to + 文本哈希）与磁盘缓存，重复划同一句话零延迟、零配额消耗，离线也能看到上次结果。
3. **OCR 体验与精度升级**：选区直接构造小 pix（不再全屏拷贝）；支持 `chi_sim+eng` 多语言组合；识别前做灰度/二值化/放大（Tesseract 对小字与深色背景识别率差）；加"识别区域显示/重试"；把 `IdentifyPictures` 的失败原因（模型缺失、选区为空、无文本）区分开提示，替代当前 `exit(1)`。
4. **配置与密钥安全**：密钥移出 `Data.ini`（改存 `%APPDATA%` + DPAPI 加密），`Data.example.ini` 入库；设置面板增加"测试连接"按钮（分别 ping 百度/有道/坚果云/一言并显示 HTTP 状态与耗时），把"密钥填错"从"翻译结果是个'错误'"变成明确诊断。
5. **WebDAV 同步健壮化**：先 `WebDav_List` 一次建远端索引再批量 diff 上传（免去 N 次 PROPFIND）；上传/下载全部路径编码；对 401/404/507 给明确文案；同步进度条 + 可取消；冲突策略（按 mtime 取新 / 询问）可选。
6. **脚本层易用性与安全**：`Opcode/*.as` 热重载（监听 mtime）+ 内置脚本编辑器 + 示例脚本库；脚本错误弹窗显示文件名/行号/调用栈（替换 `std::cout`）；给脚本执行加上步数/时间上限（`asEP_MAX_EXECUTION_TIME`）防止 UI 卡死；在 UI 中明确标注"脚本可加载任意 DLL（`opcode.cpp:151-157`）"的风险提示。



---

## 配置与本地化层 · 体检报告

> 审查对象：`Variable.cpp`（实测 420 行）、`Variable.h`（223 行）、`ini.h`（602 行）、`Data.ini`、`Language/zh.ini`、`Language/eng.ini`、`CMakeLists.txt`、`FilePath.h`、`main.cpp`；契约抽查 `ImGui/Interface.cpp`、`Vulkan/device.cpp`、`Vulkan/instance.cpp`。
> 方式：逐行精读 + 字节级编码核对，本机未编译。所有行号为实测行号。

### 一、缺陷与风险

#### [严重] SaveFile 把百度密钥写进了有道的键，有道翻译必然鉴权失败
- 位置：`Variable.cpp:102-103`
- 现象：读取时按 `Youdao_ID`/`Youdao_Key` 读取（`Variable.cpp:34-35`），界面保存时也把 `SetYoudaoID`/`SetYoudaoKey` 存进了 `Variable::YoudaoAppid`/`YoudaoSecret_key`（`ImGui/Interface.cpp:1306-1307`）。但写回磁盘时来源变量写成了百度的两个。用户在设置里填好的有道 ID/Key 每次点保存都会被百度 ID/Key 覆盖，重启后有道的 AppID/Secret 变成百度的值，鉴权不可能通过；而这是**静默数据破坏**，界面不会有任何提示。
- 证据（关键代码原文）：
  ```cpp
  //保存 百度ID Key
  iniData->UpdateEntry("BaiduAPI", "Baidu_ID", BaiduAppid);
  iniData->UpdateEntry("BaiduAPI", "Baidu_Key", BaiduSecret_key);
  //保存 有道 ID Key
  iniData->UpdateEntry("YoudaoAPI", "Youdao_ID", BaiduAppid);        // <-- 应为 YoudaoAppid
  iniData->UpdateEntry("YoudaoAPI", "Youdao_Key", BaiduSecret_key);  // <-- 应为 YoudaoSecret_key
  ```
- 建议修法：改成 `YoudaoAppid` / `YoudaoSecret_key`。顺手把这类「键名 + 来源变量」成对数据抽成一张 `{section, key, getter, setter}` 表，读写共用同一张表，从结构上杜绝读写错位。

#### [严重] 配置项值为空时点「保存」抛异常且无人捕获，整个程序退出
- 位置：`ini.h:483-486`（抛点）、`Variable.cpp:80-133`（调用点）、`ImGui/Interface.cpp:1371`（无 try）、`main.cpp:34-41`（catch 覆盖不到）
- 现象：`UpdateEntry` 的判据是"当前值的字符串长度"，不是"键是否存在"。因此只要用户在 `Data.ini`（或界面对应的输入框）里把**任意一项**留成 `Key=` 这种空值——例如 `Screenshotkey =`、`WebDav_password =`、`FontFilePath =`、`Set / Language =`——保存时就会抛 `std::runtime_error`。`ImGui/Interface.cpp:1371` 的 `Variable::SaveFile();` 外面没有任何 try/catch，异常穿过 ImGui 渲染帧、穿过 `app->run()`，被 `main.cpp:34-41` 的 catch 接住并弹 MessageBox 后**结束进程**。用户视角是「点一下保存，程序没了」。注意 `VulkanDeviceName` 出厂就是空值，作者已经为它单独打了补丁（`Variable.cpp:151-159`），但其 60 多个键都没有同等保护。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:481-488（UpdateEntry，注意 operator[] 在判定之前就已把空条目插入 map）
  inline void INIReader::UpdateEntry(const std::string& section,
      const std::string& name, const T& v) {
      if (!_values[section][name].size()) {
          throw std::runtime_error("key '" + std::string(name) +
              "' not exist in section '" + section + "'.");
      }
      _values[section][name] = V2String(v);
  }
  ```
  ```cpp
  // Variable.cpp:135-138（作者自己的注释已确认这个陷阱）
  //inih 这两个接口是互补的，而且判定看的是"值"不是"键在不在"：
  //UpdateEntry 在当前值长度为 0 时抛异常（键不存在、以及键在但值为空，都会抛），
  ```
- 建议修法：给 ini.h 增加 `SetEntry()`（存在则改、不存在则插、空值也接受），`UpdateEntry`/`InsertEntry` 只作为内部实现；`ImGui/Interface.cpp:1371` 外面包一层 try/catch 并弹错误提示；调用 `write_Gai` 前先做一次「旧值为空」的预检，避免半路抛异常后对象状态已被污染。

#### [严重] 颜色数组读取无长度校验：可以越界写，也可以少读
- 位置：`Variable.cpp:57-61`，目标数组 `Variable.h:98`（`extern unsigned char ScreenshotColor[4]`）
- 现象：`GetVector<unsigned int>` 会把 `ScreenshotColor = 0 0 0 100` 按空白切成任意个数的词，`for` 循环用的是**输入向量的长度**而不是数组容量。写 5 个及以上数字时循环写越界（破坏相邻全局量），写 3 个及以下时剩余通道保持旧值/0，界面颜色与用户配置不一致却无任何提示。
- 证据（关键代码原文）：
  ```cpp
  std::vector<unsigned int> LScreenshotColor = iniData->GetVector<unsigned int>("Set", "ScreenshotColor");
  for (size_t i = 0; i < LScreenshotColor.size(); i++)
  {
      ScreenshotColor[i] = LScreenshotColor[i];
  }
  ```
- 建议修法：`for (size_t i = 0; i < LScreenshotColor.size() && i < 4; ++i)`，并对 `size() != 4` 的情况记一条日志/补默认值；更彻底的做法是加一个 `GetFixedVector<T, N>` 帮助函数统一处理「长度不足补默认、超出截断」。

#### [严重] 源码全部无 BOM 且工程没有任何 `/utf-8` 开关，编译期与运行期双重中文风险
- 位置：`CMakeLists.txt:1-30`（无字符集开关）、`FilePath.h:2`、`main.cpp:2-6`
- 现象：全仓库 67 个含中文的文件（源码 + ini）经字节级校验都是 **UTF-8 无 BOM**（唯一带 BOM 的是 `CMakeSettings.json`）。MSVC 在中文 Windows 上会按 GBK(936) 解码这些源文件，`E6 96 87`（"文"）这类字节序列会被当成 GBK 双字节吃掉**后续的 ASCII 字节**——注释末尾连带吃掉下一行代码的开头、字符串里的中文吃掉收尾引号，就会变成语法错误。这正是作者自己写在 `main.cpp:2-6` 的现场记录。另一方面，字符串里的裸中文会被按 GBK 编进二进制，而程序里唯一显式使用 `u8` 的地方是 `ImGui/Interface.cpp:1392` 的 `u8"GitHub"`，两种编码在同一个 ImGui 界面里混用，必然出现方块/乱码。
- 证据（关键代码原文）：
  ```cpp
  // main.cpp:2-6
  /* 换了电脑编译说有语法错误：目前知道的是：中文输出的问题。解决方法：在中文结尾后加一个 "空格" 或 字母 。 */
  ```
  ```
  CMakeLists.txt 全文件搜索 /utf-8、/source-charset、/execution-charset —— 零命中
  ```
- 建议修法：`CMakeLists.txt` 上加 `target_compile_options(TranslatorKyi PRIVATE /utf-8)`（或 `add_compile_options(/utf-8)`）；同时把源文件统一转成 **UTF-8 with BOM** 作为双保险，这样即使别人用 IDE 直接编译（绕过 CMake）也不会踩坑。

#### [中等] ReadFile 逐个 Get 零默认值：任何一个键被删/写错就立刻崩溃，且异常无人接
- 位置：`Variable.cpp:10-63`（62 处单参 `Get`），`Variable.cpp:8`、`:74`
- 现象：`ReadFile` 只有 Vulkan 两项用了三参默认值版本；其余全部是无默认值版本，缺键即抛 `runtime_error`。而 `main.cpp:23-24` 的 `Variable::ReadFile(iniData);` 与 `Language::ReadFile(...)` 都在 try 之外，异常从 `main` 逃逸 → `std::terminate` → 崩溃（不弹窗、无日志，用户和作者都拿不到原因）。
- 证据（关键代码原文）：
  ```cpp
  // Variable.cpp:8
  inih::INIReader* iniData = new inih::INIReader(IniPath);
  // Variable.cpp:10
  PopUpNotificationBool = iniData->Get<bool>("Hitokoto", "PopUpNotificationBool");
  // main.cpp:23-24（注意：不在 try 内）
  Variable::ReadFile(iniData);
  Language::ReadFile(Variable::Language);
  ```
- 建议修法：所有键改成三参默认值版本 `iniData->Get<T>(sec, key, default)`，并在 `ReadFile` 内对空串做「用默认值补上」的处理；`main.cpp` 里把 `ReadFile`/`Language::ReadFile` 也包进 try/catch，失败时用 MessageBox 给出「哪个文件/哪个键」的明确提示。

#### [中等] Language::ReadFile 用相对路径 + 栈对象 + 零默认值，缺文件即崩溃、缺键即崩溃
- 位置：`Variable.cpp:241`、`Variable.cpp:243-328`
- 现象：`./Language/zh.ini` 这种相对路径依赖**当前工作目录**。用户从桌面快捷方式、或从资源管理器双击生成的 exe、或经过开机自启（`Set\Startup`）启动时，CWD 未必是 exe 所在目录，`INIReader` 构造函数里 `ParseError()` 立刻抛 `"ini file not found."`（`ini.h:302`），程序在显示任何界面之前就死了。82 个键又全是单参 `Get`，缺任意一个键同样抛。既没有「文件缺失时把界面文案留空继续跑」的降级路径，也没有回退到英文的兜底。
- 证据（关键代码原文）：
  ```cpp
  void ReadFile(std::string FilePath) {
      inih::INIReader iniData = inih::INIReader("./Language/" + FilePath + ".ini");
      TranslationKey = iniData.Get<std::string>("Translate","TranslationKey_");
  ```
- 建议修法：用 `GetModuleFileNameW` 取 exe 目录拼绝对路径；读失败时回退到内置的英文默认文案（哪怕只硬编码一份 `Language::` 默认值表），并弹一次提示。这一项和多语言切换是同一个坑的两面——`ImGui/Interface.cpp:1338` 的语言切换也走同一个函数。

#### [中等] 异常的 RAII 缺失：每次都 `new` + 手写 `delete`，中途抛异常必然泄漏
- 位置：`Variable.cpp:8` / `:74`（读）、`Variable.cpp:78` / `:164`（写）
- 现象：`INIReader` 用裸 `new` 创建、函数末尾裸 `delete`。`ReadFile` 里任意一个 `Get` 抛异常、`SaveFile` 里任意一个 `UpdateEntry` 抛异常（第一条中等风险就描述了触发条件），都会跳过 `delete` 造成泄漏。虽然进程多半紧接着就崩了、泄漏不致命，但这是**异常安全**上的坏模式，也让「加 try/catch 优雅处理」这条路一直走不通。
- 证据（关键代码原文）：
  ```cpp
  inih::INIReader* iniData = new inih::INIReader(IniPath);   // :8 / :78
  ...
  delete iniData;                                            // :74 / :164
  ```
- 建议修法：改成栈对象 `inih::INIReader iniData(IniPath);`（`Language::ReadFile` 已经是这么写的，直接对齐即可），或 `auto iniData = std::make_unique<inih::INIReader>(IniPath);`。

#### [中等] 写回是"整文件重建"：用户手写的注释、空行、键顺序全部被吃掉
- 位置：`ini.h:585-599`（`write_Gai`）、`ini.h:565-582`（`write`），输出顺序取自 `ini.h:318-324` 的 `Sections()` 与 `ini.h:331-338` 的 `Keys()`
- 现象：解析阶段注释文本根本不进入 `_values`（`ini.h:133-137` 对 `INI_START_COMMENT_PREFIXES` 的处理只是"跳过这一行"，行内注释在 `ini.h:61-69` 的 `find_chars_or_comment` 里被截断丢弃），写回时按 `std::set` 的**字典序**遍历段与键，统一输出 `key=value`。所以 `Data.ini` 里所有 `;` 注释、空行、键的原有顺序、`=` 两侧的对齐空格，在用户第一次点保存后就永久消失，键顺序还会被重排成字母序（例如 `[Set]` 段开头变成 `DisplayTime`）。这对一个**用户需要手填 API 密钥**的配置文件是实打实的体验损伤——注释里写着"去哪儿申请密钥"的说明会被抹掉。另外 `write_Gai` 直接 `std::ofstream out; out.open(filepath);` 截断覆盖，没有「写临时文件 + rename」的原子性：写到一半崩溃/断电会留下半个文件，而下次启动 `ReadFile` 正好会因为缺键崩溃。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:585-599（作者改的版本 :584 注释写着"修改后的 write()"，去掉了 write() 里"文件已存在就抛"的检查）
  inline static void write_Gai(const std::string& filepath,
      const INIReader& reader) {
      std::ofstream out;
      out.open(filepath);
      if (!out.is_open()) { throw std::runtime_error("cannot open output file: " + filepath); }
      for (const auto& section : reader.Sections()) {
          out << "[" << section << "]\n";
          for (const auto& key : reader.Keys(section)) {
              out << key << "=" << reader.Get(section, key) << "\n";
          }
      }
      out.close();
  }
  ```
- 建议修法：短期至少加文件头注释（把「本文件由程序生成，注释会被覆盖」写清楚）；中期改成读-改-写式的最小改写（只替换目标键所在行、保留其余原文），或写临时文件 `Data.ini.tmp` 后 `std::filesystem::rename` 原子替换。

#### [中等] 解析回调里抛异常会穿过 C 风格缓冲区，且重复键直接让程序崩在启动阶段
- 位置：`ini.h:547-556`（`ValueHandler` 里的重复键检测）、`ini.h:79-180`（`ini_parse_stream`，line 缓冲区是 `malloc`/`free`）、`ini.h:186-194`（`ini_parse`，`fclose(file)` 在 :193）
- 现象：`INI_STOP_ON_FIRST_ERROR` 为 1（`ini.h:37`），解析器遇到重复键会从回调里抛 `"duplicate key ..."`。这个异常会**穿过 `ini_parse_stream`**：它内部 C 风格的 `malloc` 缓冲区不会被释放，`ini_parse` 末尾的 `fclose(file)`（`ini.h:193`）也不会执行。更麻烦的是这条路径发生在 `INIReader` 构造函数（`ini.h:287-290`）里——用户手工复制粘贴配置时不小心留了两行同名键，触发的是启动即崩，而且崩在配置读取阶段（没有日志、没有弹窗）。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:547-556
  if (reader->_values[section][name].size() > 0)
      throw std::runtime_error("duplicate key '" + std::string(name) + "' in section '" + section + "'.");
  ```
- 建议修法：解析期的错误不要抛异常，改成记录到 `_error` 并返回错误码（让 `ParseError()` 统一报），或至少在每个 `catch` 之外保证资源释放；重复键按"后者覆盖前者 + 记一条警告"处理更符合 ini 的惯例。

#### [中等] 三参默认值版本会把"格式错误"也当成"键缺失"静默吞掉
- 位置：`ini.h:388-397`
- 现象：三参版本内部是 `try { return Get<T>(...); } catch (std::runtime_error&) { return default_v; }`。这意味着用户把 `VulkanDeviceMode` 写成 `VulkanDeviceMode = abc` 时，程序不会提示"值不合法"，而是安静地退回默认值 `AutoBest`——用户的设置"没生效"且没有任何线索。更微妙的是：这个 catch 同时盖住了「键缺失」`ini.h:361-364`、`Converter` 解析失败 `ini.h:520-532`、甚至 `BoolConverter` 的非法布尔值 `ini.h:534-545`，三类错误被压成同一个结果。反过来，`GetVector` 对**空值**返回空 vector 而不抛异常（`ini.h:418-428` 先 `Get(section,name)` 得到空串，`istream_iterator` 切出 0 个词，`try` 块内空循环直接返回 `vs{}`），与 `Get<std::string>` 对空串正常返回、`Get<int>` 对空串抛异常的行为三者各不相同，同一份配置里空值的语义不统一。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:388-397
  template <typename T>
  inline T INIReader::Get(const std::string& section, const std::string& name,
      T&& default_v) const {
      try {
          return Get<T>(section, name);
      }
      catch (std::runtime_error& e) {
          return default_v;
      }
  }
  ```
- 建议修法：区分「键不存在」与「键存在但解析失败」两种错误类型（自定义异常类型或错误码），前者给默认值，后者记日志/提示；统一空值语义。

#### [中等] 数值配置几乎全无范围钳制，非法值一路传到渲染/计算层，且下拉索引会越界
- 位置：`Variable.cpp:48-63`（`Model`/`DisplayTime`/`FontSize`/`ReplaceLanguage` 等）、`ImGui/Interface.cpp:991-993`（`InputInt`/`InputFloat` 直接绑全局量，无 min/max）、`ImGui/Interface.cpp:300`、`ImGui/Interface.cpp:1477`、`ImGui/Interface.cpp:1048-1050`
- 现象：全文件唯一做范围检查的是 `VulkanDeviceMode`（`Variable.cpp:67-70`）。其余数值项完全信任配置：`FontSize = 0`（或负数）会先让 `ImGui/Interface.cpp:300` 的 `kuangshu / int(Variable::FontSize)` 整数除零（`WrapSize` 是 `unsigned int`，负数还会变成巨值，供 `AngelScript/FunctionalFunctions.cpp:372` 的 `Cut >= Variable::WrapSize` 使用），再让 `ImGui/Interface.cpp:1477` 的 `Variable::HitokotoFontSize / Variable::FontSize` 得到 inf；`DisplayTime = -1` 与 `HitokotoTimeInterval` 的计时比较会出现「永不消失/立即消失」的怪现象。下拉框更直接：`ImGui/Interface.cpp:1048` 用 `Variable::BaiduitemsName[Variable::ReplaceLanguage]` 索引，`ReplaceLanguage` 直接来自 `Data.ini`（`Variable.cpp:52`），写 99 就是越界读；同一处的循环 `for (int n = 0; n < Variable::BaiduitemsName.size()-1; n++)`（`Interface.cpp:1050`）在向量为空时（用户把 `Baidu_itemsName =` 清空，`GetVector` 对空值返回空 vector 而不报错）会因 `size()-1` 下溢而越界遍历。
- 证据（关键代码原文）：
  ```cpp
  // Variable.cpp:51-52（无钳制）
  FontSize = iniData->Get<float>("Set", "FontSize");
  ReplaceLanguage = iniData->Get<int>("Set", "ReplaceLanguage");
  // ImGui/Interface.cpp:300
  Variable::WrapSize = kuangshu / int(Variable::FontSize);
  // ImGui/Interface.cpp:1048-1050
  if (ImGui::BeginCombo(Language::ReplaceLanguage.c_str(), Variable::BaiduitemsName[Variable::ReplaceLanguage].c_str(), flags))
      for (int n = 0; n < Variable::BaiduitemsName.size()-1; n++)
  ```
- 建议修法：读入后集中做一次 `Clamp`（`FontSize ∈ [8, 72]`、`DisplayTime ∈ [100, 60000]`、`ReplaceLanguage ∈ [0, itemsName.size()-1]`、`HitokotoTimeInterval > 0`），把 `ImGui::InputInt/InputFloat` 换成带 `min/max` 的重载或 `ImGui::DragFloat(..., min, max)`，并在界面里用 `ImGui::BeginCombo` 前先判断索引是否在范围内。

#### [中等] `windows_Width` / `windows_Heigth` 是未初始化的全局量
- 位置：`Variable.cpp:169-170`（`int windows_Width; int windows_Heigth;` 无初始化）、`Variable.h:36-37`
- 现象：这两个全局量没有任何初始化器，也不是从 `Data.ini` 读取的（键一致性核对表里没有对应键）。若任何时候它们参与了配置写回或界面计算，读取的就是不确定值。全文件里**所有非类类型全局量都没有初始化器**（`Variable.cpp:169-226`），只有 Vulkan 段（`:229-234`）有默认值——这种"有的有、有的没有"的默认值现状本身就是风险。
- 证据（关键代码原文）：
  ```cpp
  int windows_Width;//屏幕宽度
  int windows_Heigth;//屏幕高度
  ```
- 建议修法：给每个全局量补上初始化器，或把 `windows_*` 这类运行时状态从"配置文件变量"里搬出去（它们不属于配置）。

#### [中等] 线程可见性与"先读后写被覆盖"的顺序问题
- 位置：`Variable.cpp:7`（`IniPath = FilePath;` 只存指针不拷内容）、`Variable.cpp:167` / `Variable.h:31`（`extern char* IniPath;`）、`Variable.cpp:78`（`SaveFile` 重新从磁盘解析）
- 现象：`IniPath` 是 `char*` 裸指针，`ReadFile(iniData)` 存的是字符串字面量 `"Data.ini"` 的地址（`FilePath.h:3` 的宏），当前恰好安全；但接口签名对调用方的要求（"必须传生命周期长于进程的字符串"）完全没有约束，任何 `std::string tmp; ReadFile(tmp.data())` 都会变成悬垂指针。`SaveFile` 不是基于内存态修改，而是**重新解析磁盘上当前的文件**再写回——如果用户在程序运行期间手工编辑过 `Data.ini`，或者在两个实例之间来回切换（单实例检查 `main.cpp:17-21` 只挡了同名窗口，`FindWindow` 对已改名的窗口无效），后写的实例会覆盖先写的实例的全部设置。整个 `Variable`/`Language` 命名空间没有任何同步机制，`Vulkan` 侧对 `Variable::VulkanDetectedDevices`/`CpuSoftwareRenderReason` 的写入（`Vulkan/instance.cpp:258-287`、`:381-383`）与 ImGui 线程的读取之间也没有可见性保证（**待确认**：需确认 ImGui 渲染线程与 Vulkan 设备枚举是否真在不同线程，若是同线程则此项降为轻微）。
- 证据（关键代码原文）：
  ```cpp
  // Variable.cpp:6-8
  extern void ReadFile(char* FilePath) {
      IniPath = FilePath;
      inih::INIReader* iniData = new inih::INIReader(IniPath);
  // Variable.cpp:78
      inih::INIReader* iniData = new inih::INIReader(IniPath);  // 重新读盘，不用内存态
  ```
- 建议修法：`IniPath` 换成 `std::filesystem::path` 或 `std::string`（彻底消灭裸指针）；`SaveFile` 基于内存态生成完整配置而不是重新读盘；若确有跨线程访问，用 `std::atomic` 或统一到主线程。

#### [轻微] `ini.h` 的 `Keys()` 存在两次整段拷贝，配置一大就是纯浪费
- 位置：`ini.h:331-338`、`ini.h:340-345`（按值返回整段 map 的 `Get(std::string)`）
- 现象：`Keys(section)` 先调用 `const std::unordered_map<...> Get(std::string section) const`——**按值返回整段的 map 拷贝**，参数也是按值传递——再遍历它构造 `std::set`，即两次拷贝。而 `Get<T>(section, name)`（`ini.h:355-377`）每次取值也要先走一遍这个整段拷贝：`auto const _section = Get(section);`。当前配置只有几十个键，性能上无所谓，但接口设计是一个容易被后续代码放大的坏模式（`write_Gai` 里就是「每个键一次整段拷贝」）。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:340-345
  inline const std::unordered_map<std::string, std::string> INIReader::Get(
      std::string section) const {
      auto const _section = _values.find(section);
      if (_section == _values.end()) {
          throw std::runtime_error("section '" + section + "' not found.");
      }
  // ini.h:331-338
  inline const std::set<std::string> INIReader::Keys(std::string section) const {
      auto const _section = Get(section);
      std::set<std::string> retval;
      for (auto const& element : _section) {
          retval.insert(element.first);
      }
      return retval;
  }
  ```
- 建议修法：`Get(section)` 返回 `const&`、参数改 `const std::string&`、`Keys()` 内部直接遍历 `_values.at(section)` 一次成 set；`Get<T>` 内部改用 `_values.find(section)` 避免整段拷贝。

#### [轻微] `Converter<T>` 接受"部分解析成功"，类型错误不被发现
- 位置：`ini.h:520-532`（`Converter<T>`）、`ini.h:534-545`（`BoolConverter`）
- 现象：`Converter` 的实现是 `T v{}; std::istringstream _{s}; _.exceptions(std::ios::failbit); _ >> v;`。空串会触发 `failbit` 抛异常（`"cannot parse value '...' to type<T>."`），但 `Get<int>("Key","MakeUp")` 面对 `MakeUp = 18abc` 会**成功返回 18**，后面的垃圾被静默忽略；`18.5` 会截断成 `18`。`BoolConverter` 更严格一些，只认 8 个字面量（`1/true/yes/on/0/false/no/off`），其余一律抛异常——也就是说 `Data.ini` 里把 `Startup` 写成 `True` 或 `2` 都会抛。另外 `std::transform(..., ::tolower)`（`ini.h:535`）对 `char` 为负的字节是 UB（ASCII 场景下实践安全，**待确认**）。
- 证据（关键代码原文）：
  ```cpp
  // ini.h:520-532
  template <typename T>
  inline T INIReader::Converter(const std::string& s) const {
      try {
          T v{};
          std::istringstream _{ s };
          _.exceptions(std::ios::failbit);
          _ >> v;
          return v;
      }
      catch (std::exception& e) {
          throw std::runtime_error("cannot parse value '" + s + "' to type<T>.");
      };
  }
  ```
- 建议修法：解析后检查流是否已到 `eof()`（或改用 `std::from_chars`），非完全消费即报"值格式不合法"；`BoolConverter` 的非法值建议降级为"返回默认值 + 记警告"而不是抛异常。

#### [轻微] `VectorToString` 在 `size == 0` 时下溢成死循环
- 位置：`Variable.h:14-23`
- 现象：循环条件是 `for (size_t i = 0; i < size-1; i++)`，`size` 为 0 时 `size-1` 变成 `SIZE_MAX`，配合 `v[size-1]` 会先疯狂越界读。当前调用点（`Variable.cpp:130`）固定传 4，实际安全，但这是一个没人拦得住的模板陷阱。
- 证据（关键代码原文）：
  ```cpp
  for (size_t i = 0; i < size-1; i++)
  {
      str = str + toString(int(v[i])) + " ";
  }
  ```
- 建议修法：开头加 `if (!v || size == 0) return "";`，并把循环条件改成 `i + 1 < size`。

#### [轻微] `Data.ini` 出厂就带空值键，与 `UpdateEntry` 的判据正面冲突
- 位置：`Data.ini`（`VulkanDeviceName =` 为空）、`Variable.cpp:151-159`
- 现象：作者已经知道这个坑并为 `VulkanDeviceName` 写了特判，但 `FontFilePath = 0`、`HitokotoFont = 0` 这类"用 0 表示未设置"的写法，一旦某天被清理成真正的空值，就会掉进上面那个保存即崩的坑。空值语义在项目里有三种表达（空串 / `0` / 键不存在），却没有统一定义。
- 证据（关键代码原文）：
  ```ini
  VulkanDeviceName =
  FontFilePath = 0
  HitokotoFont = 0
  ```
- 建议修法：统一"未设置"的表达（建议空串），并在 ini.h 层面让空值可写；`FontFilePath` 这种路径型字段不要用 `0` 当哨兵值。

#### [轻微] `Data.ini` 里提交了真实可用的账号与密钥
- 位置：`Data.ini`（WebDav 段 `username` / `password`，`YoudaoAPI` 段 `Youdao_ID` / `Youdao_Key`，`BaiduAPI` 段 `Baidu_ID` / `Baidu_Key`）
- 现象：坚果云 WebDav 账号密码、百度/有道翻译的 AppID 与密钥以明文形式直接进了仓库。任何拿到这个仓库的人都可以直接消耗这些配额（甚至接手该坚果云账号的读写权限）。同时 `username` 是 QQ 邮箱，属个人信息。
- 证据（关键代码原文）：
  ```ini
  [WebDav]
  username = 1779****@qq.com
  password = ********
  [YoudaoAPI]
  Youdao_ID = ********
  Youdao_Key = ********************************
  ```
- 建议修法：把 `Data.ini` 从版本控制里移除（改提交 `Data.ini.example` 只留键和注释），在 `.gitignore` 补规则，并**立即轮换**已泄露的这几个密钥与密码（历史提交里已存在，删文件不够）。

### 二、健壮性与异常安全

- **整体结论：配置层是"全有或全无"的设计——任何一个键出问题，最坏的结果是启动即崩、最好也是静默走默认值，中间没有可用的降级带。** 具体链路：
  1. 文件缺失/不可读 → `INIReader` 构造抛（`ini.h:302`）→ `main.cpp:23-24` 在 try 之外 → `std::terminate`。
  2. 键缺失/值为空但被要求解析 → 单参 `Get` 抛（`ini.h:362-363`）或 `Converter` 抛（`ini.h:520-532`）→ 同上。
  3. 重复键 → 解析回调抛（`ini.h:547-556`）→ 同上，且顺带泄漏 C 缓冲区与未关闭的文件句柄。
  4. 读取成功但值非法 → 只有 `VulkanDeviceMode` 被钳制，其余原样进运行时（除零、巨值、越界索引）。
  5. 保存时值为空 → `UpdateEntry` 抛（`ini.h:483-486`）→ `ImGui/Interface.cpp:1371` 无 try → 冒到 `main.cpp:34-41` → 弹窗后退出。
- **异常安全等级**：`ReadFile`/`SaveFile` 都是"无保证"级别（裸 `new` + 尾部 `delete`，中途抛异常即泄漏）。唯一的正面结论是：`UpdateEntry` 的异常发生在 `write_Gai`（`Variable.cpp:162`）之前，所以**不会写出半截文件**；但异常抛出前 `_values[section][name]` 已经被写入空串（`operator[]` 先于长度判断执行），内存中的对象状态已被污染，任何"catch 后接着用它继续保存"的补救写法都会踩坑。
- **写文件的原子性**：`write_Gai` 直接 `ofstream` 截断覆盖（`ini.h:585-599`），`out.close()` 后不检查 `failbit`/`badbit`。磁盘满或被杀进程时会留下损坏的 `Data.ini`，而下次启动正好会因为缺键崩溃——这两个缺陷会互相放大。
- **建议的加固顺序**：① `ImGui/Interface.cpp:1371` 包 try/catch + 错误弹窗（成本最低、收益最大）；② `main.cpp:23-24` 包 try/catch；③ `ini.h` 增加永不抛的 `SetEntry`；④ 改成栈对象/智能指针；⑤ 写临时文件 + rename。

### 三、编码与本地化

- **字符集现状（已逐字节核对）**：全部 67 个含中文的源文件与 ini 均为 **UTF-8 无 BOM**，无一例外（唯一带 BOM 的是 `CMakeSettings.json`）；行尾统一 CRLF；`Language/zh.ini` 有 1416 个非 ASCII 字节、`eng.ini` 只有 6 个（两处全角括号）；`Variable.cpp` 2370、`Variable.h` 2436、`ImGui/Interface.cpp` 3348、`Vulkan/instance.cpp` 9285。**结论：文件之间字符集是自洽的，"GBK 源码被当 UTF-8 解析"这个方向不成立；真正的缺口是"没有 BOM + 没有 `/utf-8`"这一组合，让编译器只能靠系统区域设置去猜。**
- **`FilePath.h` 的字节已被逐一验证**：`2F 2F E6 96 87 E4 BB B6 E8 B7 AF E5 BE 84` 即 UTF-8 的 `//文件路径`（用 GBK 解码会显示成 `鏂囦欢璺緞`）。这里没有 GBK 文件，但这条 `E6 96 87` 恰好演示了风险机制：GBK 把它当成一个汉字并吃掉下一个字节，若注释末尾紧跟引号/括号，就会"吃掉"语法元素——这正是 `main.cpp:2-6` 记录的症状。
- **字符集缺失的直接影响**：中文注释被按 GBK 解码时可能错位吞掉下一行代码（编译期语法错误，且换一台区域设置不同的机器表现不同）；而**字符串字面量**里的中文会被编成 GBK 字节，与程序里 `u8"GitHub"`（`ImGui/Interface.cpp:1392`）的 UTF-8 字节在同一个 ImGui 界面里混排，出现方块/乱码。作者当年用「在中文结尾补一个空格或字母」来规避（`main.cpp:2-6`），这只能治注释，治不了字符串。
- **多语言键一致性：通过。** `Language/zh.ini` 与 `Language/eng.ini` 各 82 个键，**键集合完全相同**（only-in-zh 为空、only-in-eng 为空、无重复键），段的划分一致（`[Translate]` 3 个、`[Set]` 79 个、`[tray]` 5 个），键的出现顺序也逐行一致。这一项是本层最健康的部分。
- **语言文件的格式瑕疵**：`eng.ini:30` 的 `Confirm_= Confirm` 在 `=` 前没有空格，与其余 81 行的 `Key = Value` 写法不一致（解析无影响，纯一致性问题）；zh 用 `※` 而 eng 用 `*` 做提示符号（`RenderDeviceRestart_`）；`RenderDeviceItem_` 在 zh 里是 `%s（%s）`、eng 里是 `%s (%s)`（全角/半角括号不一致）；`eng.ini` 的 `RenderDeviceMissing_` 值里含双引号，解析器能接受，但下次保存/改写时容易被破坏（**待确认**：`write_Gai` 不转义，若哪天把语言文件也纳入写回就会出问题）。
- **硬编码中文（绕过 Language::）实查 126 处**，真正影响界面与多语言的只有三处，其余是调试输出：
  - `Function/Translate.h:40` `const char* TranslateName[3] = {"百度","爬虫","有道" };` —— 被 `ImGui/Interface.cpp:334` 直接用作按钮文案 `ImGui::Button(mTranslate->TranslateName[mTranslate->mTranslate])`，**切到英文界面后这个按钮仍然是中文**，而且在当前编译设置下还可能显示为乱码。这是最该修的一处。
  - `Function/Translate.cpp:161,165,171,176,239,243,249,254,298,304` 共 10 处 `return "错误";` —— 翻译失败的返回值是裸中文，会直接显示在翻译结果框里，英文界面下同样突兀。
  - `Vulkan/instance.cpp:381,383` 的 `Variable::CpuSoftwareRenderReason = "设置里选择了「CPU 软件渲染」";` 以及 `Vulkan/device.cpp:218,234` 的 `reasons.push_back("不支持各向异性采样(samplerAnisotropy)")` 等 —— 这些字符串经 `ImGui/Interface.cpp:1174-1176` 的 `FillDeviceText(Language::RenderDeviceDegrade, { Variable::CpuSoftwareRenderReason })` **渲染到设置界面**，只有模板走了多语言，原因正文永远是中文。
  - 反例（说明大部分地方做得对）：`ImGui::Text/Button/Checkbox/Selectable` 等 UI 调用里**没有扫到任何裸中文字面量**，界面文案基本都走 `Language::`；`Tool/Tool.cpp:258-318` 的 7 处、`Vulkan/device.cpp` 与 `Vulkan/instance.cpp` 约 30 处 `VulkanDiag` 属于诊断日志，可以不管。
- **`Language::ReadFile` 缺文件时的行为（针对"界面会不会空白"）**：不会留空——`ini.h:302` 在构造函数里就抛 `"ini file not found."`，程序根本走不到渲染环节。也就是说**不可能出现"界面全空白"这种温和失败，只会直接崩溃**，而崩溃点在 `main.cpp:23-24` 的 try 之外。

### 四、配置项一致性核对表（Data.ini vs Variable.cpp 读取）

- **总体结论：键集合完全自洽。** `Variable.cpp:10-72` 共 62 处读取，逐个比对后**每一处读取的键都存在于 `Data.ini`**（含 `VulkanDeviceMode = 1`、`VulkanDeviceName =`（空值））；`Data.ini` 里也**没有"有键但代码不读"的冗余项**。不存在"代码读但文件没有"的键，所以当前这份出厂配置能正常启动。风险全部在**值**上。
- 逐段核对：

| 段 | Data.ini 键 | 代码读取位置 | 状态 |
|---|---|---|---|
| `[Hitokoto]` | PopUpNotificationBool / HitokotoTimeInterval / HitokotoDisplayDuration / HitokotoPosX / HitokotoPosY / HitokotoFontSize / HitokotoFontBool / HitokotoFont / HitokotoTTFBool（9 项） | `Variable.cpp:10-18` | 一致。注意 `HitokotoFont = 0` 是把"未设置"写成 `0`，与 `FontFilePath` 同病 |
| `[WebDav]` | url / username / password / WebFile / OpcodeBool / LanguageBool / TessDataBool / TTFBool（8 项） | `Variable.cpp:20-27` | 一致。前 4 项是明文账号密码，见风险条 |
| `[YoudaoAPI]` | Youdao_ID / Youdao_Key / Youdao_items（11 项）/ Youdao_itemsName（11 项） | `Variable.cpp:34-37` | 键一致，但**写回时被百度值覆盖**（`Variable.cpp:102-103`） |
| `[BaiduAPI]` | Baidu_ID / Baidu_Key / Baidu_items（11 项）/ Baidu_itemsName（11 项） | `Variable.cpp:29-32` | 一致。两个 `itemsName` 都是 11 项，界面 `Variable::BaiduitemsName[Variable::ReplaceLanguage]`（`ImGui/Interface.cpp:1048`）在 `ReplaceLanguage = 3` 时索引安全 |
| `[FT]` | Translate / From / To（3 项） | `Variable.cpp:39-41` | 一致 |
| `[Key]` | MakeUp / Screenshotkey / Choicekey / Replacekey（4 项） | `Variable.cpp:43-46` | 一致。`Screenshotkey = D`、`Choicekey = Q`、`Replacekey = R` 都是单字符，与 `ImGui/Interface.cpp:1311-1313` 的 `toupper(SetScreenshotkey[0])` 用法吻合 |
| `[Set]` | TesseractModel / DisplayTime / FontSize / ReplaceLanguage / FontBool / FontFilePath / Startup / Language / ScreenshotColor / Script / ScriptBool / VulkanDeviceMode / VulkanDeviceName（13 项） | `Variable.cpp:48-72` | 一致。`ScreenshotColor = 0 0 0 100` 正好 4 项，写回数组安全；`VulkanDeviceName` 为空值 |

- **`Data.ini` 中"没有对应代码读取"的键：0 个。**
- **代码读取但 `Data.ini` 中缺失的键：0 个**（`[tray]` 段的 5 个键属于 `Language/*.ini`，不在 `Data.ini` 职责内）。
- **不在 `Data.ini` 里的全局量**：`Variable::windows_Width` / `windows_Heigth`（`Variable.cpp:169-170`）、`Variable::WrapSize`（`Variable.cpp:4`，由 `ImGui/Interface.cpp:300` 算出来）属于运行时状态而非配置，但它们和配置变量混在同一个命名空间里，容易被误当成可配置项。
- **构建期一致性**：`CMakeLists.txt:53` 的 `execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Data.ini ${CMAKE_CURRENT_BINARY_DIR})` 与 `CMakeLists.txt:58` 的 `copy_directory .../Language .../Language` 都会把配置正确拷到输出目录，**这一项通过**。但两点要注意：① `execute_process` 只在 **configure 阶段**执行，改了 `Data.ini` 不重新 configure，输出目录里还是旧文件；② Debug 与 Release 有各自的 `buildRoot`（`CMakeSettings.json` 的 `x64-Debug (默认值)` 与 `x64-Release`，均为 Ninja），两个目录都要各跑一次 configure。**待确认**：`cmake -E copy` 在目标已存在且内容不同时的覆盖时机（若用户在 out 目录里改过 `Data.ini`，重新 configure 可能会用源码目录的版本盖掉用户设置）。

### 五、可添加的功能建议（务实可落地）

1. **配置 schema 集中化 + 单一默认值表（优先级最高）。** 现在"键名、默认值、范围、段名"四类信息散在 `Variable.cpp`、`Data.ini`、`ImGui/Interface.cpp` 三处，`Variable.cpp:102-103` 那个 bug 就是这种散落直接结出的果。建议在 `Variable.cpp` 里建一张 `struct ConfigItem { const char* section; const char* key; ... }` 表，`ReadFile` 与 `SaveFile` 都遍历这张表读写，并附上 `min`/`max`。这样：读写不会错位、钳制自动生效、新增配置项只改一处、`Data.ini` 缺失的键能自动补上而不是崩溃。
2. **配置读取降级 + 明确的错误上报。** 让 `ReadFile` 变成"尽力读取"：每个键用三参默认值版本，读失败时累计一份 `std::vector<std::string> warnings`，读完后统一写日志、并在设置界面顶部显示一条「有 N 项配置使用了默认值（原因：xxx）」的提示条。文件完全缺失时，用内置默认值在内存里跑起来，用户一进设置界面点保存就把一份完整正确的 `Data.ini` 写出来——现在的行为是"缺文件就崩"，对一个要装到别人电脑上用的工具来说太重了。
3. **「导出/导入配置」+ 备份与版本号。** 你已经有一套基于 WebDav 的备份恢复（`ImGui/Interface.cpp:941` 恢复后调 `Variable::ReadFile`），只差把同一个机制用在本地：一键导出 `Data.ini`（顺便把 API 密钥打码后导出，便于贴到 issue 里求助）、导入时先校验再原子替换。配合在 `Data.ini` 头部写一个 `ConfigVersion = 1`，未来改键名时就能做迁移而不是靠"缺键即崩"。

**另外三项低成本改进**：① `CMakeLists.txt` 加 `/utf-8` 并把源文件转成 UTF-8 with BOM（一次性消除"换电脑编译报语法错误"这类幽灵问题）；② `Language::ReadFile` 用 exe 目录拼绝对路径，并在语言文件缺失时回退英文默认文案；③ 把 `TranslateName`、`"错误"`、`CpuSoftwareRenderReason` 这三处中文收进 `Language::`，否则英文界面永远有中文残留。

---

### 附：本报告已核对的关键行号索引

| 结论 | 位置 |
|---|---|
| 百度密钥写进有道键（严重 bug） | `Variable.cpp:102-103` |
| 空值导致 UpdateEntry 抛异常 | `ini.h:481-488` |
| 保存入口无 try/catch | `ImGui/Interface.cpp:1371` |
| catch 覆盖不到配置读取 | `main.cpp:34-41` / `main.cpp:23-24` |
| 颜色数组无长度校验 | `Variable.cpp:57-61` / `Variable.h:98` |
| 唯一被钳制的配置项 | `Variable.cpp:66-72` |
| 裸 new + 尾部 delete | `Variable.cpp:8,74,78,164` |
| 写回抛弃注释与顺序 | `ini.h:585-599` / `ini.h:318-338` |
| 解析回调抛异常泄漏 C 缓冲 | `ini.h:547-556` / `ini.h:79-180` / `ini.h:186-194` |
| 语言文件构造即抛 | `Variable.cpp:240-241` / `ini.h:287-290` / `ini.h:302` |
| 硬编码中文按钮 | `Function/Translate.h:40` / `ImGui/Interface.cpp:334` |
| 下拉索引越界 | `ImGui/Interface.cpp:1048-1050` / `Variable.cpp:52` |
| UpdateEntry/InsertEntry 的判据 | `ini.h:459-478` / `ini.h:480-499` |
| 三参 Get 吞掉格式错误 | `ini.h:388-397` |
| 硬编码降级原因 | `Vulkan/instance.cpp:381,383` / `ImGui/Interface.cpp:1174-1176` |
| 构建期拷贝配置 | `CMakeLists.txt:53,58` |



---

## Vulkan 渲染层 · 体检报告

审查对象：`Vulkan/` 全部 38 个文件（约 3600 行）+ `vk_mem_alloc.h` 使用方式 + 调用方契约（`application.cpp`、`ImGui/Interface.cpp`、`imgui_impl_vulkan.cpp`）。
方法：逐行精读，无编译、无 validation layer 运行（本机无 Environment/ 与 Vulkan SDK，结论均为文本证据 + Vulkan 规范推导）。
严重度：`[严重]` 会稳定崩溃/卡死/触发校验错误；`[中等]` 特定条件或性能/健壮性；`[轻微]` 可维护性。
标注「待确认」的条目需要跑一次启用校验层的 Debug 构建才能最终定论。

---

### 一、移植残渣与语义错误（PixelClean 遗留）

代码级残留（`Global::`、`PixelClean`、`GameMods`、`GameKeyEnum`、`PixelTexture`、`shaderc`、`<vma/vk_mem_alloc.h>`、`TOOL::VulKanError`、`TOOL_SpdLog`）**已清理干净**——全仓库 grep 仅 10 处命中，全部是 Android 分支或注释：

1. **`Vulkan/windowSurface.cpp:4-6, 24-31`** — 真实 Android 分支：`#include <vulkan/vulkan_android.h>` + `VkAndroidSurfaceCreateInfoKHR` + `vkCreateAndroidSurfaceKHR`。Windows 构建下是死代码（不参与编译）。同时 `:10-16` 的构造函数形参用 `#if defined(_WIN32) / #elif defined(__ANDROID__)` 包了**同一个** `Window* window` 声明，纯冗余。
2. **`Vulkan/instance.cpp:460`** — `return true; //Android 的 ICD 是系统自带的`（非 Windows 兜底）。
3. **`Vulkan/instance.cpp:106 / 114 / 128`**、`application.h:76`、`application.cpp:48`、`ImGui/Interface.cpp:691` — 注释里提到 Android / 「和 PixelClean 一致」。

**真正「换了名字没换语义」的地方（本项目无对应功能却仍保留的引擎逻辑）**：

| 位置 | 残留内容 | 为什么是残留 |
|---|---|---|
| `Vulkan/instance.cpp:608` | `appInfo.pApplicationName = "vulkanLession"` | 教程/PixelClean 名字；同文件 `:236` 探测实例写的是 `"TranslatorKyi ICD probe"`，两处不一致 |
| `Vulkan/device.cpp:166-173, 281-285, 293-294` | 注释「UVDynamicDiagram 管线将改用顶点着色器实例化展块」「几何着色器已改为可选」 | TranslatorKyi 只有 ImGui 一个管线，没有 UVDynamicDiagram；`-100000` 评分惩罚是为该引擎服务的 |
| `Vulkan/device.cpp:457-514` | `getComputeCapabilities()`（NV SM builtins / AMD shader core / subgroupSize） | 计算着色器能力查询，本项目**从未调用**（全仓库无调用点） |
| `Vulkan/buffer.cpp:50-67` | `createStorageBuffer`（注释「粒子系统 SSBO」） | 本项目无粒子系统、无 SSBO 使用点 |
| `Vulkan/Window.cpp:129-162` | `processEvent()` 里 W/S/A/D 四个空分支、P 键切鼠标、ESC→`exit(0)` | 游戏输入处理，**无人调用**（`mainLoop` 只调 `pollEvents()`） |
| `Vulkan/Window.cpp:74-105` | `SystemTray()` 用 `"MyWindowClass"` / `"My Window"` 类名与窗口名 | 模板残留；且 `nidApp.uFlags` 含 `NIF_GUID` 却**未设置 `guidItem`**，托盘图标可能注册失败 |
| `Vulkan/buffer.h/.cpp`、`image.h/.cpp`、`shader.*`、`pipeline.*`、`sampler.*`、`description.h`、`descriptorSet*`、`descriptorPool.*`、`descriptorSetLayout.*` | 整套「顶点/索引/UBO/SSBO + DescriptorSet + Pipeline + Shader」基础设施 | **本项目完全未使用**：`UniformParameter`/`DescriptorSet`/`Pipeline`/`Shader`/`GAME::Texture` 在全仓库除自身定义外零引用；ImGui 用自己的描述符池与管线（`Interface.cpp:72-97`、`imgui_impl_vulkan.cpp:935`）。约 12 个文件、~800 行为纯负担，且是本次审查中绝大多数缺陷的来源 |

> 说明：`Vulkan/commandBuffer.*`（主/次指令缓冲）、`Image`（深度/MSAA 图）、`Buffer`（`Image` 上传用）仍被 swapChain/image 间接使用，**不能整体删除**。

---

### 二、Vulkan 正确性缺陷

#### [严重] 第一帧永久卡死：fence 创建时未置 signal，却被无限等待
- 位置：`application.cpp:187`、`application.cpp:317`、`Vulkan/fence.cpp:12, 30-32`
- 现象：首次进入 `render()` 时 `mFences[0]->block()` 等待一个从未被 signal 过的 fence，`timeout = UINT64_MAX` → 进程永久挂起（表现为「按翻译热键后整个程序无响应」）。
- 证据：
  - `application.cpp:187` `VulKan::Fence* fence = new VulKan::Fence(mDevice);`（第二参数默认 `false`）
  - `Vulkan/fence.cpp:12` `createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;`
  - `application.cpp:317` `mFences[mCurrentFrame]->block();`
  - `Vulkan/fence.cpp:31` `vkWaitForFences(mDevice->getDevice(), 1, &mFence, VK_TRUE, timeout);`，`fence.h` 默认 `timeout = UINT64_MAX`
  - 启动路径可达：`application.cpp:217-226`（`PopUpNotificationBool` 开的「一言」弹窗）与 `:212` 翻译界面都会在第一帧就调用 `render()`
- 建议修法：`new VulKan::Fence(mDevice, true)`；并把 `block()` 的返回值纳入判断（超时/`VK_ERROR_DEVICE_LOST` 时抛异常或重建设备），不要静默忽略。

#### [严重] ImGui 管线采样数与 RenderPass 采样数不一致
- 位置：`application.cpp:82` vs `application.cpp:99`
- 现象：`init_info.MSAASamples` 取的是设备最大可用采样数（通常 4/8），但 RenderPass 的颜色附件写死 `VK_SAMPLE_COUNT_1_BIT`。`ImGui_ImplVulkan_CreatePipeline` 会把 `MSAASamples` 直接写进 `VkPipelineMultisampleStateCreateInfo::rasterizationSamples`，与本 pipeline 所依附的 renderPass 附件采样数不符 → `vkCreateGraphicsPipelines` 返回 `VK_ERROR_*`，ImGui 初始化阶段即失败（在启用校验层时是明确的校验错误）。
- 证据：
  - `application.cpp:82` `init_info.MSAASamples = mDevice->getMaxUsableSampleCount();`
  - `application.cpp:99` `finalAttachmentDes.samples = VK_SAMPLE_COUNT_1_BIT;//采样`
  - `ImGui/imgui_impl_vulkan.cpp:833` `ms_info.rasterizationSamples = (MSAASamples != 0) ? MSAASamples : VK_SAMPLE_COUNT_1_BIT;`
- 建议修法：二选一——把 `MSAASamples` 改成 `VK_SAMPLE_COUNT_1_BIT`（最小改动），或真正把 MSAA 接上（见下一条）。

#### [严重] FrameBuffer 附件数与 RenderPass 声明不匹配，且 MSAA/深度附件完全未接入 RenderPass
- 位置：`Vulkan/swapChain.cpp:181-191` vs `application.cpp:93-135`
- 现象：FrameBuffer 无条件绑定 **3** 个附件（swapchain 图 + MSAA 图 + 深度图），而 RenderPass 只注册了 **1** 个 `VkAttachmentDescription`、subpass 也只 `addColorAttachmentReference`。附件数量不一致是硬性校验错误；即使驱动容忍，MSAA 与深度也从未被采样（RenderPass 里根本没有它们）。
- 证据：
  - `Vulkan/swapChain.cpp:181-185` `std::array<VkImageView, 3> attachments = { mSwapChainImageViews[i], mMutiSampleImages[i]->getImageView(), mDepthImages[i]->getImageView() };`
  - `Vulkan/swapChain.cpp:190` `frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());`
  - `application.cpp:107` `mRenderPass->addAttachment(finalAttachmentDes);`（全函数只调用一次）
  - `application.cpp:118` `subPass.addColorAttachmentReference(finalAttachmentRef);`（未调用 `setResolveAttachmentReference`）
- 代价：每张 swapchain 图都白白分配了一张 MSAA 图 + 一张深度图（`swapChain.cpp:124-173`），1080p 下每图约 8~40 MB，随 `mImageCount` 翻倍，纯浪费。
- 建议修法：短期内把 FrameBuffer 只绑 1 个附件、删掉 MSAA/深度图的创建；要恢复 MSAA 就补齐 RenderPass 的 resolve 附件与 subpass 的 `setResolveAttachmentReference`，并把 `MSAASamples` 与附件采样数对齐。

#### [严重] 当设备只支持 1× 采样时空指针解引用
- 位置：`Vulkan/swapChain.cpp:171` 与 `Vulkan/swapChain.cpp:183`
- 现象：`sampleCount == 1` 分支把 `mMutiSampleImages[i]` 置 `nullptr`，但 `createFrameBuffers()` 无条件解引用它 → 崩溃。这条路径尤其要紧，因为本项目新加的 CPU 降级会主动把用户送到 SwiftShader 上。
- 证据：
  - `Vulkan/swapChain.cpp:170-172` `else { mMutiSampleImages[i] = nullptr; }`
  - `Vulkan/swapChain.cpp:183` `mMutiSampleImages[i]->getImageView(),`
  - 触发条件：`Vulkan/device.cpp:442-454` `getMaxUsableSampleCount()` 在 `framebufferColor/DepthSampleCounts` 只有 1× 时返回 `VK_SAMPLE_COUNT_1_BIT`
- 「待确认」：主流硬件都 ≥2×，所以实测未必命中；确认方法——把 `getMaxUsableSampleCount()` 临时改成 `return VK_SAMPLE_COUNT_1_BIT;` 跑一次，或启用校验层看是否在 `vkCreateFramebuffer` 前崩溃。

#### [严重] 顶层描述符池/指令池在 GPU 可能仍在使用时被销毁
- 位置：`ImGui/Interface.cpp:162-171`、`application.cpp:388-405`
- 现象：`mainLoop` 结束只做了一次 `vkDeviceWaitIdle`（`application.cpp:231`），但 `cleanUp()` 释放的对象里包含 `ImGui::RenderPlatformWindowsDefault()` 用过的 ImGui 描述符池、以及被 pending 提交引用的次指令缓冲；`LoadTextureFromFile` 每次都会 `vkDeviceWaitIdle` 但 `RemoveTexture` 立刻释放内存，中间没有任何同步保证。这是「退出/重开截图窗口时偶发崩」的典型来源。
- 证据：
  - `ImGui/Interface.cpp:170` `vkDestroyDescriptorPool(mDevice->getDevice(), g_DescriptorPool, nullptr);`（`ImGui_ImplVulkan_Shutdown()` 之前没有任何 `vkDeviceWaitIdle`）
  - `application.cpp:231` `vkDeviceWaitIdle(mDevice->getDevice());//等待命令执行完毕`
  - `ImGui/Interface.cpp:1710-1715` `vkFreeMemory(...)` 紧接 `vkDestroyBuffer(...)`（顺序反了，且无等待）
  - `Vulkan/image.cpp:228-306` / `Vulkan/buffer.cpp:311-329`：每次布局转换/上传都是 `new CommandBuffer` → `begin` → `submitSync` → `delete`，其中 `Vulkan/commandBuffer.cpp:114-123` 的 `submitSync` 直接 `vkQueueWaitIdle(queue)`
- 建议修法：`cleanUp()` 与 `ImGuiInterFace::~ImGuiInterFace()` 的第一行都补 `vkDeviceWaitIdle(mDevice->getDevice())`；`RemoveTexture` 顺序改为「先 destroy 子对象、最后 free 内存」。

#### [中等] `createCommandBuffers()` 每帧复用同一批主指令缓冲，却不检查其在途状态
- 位置：`application.cpp:138-177, 333`
- 现象：`mCommandBuffers[imageIndex]->begin()` 默认参数是 `flag = 0`（既没有 `ONE_TIME_SUBMIT` 也没有 `SIMULTANEOUS_USE`），且这一批缓冲按 image 索引复用。因为 `mFences[mCurrentFrame]` 的粒度是「帧」而不是「image」，理论上存在重录一张仍被 GPU read 的指令缓冲、以及同一被 acquire 两次的 image 被重复录音的风险 → 校验错误 + 画面撕裂/闪回。
- 证据：
  - `application.cpp:333` `createCommandBuffers(imageIndex);`
  - `application.cpp:164` `mCommandBuffers[i]->begin();//开始录制主指令`
  - `Vulkan/commandBuffer.h:38` `void begin(VkCommandBufferUsageFlags flag = 0, ...)`
  - `application.cpp:317` 只等 `mFences[mCurrentFrame]`，`mCurrentFrame` 与 `imageIndex` 是两套索引
- 「待确认」：需要校验层实测（`VUID-vkBeginCommandBuffer-commandBuffer-00049`）。确认方法：Debug + 校验层下连续触发翻译窗口并拖动/截图。

#### [中等] 无 swapchain 重建：`OUT_OF_DATE` / `SUBOPTIMAL` 与窗口 resize 全部落空
- 位置：`application.cpp:322-329, 380-383`、`Vulkan/Window.cpp:29-32`
- 现象：`vkAcquireNextImageKHR` 与 `vkQueuePresentKHR` 的返回值都赋给了 `result` 却**从未被判断**，也没有任何 `recreateSwapChain()`。同时 `windowResized` 回调置位的 `mWindowResized` 无人读取，`getWidth/getHeight` 也没有重建调用者。窗口一旦被改变尺寸/被 DWM 重建表面，就是「卡住 + 黑屏」。
- 证据：
  - `application.cpp:323-329` `VkResult result = vkAcquireNextImageKHR(...);`，其后到 `:333` 之间无任何 `if (result == ...)`
  - `application.cpp:380` `result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);`，之后直接 `:383 mCurrentFrame = ...`
  - `Vulkan/Window.cpp:29-32` `mWindowResized = true;`（grep 全仓库无其他引用）
- 建议修法：把 `render()` 改成返回 `bool` 或在内部处理 `VK_ERROR_OUT_OF_DATE_KHR || VK_SUBOPTIMAL_KHR`：`vkDeviceWaitIdle` → 依序销毁 FrameBuffer/ImageView/深度图/MSAA 图/旧 Swapchain → 重建 → **同时重置 `mCurrentFrame`、重建 fence/semaphore**（当前 fence/semaphore 数量按 image 数创建，`Variable::windows_Width/Heigth` 也依赖它）。

#### [中等] `vkAcquireNextImageKHR` 之后立即使用 image 索引、fence 却按帧索引——失败路径无兜底
- 位置：`application.cpp:317-360`
- 现象：`mFences[mCurrentFrame]->resetFence()` 在 `:359` 于 `vkQueueSubmit` 之前无条件执行，只要 submit 抛异常/失败，这个 fence 就永远处于未 signal 状态，下一帧立刻死锁（与第一条叠加）。且 `mFences` / 两套 semaphore 都是按 `mSwapChain->getImageCount()` 创建的，若将来重建 swapchain 而 image 数变化，数组索引会越界。
- 证据：
  - `application.cpp:359-362` `mFences[mCurrentFrame]->resetFence(); if (vkQueueSubmit(...) != VK_SUCCESS) { throw ... }`
  - `application.cpp:180-189` 循环边界 `i < mSwapChain->getImageCount()`
  - `application.cpp:327` `mImageAvailableSemaphores[mCurrentFrame]->getSemaphore()` 与 `:351` `mCommandBuffers[imageIndex]` 混用两套索引

#### [中等] 主指令缓冲在 primary 用途下仍传入非空 `pInheritanceInfo`
- 位置：`Vulkan/commandBuffer.cpp`（`begin` 实现）、`Vulkan/commandBuffer.h:38`
- 现象：`begin()` 的默认形参是值语义的 `const VkCommandBufferInheritanceInfo& inheritance = {}`，函数内一律把它挂到 `pInheritanceInfo` 上；主指令缓冲要求该字段为 `NULL`（`VUID-VkCommandBufferBeginInfo-pInheritanceInfo-00000`）。次指令缓冲路径（`ImGui/Interface.cpp:227`）反而是正确用到了它。
- 证据：
  - `Vulkan/commandBuffer.h:38` `void begin(VkCommandBufferUsageFlags flag = 0, const VkCommandBufferInheritanceInfo& inheritance = {});`
  - `application.cpp:164` `mCommandBuffers[i]->begin();`（不传 flag / inheritance）
- 建议修法：在 `begin()` 内判断 `mAsSecondary`，只有次指令缓冲才赋 `pInheritanceInfo`。

#### [中等] 纹理上传路径的三个确定性缺陷（截图功能）
- 位置：`ImGui/Interface.cpp:1512-1717`
- 现象：
  1. 图像与视图格式不一致：`VkImage` 建为 `R8G8B8A8_UNORM`，`VkImageView` 却写 `B8G8R8A8_UNORM` → 校验错误；即便驱动放过，R/B 通道互换，截图偏蓝。
  2. 上传 target buffer 的 `image_size` 直接取 `Variable::windows_Width * Heigth * 4`，再 `memcpy(map, Texturedata, image_size)`，没有任何「实际截图尺寸」校验 → 尺寸不一致时越界读。
  3. `LoadTextureFromFile` 每次分配的命令缓冲（`:1634-1642`）从不 `vkFreeCommandBuffers`；`CheckVkResultFn` 传的是 `application.cpp:11-18` 的 `check_vk_result`，其中 `err < 0` 会直接 **`abort()`**。
- 证据：
  - `ImGui/Interface.cpp:1535` `info.format = VK_FORMAT_R8G8B8A8_UNORM;`
  - `ImGui/Interface.cpp:1566` `info.format = VK_FORMAT_B8G8R8A8_UNORM;`
  - `ImGui/Interface.cpp:1526` `size_t image_size = tex_data->Width * tex_data->Height * tex_data->Channels;` / `:1620` `memcpy(map, Texturedata, image_size);`
  - `application.cpp:16-17` `if (err < 0) abort();`

#### [轻微] `querySwapChainSupportInfo` 不校验返回值、`chooseSurfaceFormat` 可能越界
- 位置：`Vulkan/swapChain.cpp:222-246, 261`
- 现象：三次 `vkGetPhysicalDeviceSurface*` 的返回码全部丢弃；若表面不支持呈现（`VK_ERROR_SURFACE_LOST_KHR`），`mFormats` 为空，`:261 return availableFormats[0];` 直接越界。
- 证据：`Vulkan/swapChain.cpp:248-261`（`if (availableFormats.size() == 1 && ...)` 后直接 `for`，兜底是索引 0）

---

### 三、同步与帧管理

- **不存在「同一 semaphore 被两帧同时等待/重复 signal」的经典错误**：wait/signal 都取 `mCurrentFrame`（`application.cpp:327/342/355`），指令缓冲取 `imageIndex`（`:351`），两者在 `mCurrentFrame = (mCurrentFrame + 1) % mSwapChain->getImageCount()`（`:383`）下始终保持 imageCount 长度，条目本身安全。真正的风险在「索引粒度不一致 + 无重建」（见二·6/二·7）。
- **`MAX_FRAMES_IN_FLIGHT` 常量在本项目不存在**：帧数完全等于 swapchain image 数（`mCommandBuffers`、`mImageAvailableSemaphores`、`mRenderFinishedSemaphores`、`mFences` 都按 `getImageCount()` 建，`application.cpp:61-64, 180-189`）。这意味着 `mImageCount`（`swapChain.cpp:38-46`，最少 3、受 `maxImageCount` 夹紧）一旦变化，所有数组必须同步重建——当前没有任何代码做这件事。
- **fence reset 时机**：`application.cpp:359` 在 submit 前 reset 是标准写法，但**没有覆盖 submit 失败路径**（见二·6），且 `Fence::block()`（`Vulkan/fence.cpp:30-32`）不检查 `vkWaitForFences` 返回值，`VK_TIMEOUT`/`VK_ERROR_DEVICE_LOST` 会被当成「等到了」。
- **`vkDeviceWaitIdle` 覆盖不足**：全仓库仅两处——`application.cpp:231`（mainLoop 之后）、`ImGui/Interface.cpp:1700`（截图上传之后）；`Vulkan/swapChain.cpp:203-220`（SwapChain 析构）、`ImGui/Interface.cpp:162-171`（ImGui 析构）、`application.cpp:388-405`（cleanUp）都没有等待。
- **无帧率节流以外的呈现控制**：`mainLoop` 每帧固定 `sleep_for(10ms)`（`application.cpp:198`），且 present mode 优先 `MAILBOX`（`swapChain.cpp:269-271`，注释自称「追求不锁帧」）。对一个悬浮翻译窗来说 MAILBOX 只换来无意义的 GPU 空转功耗，建议默认 FIFO。

---

### 四、性能与资源

- **[中等] 每次布局转换/上传都是「新建指令缓冲 + 全局 `vkQueueWaitIdle`」**：`Vulkan/image.cpp:228-306`（`setImageLayout` 在不传 command buffer 时 `new CommandBuffer` → `submitSync` → `delete`）、`Vulkan/buffer.cpp:258-329`、`Vulkan/commandBuffer.cpp:114-123`。仅初始化阶段就会执行 `2 × mImageCount + 2 × mImageCount + 1(字体)` 次队列空闲等待。建议改为一次性 `ONE_TIME_SUBMIT` 批处理 + 单个 fence。
- **[中等] `LoadTextureFromFile` 每次截图都 `vkDeviceWaitIdle` 并且每次重新分配整条上传链**：`ImGui/Interface.cpp:1528-1700`（`image_size` 在全屏分辨率下可达 30~130 MB；`:1700 vkDeviceWaitIdle`）。建议：保留并复用上传 buffer/指令缓冲，只更新内容；上传完成用 fence 而不是 `vkDeviceWaitIdle`。
- **[中等] 每帧重新录制两级的指令缓冲**：`application.cpp:333` 每帧都 `begin/end` 主指令 + `ImGui/Interface.cpp:227-229` 每帧重录次指令。ImGui 的 draw data 每帧本来就要重录，但主指令缓冲里只有 `vkCmdBeginRenderPass/ExecuteCommands/EndRenderPass` 三条，可以直接复用（配合 fences 的 image 粒度正确性）；更值得做的是「界面未变化时不渲染」，当前已有 `GetUpdateTheScreen()` 初步判断（`Interface.cpp:187-190`）。
- **[中等] 深度/MSAA 附件是纯浪费**：见二·3。删掉后每张图省下的显存比目前渲染所需还多。
- **[轻微] sampler 全 `NEAREST` 且 `maxLod = 0`**：`Vulkan/sampler.cpp:12-13, 33`。本项目的截图/图片显示用它会明显锯齿（ImGui 自己的字体采样器是 LINEAR，见 `imgui_impl_vulkan.cpp:888-899`）。由于该 `Sampler` 类本身无人调用，实际只影响将来复用。
- **[轻微] `getMaxUsableSampleCount()` 从 64 往下试**：`Vulkan/device.cpp:447-452`。虽然正确，但在多 GPU/MSAA 场景下取到 8×/16× 会显著增加带宽；对一个悬浮小窗没有任何收益。
- **[轻微] present mode 与图像数**：`swapChain.cpp:38-46` 先按 `minImageCount + 1` 再抬到 3，`MAILBOX` 下 3 张足够；但既然没有重建逻辑，`mImageCount` 与实际返回数量在 `:103-106` 被覆写后，`getImageCount()` 与其他数组的一致性完全依赖「创建时机」，属脆弱设计。

---

### 五、设备选择与降级逻辑复核

结论：这套新增逻辑**整体是自洽的**，评分/排序/兜底与 UI 文案基本一致，下面只列不一致与风险点。

- **排序正确性（已核对，无缺陷）**：`Vulkan/device.cpp:120-141` 的 `std::stable_sort` 比较器满足严格弱序（`suitable` → `isCpu` → `Specific` 名字 → 分数），`AutoBest/AutoWorst/CPU/Specific` 四种模式语义与 `:98-117` 的注释一致；`wantCpuDevice` 分支内 `return a.score > b.score;` 与其余分支的分数方向分开，不会互相污染。
- **[中等] `deviceType` 判据只认 `VK_PHYSICAL_DEVICE_TYPE_CPU`**：`Vulkan/device.cpp:93` `c.isCpu = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);`。SwiftShader 报的就是 CPU 类型，所以正常；但若某个软件 ICD 自报 `OTHER`/`VIRTUAL_GPU`，它会被当成硬件参与「硬件优先」排序，`Variable::RunningOnSoftwareRenderer`（`:185`）也会是 false。建议同时识别 `VIRTUAL_GPU` / `OTHER` + 名字白名单（`SwiftShader`/`llvmpipe`）。「待确认」：需要一台只装了软渲染 ICD 的机器或改 ICD 报告类型才能实测。
- **[中等] 排序比较器里 `CPU` 模式仍要求 `suitable`**：`Vulkan/device.cpp:122` `if (a.suitable != b.suitable) return a.suitable;`，而 `isDeviceSuitable` = `physicalDeviceMeetsMinimumRequirements`（要求 `samplerAnisotropy` + `VK_KHR_swapchain`）。SwiftShader 支持这两项，没问题；但如果将来把要求收紧，`CPU` 模式会先被 `suitable` 过滤掉，用户明确选了「CPU 软件渲染」却仍然抛 `Device::pickPhysicalDevice: no suitable physical device`（`:205`）——与 `ensureVulkanIcdAvailable()` 里「设置里选择了 CPU 就必须用 CPU」的承诺（`Vulkan/instance.cpp:380-392`）不一致。
- **[中等] `AutoWorst` 的诊断文案与实际选择不符**：`Vulkan/device.cpp:178-181` 在「指定的设备没被识别到」时输出「已按**自动选择最高性能**改用上面这台设备」，但这段代码在 `wantLowestScore` 时也会走到；且此时选中的其实是评分**最低**的那台。纯文案问题，但会误导排障。
- **[中等] 环境变量会被写入并长期污染用户进程环境**：`Vulkan/instance.cpp:360` `::SetEnvironmentVariableA("VK_ICD_FILENAMES", ...)`。读没有副作用（`:194-198 envVarIsSet`、`:382/:394`），但写有：该变量会随进程环境被子进程继承，且退出时无处还原（无对应清理代码）。一个「划词翻译」工具改写整个进程的 Vulkan ICD，会让同进程内其它 Vulkan 组件（截图/OCR 若将来引入）也被强制到 SwiftShader。建议：只在确实要走软渲染时设置，并在退出路径（`Application::cleanUp`）恢复原值（`GetEnvironmentVariableA` 先存后还）。
- **[轻微] 探测实例与真实实例的 `apiVersion` 不一致是刻意设计，但后果未文档化**：探测用 `VK_API_VERSION_1_0`（`Vulkan/instance.cpp:240`）会漏掉只在 1.1+ 才可见的设备/驱动特性；真实实例用 `min(loader 版本, 1.3)`（`:563-568`）。两者结论在极端驱动上可能不同。
- **[轻微] SwiftShader 搜索路径硬编码 5 条 Edge/WebView 路径**：`Vulkan/instance.cpp:338-347`，含 `recursive_directory_iterator` 全盘递归（`:354`）。若这些目录很大，启动时会明显变慢；且未包含「程序自身目录」与「system32」（`:456-457` 的提示文案却建议放这两处）。
- **[轻微/正面]**：`probeVulkanDevices`（`:233-314`）在探测后销毁临时实例、不改变环境，顺序放在设置 `VK_ICD_FILENAMES` 之前（`:374-375` 注释解释充分），这一点是对的；`physicalDeviceMeetsMinimumRequirements`（`Vulkan/device.cpp:213-247`）与 `isDeviceSuitable`（`:290-296`）共用同一判据，`instance.cpp:416` 打印的「最低要求」文案与之逐字对应——**没有发现「探测说能用、选设备说不能用」的自相矛盾**。

---

### 六、可读性与可维护性

- **命名空间 `VulKan` vs 目录 `Vulkan` 大小写不一致**：`namespace VulKan`（`Vulkan/device.cpp:16` 等全部文件）与 `include "Vulkan/..."` 并存；更糟的是 `application.h:13-15` 用的是 `#include "VulKan/commandBuffer.h"`（**小写 k**），而目录名是 `Vulkan`。Windows 大小写不敏感所以能编译，一旦在大小写敏感的文件系统/CI（WSL、Linux 交叉编译、区分大小写的网络盘）上构建就立刻失败。建议统一为 `Vulkan`。
- **同类名与文件名大小写不统一**：`Vulkan/Window.h`（大写 W）与 `windowSurface.h`（小写 w）并存，在 `VS`/`CMake` 的 glob 或大小写敏感环境中易混。
- **[中等] 类型级别的「复制粘贴」重复**：`Vulkan/image.cpp:228-306 setImageLayout` 与 `:308-371 ThreadSetImageLayout` 是同一段代码的两份；`Buffer::findMemoryType` 与 `Image::findMemoryType` 重复；`application.cpp:93-135` 的 RenderPass 注释仍写着「0：最终输出图片 1：Resolve图片（MutiSample） 2：Depth图片」，而实现只有 0 号位——**注释与实现不符**，是本次最容易误导后续维护者的一处。
- **[中等] 死代码与死注释遍布**：`Vulkan/image.cpp:104, 111-130, 161-165` 被 `/* */` 注释掉的手写 `vkAllocateMemory/vkBindImageMemory`；`Vulkan/image.cpp:308-371` 注释里还有乱码「一83个」；`application.cpp:78-80` 三行被注释掉的 init_info 字段；`application.h:21` `//#include "Function/opcode.h"`。这些残留会让人误判哪些路径是活的。
- **`Vulkan/Window.cpp:49` 窗口创建失败只 `std::cerr` 不返回/不抛异常**，后续对 `nullptr` 窗口的 `glfwCreateWindowSurface` 行为未定义。
- **文件尺寸**：`Vulkan/instance.cpp` 726 行（其中 ICD 探测/降级约 300 行、诊断输出约 150 行）承担了「探测 + 降级 + 设备列举 + 实例创建 + 调试层 + 扩展过滤」六件事，建议拆成 `instance.cpp` + `icdProbe.cpp` + `diagnostics.cpp`；`Image`(342)+`Buffer`(305) 也可按「设备本地/暂存」拆。
- **`Vulkan/Window.h` 里 `setApp(GAME::Application*)` 与全局 `GAME::Application* mAppcpp`（`Window.cpp:4`）**：为托盘菜单回调引入的全局可变状态，是移植自 PixelClean 相机/输入体系的结构，本项目只用于 `WindowProc` 的 `MenuEnum`；可用 `GWLP_USERDATA` 收敛到实例上。

---

### 七、可添加的功能建议（务实可落地）

1. **补一个 `recreateSwapChain()` 并让它成为唯一重建入口**（先做这个，它是二·6 的根因）：统一负责 `vkDeviceWaitIdle` → 销毁 FrameBuffer/ImageView/深度图 → 重建 Swapchain → **同步重建 fences/semaphores/主指令缓冲数组**，并在 `render()` 里处理 `OUT_OF_DATE`/`SUBOPTIMAL`、在 `windowResized`（`Window.cpp:29-32`）里置脏标志。同时把 `mCurrentFrame` 重置为 0。
2. **加一条「可开关的校验层」通道 + 对象命名**：目前 `application.cpp:47` 硬编码 `Instance(false)`，`Instance` 里第 5 组回退配置（`instance.cpp:583`「no validation」）永远不会被触发（因为传进来的 `mEnableValidationLayer` 已是 false），`TRANSLATOR_ENABLE_LOG`（`DebugLog.h:4`）也是 0——等于**任何 Vulkan 问题都不可观测**。建议：从 `Data.ini` 读一个 `VulkanValidation=1` 开关；校验层可用时通过 `VK_EXT_debug_utils` 的 `pNext` 链把调试信使挂到 `vkCreateInstance`（当前 `setupDebugger()` 只在 `:871` 成功后单独创建，`vkCreateInstance`/`vkDestroyInstance` 自身的校验消息会丢失）；再给关键对象加 `vkSetDebugUtilsObjectNameEXT` 命名（Swapchain/Framebuffer/Pipeline/Semaphore），排查效率会提升一个量级。
3. **把「主循环 10ms 无条件睡眠 + 每帧重建指令缓冲」换成事件驱动 + 脏标记**：`application.cpp:198` 的固定 10ms 与 `:212-226` 的渲染条件其实已经接近「按需渲染」，只差把「界面是否变化」的判定前移（ImGui 的 `io.MetricsRenderVertices == 0` 或直接比较 draw data 哈希），配合 FIFO 呈现（`swapChain.cpp:269-271`）可让这个常驻工具在空闲时的 CPU/GPU 占用接近 0。

> 另建议顺手做的低风险清理：删除 `Pipeline`/`Shader`/`Sampler`/`DescriptorSet*`/`DescriptorPool`/`description.h`/`Buffer` 中未被引用的公开接口与 `getComputeCapabilities()`（见一·表格），可使 `Vulkan/` 减少约 800 行、并直接消掉本报告中「二·7 描述符写入数组」类问题的一半。



---

## 工具层与脚本引擎 · 体检报告

审查范围：`Tool/Tool.cpp`、`Tool/Tool.h`、`Tool/CMakeLists.txt`、`AngelScript/AngelScriptCode.cpp/.h`、`AngelScript/FunctionalFunctions.cpp/.h`、`AngelScript/CMakeLists.txt`、`Opcode/D.cpp`、`Opcode/Script.as`；契约文件：`Variable.h`、`application.cpp`、`Function/Translate.h`、`ImGui/Interface.h`、`ini.h`、根 `CMakeLists.txt`。
方法说明：本机无法编译（根 `CMakeLists.txt:17-26` 依赖的 `Environment/` 目录不存在），全部结论来自逐行精读 + 调用点交叉验证。**实测行数与任务书不符**：`Tool/Tool.cpp` 为 550 行（非 481），`Tool/Tool.h` 为 144 行（非 105）；本报告行号以实际文件为准。第三方源码（scriptbuilder/scriptstdstring/rapidxml/stb/ImGui 官方/vk_mem_alloc）按其契约使用，不计入缺陷。

### 一、缺陷与风险

#### [严重] ClipboardTochar 在全部重试失败后走到函数末尾，非 void 函数掉尾
- 位置：`Tool/Tool.cpp:252-276`（循环体 `Tool/Tool.cpp:254-275`）
- 现象：函数返回类型是 `std::string`，但唯一的 `return` 在成功分支里（`Tool/Tool.cpp:273`）。若 5 次重试全部失败（剪贴板被其他进程长时间占用、或 `GetClipboardData(CF_TEXT)` 恒为 NULL），`while` 结束后控制流抵达 `Tool/Tool.cpp:276` 的函数尾部，**没有 return 语句**——C++ 中这是未定义行为。MSVC 在 Release 下的典型表现是在调用方栈/寄存器上就地构造一个垃圾 `std::string`（析构时即崩），Debug 下直接 `runtime error`。
- 证据：
  ```cpp
  int ClipboardBoll = 5;
  while (ClipboardBoll > 0) { ClipboardBoll--; ...
          else { std::string CharS = (char*)GlobalLock(hmem); CloseClipboard(); return CharS; } }
  }   // Tool/Tool.cpp:275-276 —— 无 return，函数结束
  ```
- 建议修法：`std::string ClipboardTochar()` 改成失败即 `return std::string();`（或返回 `std::optional<std::string>`），并让调用方区分"空剪贴板"和"读取失败"；`Tool/Tool.cpp:255` 的重试之间补 `Sleep(1~5)`，否则 5 次空转在同一时间片内毫无意义。

#### [严重] ClipboardTochar 失败路径调用 CloseClipboard，且只读 CF_TEXT、缺 GlobalUnlock
- 位置：`Tool/Tool.cpp:256-261`、`Tool/Tool.cpp:263-273`
- 现象：三处独立缺陷。(a) `OpenClipboard` 失败却调用 `CloseClipboard()`——配对错误，成功时打开别人的剪贴板反而去关闭它，属于对 Win32 契约的误用（`CloseClipboard` 只能关闭本线程打开的剪贴板）。(b) `GlobalLock(hmem)` 的返回值**未判空**就交给 `std::string` 构造：`GlobalLock` 失败返回 NULL，`std::string((char*)NULL)` 是 UB。(c) 锁定成功后**没有 `GlobalUnlock`**，锁计数永久 +1，长时间运行会累积剪贴板内存不可回收。
- 证据：
  ```cpp
  if (!OpenClipboard(NULL)) { printf("打开剪贴板失败\n"); CloseClipboard(); continue; }   // Tool/Tool.cpp:256-261
  else { std::string CharS = (char*)GlobalLock(hmem); CloseClipboard(); return CharS; }  // Tool/Tool.cpp:270-274
  ```
- 建议修法：把剪贴板封装成 RAII（`OpenClipboard` 成功才 `CloseClipboard`；`GlobalLock` 成对 `GlobalUnlock`）。另外 `Tool/Tool.cpp:263` 只请求 `CF_TEXT`：现代 Windows 应用（浏览器、记事本、微信）多数只提供 `CF_UNICODETEXT`，此时 `GetClipboardData(CF_TEXT)` 返回 NULL 走失败路径——这是"复制后翻译没反应"这类用户抱怨的直接来源。应优先 `CF_UNICODETEXT` + `WideCharToMultiByte(CP_UTF8, ...)`。

#### [严重] screen() 的 GDI 资源全部释放错误：每截一张图泄漏一张全屏位图 + 一个窗口 DC
- 位置：`Tool/Tool.cpp:328-374`，资源创建 `Tool/Tool.cpp:330-331`、`343`、`360`，释放 `Tool/Tool.cpp:369-372`
- 现象：三处错误叠加。(a) `bm` 在 `Tool/Tool.cpp:344` 被 `SelectObject` 选入 `dc` 后，`Tool/Tool.cpp:370` 直接 `DeleteObject(bm)`——位图仍被 DC 占用时删除会失败（返回 FALSE 且句柄无效），**每张截图泄漏一张 `W×H×4` 字节的全屏位图**（1080p ≈ 8MB/张，4K ≈ 33MB/张），连点几十次截图即可耗尽 GDI/内存。(b) `_dc` 来自 `GetWindowDC`（`Tool/Tool.cpp:330`），必须用 `ReleaseDC(window, _dc)` 归还，`Tool/Tool.cpp:371` 的 `DeleteDC(_dc)` 对窗口 DC 无效且失败，**每张泄漏一个窗口 DC**（GDI 每进程句柄配额 10000）。(c) 释放顺序错误：应先 `SelectObject(dc, oldBm)` 还原旧位图，再 `DeleteObject`，最后 `DeleteDC`。
- 证据：
  ```cpp
  DeleteObject(dcf);
  DeleteObject(bm);      // Tool/Tool.cpp:370 —— bm 仍选在 dc 中，删除失败
  DeleteDC(_dc);         // Tool/Tool.cpp:371 —— 应为 ReleaseDC(window, _dc)
  DeleteDC(dc);
  ```
- 建议修法：`HGDIOBJ old = SelectObject(dc, bm);` 保存，收尾改为 `SelectObject(dc, old); DeleteObject(bm); DeleteObject(dcf); DeleteDC(dc); ReleaseDC(window, _dc);`。更彻底的做法是整段改用 `PrintWindow`/`BitBlt` + `DeleteObject` 的 RAII 包装类。

#### [严重] screen() 复用调用方缓冲区却按"当前分辨率"重算长度，分辨率变大时越界写堆
- 位置：`Tool/Tool.cpp:338-340`、`Tool/Tool.cpp:363-366`；调用方 `application.cpp:307`、`application.h:37`
- 现象：`screen(char* buf)` 的签名不带长度，无法知道 `buf` 有多大。`Tool/Tool.cpp:338` 只在 `buf == nullptr` 时按当前分辨率 `new`。首次截图后调用方（`application.h:37` 的 `char* buffer`）持有的缓冲被下一帧继续传入；而 `Tool/Tool.cpp:334-336` **每次都重新读取桌面分辨率和写回全局**。用户在两次截图之间切换显示器/改分辨率/接入远程桌面（分辨率由 1920×1080 变 3840×2160）时，`Tool/Tool.cpp:365` 的 `memcpy(&buf[...])` 就会按新尺寸写入按旧尺寸分配的堆块——**堆越界写**（Release 下 `memcpy` 无检查，直接破坏堆）。此外 `Tool/Tool.cpp:342` 的 `void* buff = buf;` 是死代码，指针立刻在 `Tool/Tool.cpp:360` 被 `CreateDIBSection` 的 out 参数覆盖。
- 证据：
  ```cpp
  if (buf == nullptr) { buf = new char[Variable::windows_Heigth * Variable::windows_Width * 4]; }  // Tool/Tool.cpp:338-340
  ...
  memcpy(&buf[(yyy * Variable::windows_Width * 4)], &((char*)buff)[((Variable::windows_Heigth - yyy - 1) * Variable::windows_Width) * 4], (4 * Variable::windows_Width));  // Tool/Tool.cpp:365
  ```
- 建议修法：改签名为 `bool screen(std::vector<uint8_t>& out)`（或返回 `std::unique_ptr<uint8_t[]>` + 出参宽高），由工具层负责分配、调用方只读；宽度高度用 `GetSystemMetrics(SM_CXVIRTUALSCREEN)`/`SM_CYVIRTUALSCREEN` + `SM_XVIRTUALSCREEN`（多显示器）而非 `GetWindowRect(GetDesktopWindow())`（只覆盖主屏，`Tool/Tool.cpp:334-336`）。

#### [严重] screen() 用已选入 DC 的位图调用 GetDIBits，返回码不检查
- 位置：`Tool/Tool.cpp:344`、`Tool/Tool.cpp:360-361`
- 现象：`bm` 已在 `Tool/Tool.cpp:344` 被选入 `dc`，而 `Tool/Tool.cpp:361` 的 `GetDIBits(dc, bm, ...)` 违反 MSDN 明确约束："The bitmap identified by hbmp must not be selected into a device context when the application calls this function." 函数会失败并返回 0，**`buff` 保持未初始化/全零**（`CreateDIBSection` 返回的 DIB 内存不保证清零），随后 `Tool/Tool.cpp:363-366` 把这块垃圾数据翻转拷进 `buf`——截图内容不可信。返回值被直接忽略，没有任何失败探测。
- 证据：
  ```cpp
  void* dcf = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &buff, NULL, NULL);   // Tool/Tool.cpp:360 返回值不判空
  GetDIBits(dc, bm, 0, Variable::windows_Heigth, buff, &bi, DIB_RGB_COLORS);   // Tool/Tool.cpp:361 返回值不检查
  ```
- 建议修法：`CreateDIBSection` 用独立的内存 DC（如 `CreateCompatibleDC(NULL)` 并只把 DIB 选入它），或先 `SelectObject(dc, old)` 把 `bm` 移出再 `GetDIBits`；至少判断 `GetDIBits` 返回值非 0 才继续，失败时返回空缓冲并由调用方跳过本次预览。相关：`Tool/Tool.cpp:347` 的 `GetObject(bm, 84, buff)` 中 84 是魔数（`tagBITMAP` 只有 32 字节，文档上写 `sizeof(BITMAP)` 即可，且这里取的 `buff` 随后被覆盖，整行无实际作用）。

#### [严重] RemoveExcessiveSpaces 对"全是空格"的输入越界读 + 使用未初始化的 jie
- 位置：`AngelScript/FunctionalFunctions.cpp:303-329`，未初始化变量 `AngelScript/FunctionalFunctions.cpp:304`，危险循环条件 `AngelScript/FunctionalFunctions.cpp:308`
- 现象：`int jie;` 声明未初始化。内层循环（`AngelScript/FunctionalFunctions.cpp:311-320`）只在遇到非空格（`FunctionalFunctions.cpp:312-315`）或走到末尾（`FunctionalFunctions.cpp:317-319`）时才给 `jie` 赋值。以 `str == "  "`（两个空格）为例推演：`i=0` 命中空格分支，内层 `x=0` 是空格、`x==str.size()-1` 为真 → `jie = 2`；`str` 变为 `""`；外层回到条件 `i < str.size() - 1`，此时 `str.size()-1 == SIZE_MAX`，`1 < SIZE_MAX` 为真 → **`str[1]` 对已空的字符串越界读**（`FunctionalFunctions.cpp:310`）。若 `str` 变成空串而内层循环体一次都不执行（`x < str.size()` 立即为假），`jie` 保持未初始化 → `str.substr(jie, str.size()-jie)`（`FunctionalFunctions.cpp:321`）以负值/垃圾值转 `size_t` 作为起点，可能抛 `std::out_of_range` 直接穿过 AngelScript 打到主循环。
- 证据：
  ```cpp
  int jie;                                          // FunctionalFunctions.cpp:304 未初始化
  for (size_t i = 0; i < str.size() - 1; i++)       // FunctionalFunctions.cpp:308 空串时为 SIZE_MAX
  { if (str[i] == ' ') { for (size_t x = i; x < str.size(); x++) { ... } str = str.substr(0, i) + str.substr(jie, str.size() - jie); } }
  ```
- 建议修法：把 `size_t` 参与的 `size()-1` 全部改成 `i + 1 < str.size()` 形式；`jie` 初始化并在内层循环后立即使用局部变量；更推荐整体重写为一次遍历的 `std::unique` 风格去重（O(n)）。该函数在 `Opcode/Script.as` 的替换流程里**每轮都被调用**，输入直接来自剪贴板/OCR，全是空格的输入并非罕见。

#### [严重] StrName：空串越界读，且路径分隔符只认反斜杠导致返回值错误
- 位置：`Tool/Tool.cpp:57-71`，循环 `Tool/Tool.cpp:60`
- 现象：(a) `Str` 为空时 `Str.size() - 1` 为 `SIZE_MAX`，`Tool/Tool.cpp:60` 立刻以 `SIZE_MAX` 下标访问——**对空 `std::string` 的越界读**，循环还要从 `SIZE_MAX` 递减到 1 才结束，实际是在任意内存上扫描直至触发访问违例。(b) 循环只把 `'\\'` 当目录分隔符（`Tool/Tool.cpp:64`），遇到正斜杠路径时 `xieI` 保持 0、`dianI` 变成"点号下标 − 1"，返回值变成"整段路径前缀 + 丢掉最后一个字符"。实测推演 `StrName("./TTF/msyh.ttf")`：`dianI = 10-1 = 9`、`xieI = 0` → 返回 `"./TTF/ms"`（期望 `"msyh"`）。
- 证据：
  ```cpp
  size_t dianI = Str.size(); size_t xieI = 0;
  for (size_t i = Str.size() - 1; i > 0; i--) {          // Tool/Tool.cpp:60
      if (Str[i] == '.') { dianI = i - 1; }
      if (Str[i] == '\\') { dianI -= i; xieI = i + 1; break; }
  }
  return Str.substr(xieI, dianI);
  ```
- 建议修法：直接 `std::filesystem::path(Str).stem().string()`。空串与无点/无扩展名情况由标准库正确覆盖。注意此函数的错误结果会被 `ImGui/Interface.cpp:742/746/748/752/762` 用作**匹配键**：`FilePath` 匹配不到时不写 `Index[0]`，于是 `ImGui/Interface.cpp:742` 的 `ModelIndex` 静默停留在 `0`，用户在上次会话选定的模型/字体/语言/脚本在重启后被悄悄切回第一项。触发路径已确认：`ImGui/Interface.cpp:1347` 写入 `"./TTF/" + name + ".ttf"`（正斜杠），下次启动 `ImGui/Interface.cpp:746/748` 就用正斜杠路径调用 `StrName`。

#### [严重] FilePath 的 directory_iterator 无异常保护，缺目录即抛异常（首次运行必崩）
- 位置：`Tool/Tool.cpp:73-88`，构造迭代器 `Tool/Tool.cpp:75`
- 现象：`std::filesystem::directory_iterator(path)` 对不存在的目录**抛出 `std::filesystem::filesystem_error`**。所有调用点都在业务代码里且不在 try 中，`main.cpp:34-41` 的 catch 也覆盖不到菜单刷新路径（`ImGui/Interface.cpp:742-762`）。产品自身的语言文件明确预期"没有 TessData/Opcode 目录"这一状态（`Variable.h` 的 `NotTesseractModelText`/`NotScript` 文案就是为此准备的），也就是说**这是被设计为会发生的输入，却会抛出未捕获异常导致进程终止**。另外该循环不写 `Index[0]` 时调用方无法区分"没匹配到"和"没目录"，且遍历顺序由文件系统决定、不稳定（同一份配置两次启动可能给出不同的下拉框顺序）。
- 证据：
  ```cpp
  void FilePath(const char* path, std::vector<std::string>* strS, const char* Suffix, const char* Name, int* Index) {
      std::string ModelFileName;
      for (const auto& entry : std::filesystem::directory_iterator(path)) {   // Tool/Tool.cpp:75 抛异常
  ```
- 建议修法：改用带 `std::error_code` 的重载（`directory_iterator(path, ec)`）或先 `if (!std::filesystem::exists(path)) return;`；成功匹配返回 `bool`，未匹配时让调用方显式回落；排序后再 push（`std::sort` 文件名）以保证下拉框顺序稳定。

#### [严重] AngelScriptCode 生命周期：构造失败路径先销毁引擎、析构再二次释放（use-after-free / double free）
- 位置：`AngelScript/AngelScriptCode.cpp:70-78`、`AngelScript/AngelScriptCode.cpp:100-108`、`AngelScript/AngelScriptCode.cpp:134-143`、析构 `AngelScript/AngelScriptCode.cpp:153-158`
- 现象：三条路径都把 `engine`/`context` 提前销毁，但对象的生命周期还在继续。(a) 脚本文件打不开时 `AngelScript/AngelScriptCode.cpp:104-107` 执行 `context->Release(); engine->ShutDownAndRelease(); mOpen = false; return;`——`context` 与 `engine` 指针仍非空，待用户切换脚本触发 `delete`（`ImGui/Interface.cpp:1331`）时，析构函数 `AngelScriptCode.cpp:155-156` 对同一对已释放对象再调一次 `Release()`/`ShutDownAndRelease()` → **double free**。(b) 任一 `GetFunction` 失败（`AngelScriptCode.cpp:72-77`）就在析构引擎后 `return`，但构造函数**没有停止**，`AngelScriptCode.cpp:128-129` 继续执行 `GetFunction` → `builder->GetModule()` 已是 `ShutDownAndRelease` 销毁过的模块，`GetFunctionByDecl` 在已释放内存上查找 → **use-after-free**。触发条件极其普通：用户在 `./Opcode/` 放一个语法错误的 `.as`（`BuildModule` 返回错误数 > 0 时模块被丢弃，句柄必然取不到），或在构建目录里根本没有 `./Opcode` 目录。(c) 析构函数 `AngelScriptCode.cpp:155` 对 `context` 不判空：`Tool/../AngelScriptCode.cpp:88-92` 的 `SetMessageCallback` 失败路径只 `return`（此时 `context` 仍是 `nullptr`）→ 空指针解引用。
- 证据：
  ```cpp
  void AngelScriptCode::GetFunction(asIScriptFunction** Function, std::string str) {
      (*Function) = builder->GetModule()->GetFunctionByDecl(str.c_str());
      if (!(*Function)) { std::cout << "Failed to get script function: " << str <<  std::endl;
          context->Release(); engine->ShutDownAndRelease(); return; }      // AngelScriptCode.cpp:74-76
  }
  AngelScriptCode::~AngelScriptCode() { context->Release(); engine->ShutDownAndRelease(); ... }   // :155-156
  ```
- 建议修法：构造期失败一律走"标记不可用 + 把指针置 nullptr"而不是"就地销毁"：`engine->ShutDownAndRelease(); engine = nullptr; context = nullptr;`；析构函数全部判空；把 `IsAvailable()`/`GetOpenBool()` 与真实状态对齐（现在 `AngelScriptCode.cpp:111` 在编译之前就把 `mOpen = true`，`BuildModule` 失败后 `GetOpenBool()` 仍返回 true，调用方以为脚本可用）。`StartNewModule`/`AddSectionFromMemory`/`BuildModule`（`AngelScriptCode.cpp:120-124`）三个返回值必须检查，`BuildModule()` > 0 时立即判定为不可用。

#### [严重] RunFunction/Register 在失败时销毁引擎后继续使用
- 位置：`AngelScript/AngelScriptCode.cpp:134-143`、`AngelScript/AngelScriptCode.cpp:161-225`
- 现象：`context->Prepare()` 失败即 `context->Release(); engine->ShutDownAndRelease(); return;`（`AngelScriptCode.cpp:138-143`）。此函数是**运行期每帧/每次热键调用**的路径（`application.cpp:248-253` 会按 `ScriptBool` 调 `RunFunction`），一旦这里释放，后续任何一次 `RunFunction` 都在已释放的 `context` 上调用 `Prepare` → 立即崩溃，且崩溃点离原因很远、难定位。`Register()`（`AngelScriptCode.cpp:161-225`）里每个 `RegisterGlobalFunction` 失败也都 `engine->ShutDownAndRelease(); return;`，而构造函数随后仍在 `AngelScriptCode.cpp:98` 调用 `engine->CreateContext()`。
- 证据：
  ```cpp
  r = context->Prepare(Function);
  if (r < 0) { std::cout << "Failed to prepare script context" << std::endl;
      context->Release(); engine->ShutDownAndRelease(); return; }   // AngelScriptCode.cpp:138-142
  ```
- 建议修法：函数级失败只做"本次不执行"（`return;`），引擎销毁只允许发生在析构函数里；`Prepare` 失败时补 `context->Unprepare()`（AngelScript 要求成对）并在下次调用前重建上下文。

#### [严重] TranslateAPI 全局指针不判空 + Translate::TranslateAPI 的 default 分支无返回值
- 位置：`AngelScript/AngelScriptCode.cpp:26-30`、`Function/Translate.h:24-39`
- 现象：`AngelScriptTranslate` 是 `nullptr` 初始化的裸全局指针（`AngelScriptCode.cpp:26`），只在 `application.cpp:89` 运行期赋值一次。脚本函数一旦在赋值之前或赋值失败后被调用，`AngelScriptTranslate->TranslateAPI(str)` 就是**空指针解引用**。另一方面，被调用的 `Function/Translate.h:24-39` 的 `switch` 在 `default` 分支没有 `return`，非 void 函数掉尾同样是 UB（返回垃圾 `std::string`，析构即崩）。这条链路是脚本的核心用途（`Opcode/Script.as:4`、`Opcode/Script.as:19`、`Opcode/Script.as:33` 三处入口都调 `TranslateAPI`），且脚本可被用户随意编辑，触发面很大。
- 证据：
  ```cpp
  Translate* AngelScriptTranslate = nullptr;                                    // AngelScriptCode.cpp:26
  std::string TranslateAPI(std::string str) { return AngelScriptTranslate->TranslateAPI(str); }   // :28-30
  ```
- 建议修法：`if (!AngelScriptTranslate) return std::string();`；`Translate.h` 的 `switch` 补 `default: return English;`；更稳妥的是把该指针换成 `Function/Translate` 的引用/`std::function` 注入，彻底取消全局裸指针。

#### [严重] 开机自启的判定分支写反：用户关闭自启时反而写入 Run 键
- 位置：`Tool/Tool.cpp:90-116`（实现）、`ImGui/Interface.cpp:1366-1368`（调用点）
- 现象：`SetModifyRegedit` 在成功时返回 `true`、失败返回 `false`（`Tool/Tool.cpp:90-116`），但调用点把 `true` 当失败处理。后果有两重：(a) 用户勾选开机自启时注册表**不写**（功能静默失效）；(b) 用户取消勾选时**照样写入** `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`（违背用户意图的系统状态变更，且每次启动都写一遍）；(c) 每次成功都往日志里写一行 `SetModifyRegedit(): Error`，把真错误淹没在假错误里。
- 证据：
  ```cpp
  if (TOOL::SetModifyRegedit("TranslatorKyi", Variable::Startup))
  {
      TOOL::logger->error("SetModifyRegedit(): Error");
  }
  ```
- 建议修法：`if (!TOOL::SetModifyRegedit(...))`；同时让 `SetModifyRegedit` 在失败时把 `GetLastError()` 一并记录。**待确认**：该调用点是否只在 `Startup` 变化时触发（若是每次写设置都调用，则注册表键会被反复重写）；确认方法：跟踪 `ImGui/Interface.cpp:1360-1370` 的外层 `if` 条件与 `SetBool` 状态机。

### 二、资源与句柄管理（GDI / 剪贴板 / 日志）

- **GDI 汇总**：一次 `screen()` 创建 5 个句柄（`GetWindowDC`、`CreateCompatibleDC`、`CreateCompatibleBitmap`、`CreateDIBSection`、`SelectObject` 引入的旧位图），正确释放应 5 步，实际只做对 2 个（`DeleteObject(dcf)`、`DeleteDC(dc)`）。以 1080p、每分钟 10 次截图计，约 80MB/分钟位图 + 10 个窗口 DC/分钟，**必现的资源泄漏**（`Tool/Tool.cpp:369-372`）。
- **剪贴板汇总**：读取侧泄漏 `GlobalLock` 锁计数（`Tool/Tool.cpp:271`）；写入侧在失败分支泄漏 `GMEM_MOVEABLE` 全局内存——`GlobalAlloc` 成功后 `GlobalLock` 失败（`Tool/Tool.cpp:305-310`）或 `SetClipboardData` 失败（`Tool/Tool.cpp:316-321`）都只 `CloseClipboard(); continue;`，`hMemory` 未 `GlobalFree`。**成功路径反而是正确的**：`SetClipboardData(CF_TEXT, hMemory)` 成功后所有权移交系统，不得再 `GlobalFree`（`Tool/Tool.cpp:316-324`）。长度按 `strlen(str.c_str()) + 1` 计算（`Tool/Tool.cpp:297`）对 `CF_TEXT` 是自洽的——任务书猜测的 `(len+1)*sizeof(wchar_t)` 在该实现里不存在。
- **日志对象**：`Tool/Tool.cpp:9-19` 的 `SpdLogInit` 用 `new` 构造 `spdlog::logger` 后从不 `delete`，也不注册进 spdlog registry，析构顺序完全依赖进程退出；`Tool/Tool.cpp:7` 的 `TOOL::logger` 是命名空间作用域指针，静态零初始化为 `nullptr`，因此**所有使用点都必须判空**，而 `main.cpp:39` 的 `TOOL::logger->error(e.what())`、`ImGui/Interface.cpp:1367` 都没有判空。`SpdLogInit` 本身在 `main.cpp:26` 调用且不在 try 内（`main.cpp:34` 的 try 在其后），一旦 `logs/` 目录不可写就抛出 `spdlog_ex` 直接终止进程。建议改为函数内 `static std::shared_ptr<spdlog::logger>` + `spdlog::register_logger`，并让 `logger` 的访问走一个 `Logger()` 访问函数，内部保证非空（缺失时回退到 stderr 或空对象 sink）。
- **日志文件**：`Tool/Tool.cpp:9-19` 写死相对路径 `logs/Error.txt`，与 `./Opcode`、`./TessData` 一样依赖进程当前工作目录（根 `CMakeLists.txt:52-58` 会把资源拷到构建目录，说明这是预期行为），但从桌面快捷方式以外的方式启动（拖到别的目录、从 `Program Files` 启动）会写到意外位置或直接失败。另外**没有轮转**（未用 `rotating_file_sink`），`Tool/Tool.cpp:9-19` 的 file 级别是 `trace`，长期运行会把 `Error.txt` 撑到无限大。命名空间叫 `TOOL`，文件名却叫 `Error.txt`，语义也不符。

### 三、AngelScript 集成与安全

- **暴露给脚本的 API 面**：`AngelScript/AngelScriptCode.cpp:166-216` 注册 `print`/`SetOutput`/`SetInput`/`GetInput`/`TranslateAPI`，`AngelScript/FunctionalFunctions.cpp:12-90` 注册 13 个纯字符串函数。逐条核对后，**没有任何文件读写、网络、内存原语或进程操作**，脚本只能处理字符串并经 `SetOutput` 回写——这个受控面是设计上的优点，值得保留。
- **潜在后门（当前未启用）**：`AngelScript/AngelScriptCode.cpp:173-187` 整段被注释的 DLL 注册代码会把 `Opcode/D.cpp:1-12` 的 `print(__int64 P)` 暴露成脚本的 `void print(int64)`，而该函数把整数当 `std::string*` 解引用：
  ```cpp
  DLLEXPORT void print(__int64 P) { std::cout << (*((std::string*)P)).c_str(); }   // Opcode/D.cpp:1-12
  ```
  一旦取消注释，脚本即可传任意地址读取任意内存（且 `Opcode/D.cpp` 由根 `CMakeLists.txt:13` 的 `aux_source_directory(. DIRSRCS)` 编进主程序）。**报告结论：当前处于注释状态，不构成现实风险；但该文件不应留在构建树中**，建议删除或加 `#if 0` + 注释说明"仅为历史样例，禁止启用"。
- **异常穿透**：`AngelScript` 不会捕获 C++ 异常。`FunctionalFunctions.cpp` 里多处 `substr` 在越界参数下会抛 `std::out_of_range`（`FunctionalFunctions.cpp:120-130`、`FunctionalFunctions.cpp:303-329`），这些异常会直接穿过 `context->Execute()`（`AngelScriptCode.cpp:146`）打到 `application.cpp` 的主循环。`application.cpp:234-313` 的热键处理没有 try/catch，异常逃逸到 `main` 就是 `std::terminate`。建议在 `RunFunction` 内部包一层 try/catch 并把异常内容写入日志，同时把脚本层函数改成"永不抛"的实现（全部用 `find` 的返回值做判断，不做裸 `substr`）。
- **执行错误不可诊断**：`AngelScriptCode.cpp:146-149` 对 `Execute()` 的任何非 `asEXECUTION_FINISHED` 结果都只打印一行 `Failed to execute script`，不区分 `asEXECUTION_EXCEPTION`/`asEXECUTION_ABORTED`/`asEXECUTION_SUSPENDED`，不调用 `GetExceptionString()`/`GetExceptionLineNumber()`，也从不 `Reset()` 上下文。脚本里一行除零或空引用，用户看到的信息量等于零。
- **单例线程安全**：`AngelScriptCode.h:13-18` 的 `GetAngelScriptCode()` 是裸 `new` 的懒汉单例，无锁无 `std::call_once`；`ImGui/Interface.cpp:1331` 用 `delete` 销毁它。当前若脚本只在主线程执行则无实际竞态，但改为多线程执行前必须先修（**待确认**：是否存在非主线程调用 `RunFunction`；确认方法：全仓搜索 `RunFunction` 与 `std::thread`）。

### 四、编码与字符串约定

- **剪贴板按 ANSI/GBK 解释**：`application.cpp:247`、`ImGui/Interface.cpp:241-276`、`ImGui/Interface.cpp:608-618` 都是 `TOOL::UnicodeToUtf8(TOOL::ClipboardTochar())`。`ClipboardTochar` 返回的是 `CF_TEXT`（系统 ANSI 代码页，中文 Windows 上是 GBK），再经 `s2ws` 按当前 locale 转宽字符（`Tool/Tool.cpp:160-174`），两步都依赖 locale 且链条很长。浏览器/现代应用只提供 `CF_UNICODETEXT` 时整条链路直接失败（见上）。建议收敛为一条路径：`CF_UNICODETEXT` → `WideCharToMultiByte(CP_UTF8, ...)` → `std::string`（UTF-8），并让 `ClipboardTochar` 返回明确 UTF-8 语义（改名 `ClipboardToUtf8`，避免重蹈 `UnicodeToUtf8` 这种名不副实的覆辙）。
- **setlocale 是进程级全局状态**：`Tool/Tool.cpp:143-157` 与 `Tool/Tool.cpp:160-174` 各自 `setlocale(LC_ALL, "chs")` 然后恢复，非线程安全，且异常路径下 locale 不还原（`delete[]` 不在 RAII 里）。非中文系统上 `"chs"` 不是合法 locale 名，`setlocale` 返回 NULL 被静默忽略，实际按系统 locale 转换——**跨语言系统上的行为不可预期**。建议全部换成显式代码页的 `MultiByteToWideChar`/`WideCharToMultiByte`，彻底不碰 locale。
- **Utf8ToUnicode 解码无校验**：`Tool/Tool.cpp:207-250` 手写 UTF-8 解码，`(utf8_str[i+1] & 0x3F)` 之类的续字节既不校验 `0x80` 掩码也不校验 `i+1 < size`——被截断的 UTF-8（爬虫/OCR/剪贴板都可能给出）会**越界读**最多 3 字节；4 字节序列（emoji、生僻汉字）写入 Windows 的 16 位 `wchar_t` 会被截断成错误字符。建议改用 `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)`，非法序列显式替换为 U+FFFD。
- **字符串返回值所有权**：任务书担心的"`char*` 返回 + static 缓冲不可重入"在该项目**不成立**——`Tool/Tool.cpp:252` 的 `ClipboardTochar`、`Tool/Tool.cpp:177` 的 `UnicodeToUtf8`、`Tool/Tool.cpp:207` 的 `Utf8ToUnicode`、`Tool/Tool.cpp:57` 的 `StrName` 全部返回 `std::string`（值语义），无 static 缓冲、无跨调用覆盖问题。唯一返回裸指针的是 `Tool/Tool.cpp:328` 的 `screen(char*)`，其所有权与长度约定完全未定义（见第一节对应条目）。
- **多字节字符串被逐字节处理**：`AngelScript/FunctionalFunctions.cpp:270-282`（`LeaveOnlyLetters`）、`FunctionalFunctions.cpp:356-379`（`Autowrap`）用 `std::regex` 对 UTF-8 字节流逐字节匹配，中日韩字符（3 字节）会被拆成 3 个"非字母"字节并各自替换为空格或在中间插换行。功能上"能跑"，但对中文输入的结果是错误的字符边界。**待确认**：MSVC 的 `std::regex` 遇到 ≥0x80 的字节是否抛 `std::regex_error`；确认方法：单独编译 `LeaveOnlyLetters` 并喂入 `"中文 abc"` 观察是否抛异常（若抛，则它会经 `Execute()` 穿透到主循环，升级为严重）。
- **`Autowrap` 的换行阈值**：`FunctionalFunctions.cpp:356-379` 用 `Cut >= Variable::WrapSize`，而 `Variable::WrapSize` 由 `ImGui/Interface.cpp:300` 的 `kuangshu / int(Variable::FontSize)` 计算——字号大于窗口宽度或字号配置为 0 时结果为 0，导致**每个字符后都插一个换行**。建议在计算侧 `std::max(1, ...)`，并在 `Autowrap` 内对 `WrapSize == 0` 直接返回原串。

### 五、可读性与可维护性

- **巨函数**：`Tool/Tool.cpp:328-374`（`screen`，47 行混合了窗口枚举、位图创建、像素格式转换、行翻转、资源释放五件事）；`Tool/Tool.cpp:426-549`（计时器家族，124 行 8 个函数共享同一组全局数组）；`AngelScript/AngelScriptCode.cpp:80-132`（构造函数 53 行，承担引擎创建/回调设置/API 注册/文件读取/编译/句柄获取六件事，且其中任何一步失败都不能正确回滚——见第一节）。
- **原生 Windows 结构体对齐/魔数**：`Tool/Tool.cpp:347` 的 `GetObject(bm, 84, buff)`；`Tool/Tool.cpp:351` 用 `tagBITMAPINFO bi;` 后逐个字段赋值而**没有 `memset` 或 `bi.bmiColors` 初始化**，`biSizeImage`/`biXPelsPerMeter` 等字段虽被显式赋 0，但依赖"恰好都写了"这一事实，任何新增字段遗漏都是静默的未初始化读。应改为 `tagBITMAPINFO bi{};`（值初始化）。
- **头文件耦合**：`Tool/Tool.h:5` 包含 `../Variable.h`（业务配置头）、`Tool/Tool.h:8-12` 包含 4 个 spdlog 头。结果是**任何只想用 `StrName` 的 TU 都被迫拖进 spdlog 头 + INI 解析头**，编译时间与耦合度都被放大。建议：把 `logger` 的声明拆到 `Tool/Log.h`，`Tool.h` 去掉 `../Variable.h`（`windows_Width/Heigth` 依赖可改为通过出参返回）。
- **头注释/声明与实现不一致的零碎问题**：`Tool/Tool.h:41/43` 的注释与函数对调（"string 转 wstring"挂在 `ws2s` 上，实际 `Tool.h:41` 是 `ws2s`）；`Tool/Tool.h:21-22` 只声明了 `Converter<T>` 模板而定义在 `Tool/Tool.cpp:21-33`，任何其他 TU 实例化它都会**链接失败**（当前无人调用，是埋好的雷）；`Tool/Tool.h:59-63` 的 `extern const int number; extern const double miao_time;` 中 `const` 默认内部链接，跨 TU 使用需 `extern` 定义配套（**待确认**：`Tool.cpp` 中是否以 `extern` 形式定义；确认方法：搜索 `const int number`）。
- **两个 CMakeLists 都是"隐形"配置**：`Tool/CMakeLists.txt` 与 `AngelScript/CMakeLists.txt` 各只有 3 行 `file(GLOB_RECURSE ...)` + `add_library(...)`。`GLOB_RECURSE` 不会在新增文件时自动重跑（依赖手工重跑 cmake），而且会把目录下**所有** `.cpp` 一并编译（含第三方 `scriptbuilder.cpp`/`scriptstdstring.cpp`，以及任何将来放进来的临时文件）；两者都没有 `target_include_directories`/`target_link_libraries`，全靠根 `CMakeLists.txt:17-26` 的全局 `include_directories` 和全局链接列表。建议至少改为显式列出源文件 + `target_include_directories(ToolLib PUBLIC ...)`，让依赖关系在 target 上可见。
- **脚本示例重复**：`Opcode/Script.as:12-23` 的 `ScreenshotFunction` 与 `Opcode/Script.as:26-37` 的 `ChoiceFunction` 函数体逐行一致（重复代码，可合并为一个共用实现）；`ReplaceFunction`（`Opcode/Script.as:1-9`）漏了 `RemoveExcessiveSpaces`，与另外两个入口不一致——而该函数正是第一节里最危险的函数，示例脚本的路径覆盖不均匀会误导使用者。
- **错误处理风格不统一**：同一层里混用 `printf`/`puts`/`std::cout`/`spdlog` 四种输出（`Tool/Tool.cpp:258/266` 用 `printf`，`Tool/Tool.cpp:284` 用 `puts`，`AngelScript/AngelScriptCode.cpp:58` 用 `std::cout`，只有 `ImGui/Interface.cpp:1367` 用 logger）。关键事实：**Release 构建是 `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`（根 `CMakeLists.txt:113`），没有控制台**，所以工具层与脚本引擎的全部诊断信息在发布版里**一个字都看不到**。这是本项目可观测性最大的缺口。

### 六、可添加的功能建议（务实可落地）

1. **日志统一 + 脚本错误可见**：把 `Tool/Tool.cpp:258/266/284`、`AngelScript/AngelScriptCode.cpp:58-67/73/90/103/139/148`、`FunctionalFunctions.cpp:6-10` 的全部 `printf`/`puts`/`std::cout` 改为 `TOOL::logger`（缺失时回退到 `OutputDebugString`）；`SpdLogInit` 换成 `rotating_file_sink`（如 5MB × 3）；再在 `AngelScriptMessage` 回调里加一个"最近 N 条脚本错误"的环形缓冲（含列号 `msg->col` 与 `section`），由 ImGui 提供一个"脚本诊断"面板展示——用户自己写的 `.as` 出错时能自助定位，而不是黑箱失效。
2. **把 `screen()` 的缓冲区契约变成类型**：改为 `bool screen(std::vector<uint8_t>& out, int& w, int& h)`（内部按需 `resize`，并显式声明输出为 BGRA 或加一个 `to_rgba` 参数），一举消除"无长度参数导致换分辨率越界"和"BGRA/RGBA 与 Vulkan 图像格式不一致"两个问题；资源释放改用 RAII 小类，并顺手支持多显示器（`SM_CXVIRTUALSCREEN`）。附带收益：`ImGui/Interface.cpp:1535/1566` 的图像与视图格式不一致（`VK_FORMAT_R8G8B8A8_UNORM` vs `VK_FORMAT_B8G8R8A8_UNORM`，且错误码被 `//check_vk_result(err)` 吞掉）可以同时定案——**待确认**，确认方法：开启 Vulkan 校验层观察 `VUID-VkImageViewCreateInfo-image-01762`。
3. **给脚本引擎加"执行护栏"**：`RunFunction` 内用 try/catch 兜住所有 C++ 异常并写日志；`Prepare`/`Execute` 失败时打印异常行号与调用栈（`GetExceptionString`/`GetExceptionLineNumber`）；执行前检查 `GetOpenBool()` 与句柄非空；给 `context->Prepare` 配对的 `Unprepare`；并在引擎上设置 `SetEngineProperty(asEP_MAX_STACK_SIZE)` 之类的资源上限，防止用户脚本死循环卡死主线程（当前 `Execute()` 是同步调用，一个 `while(true)` 会冻住整个 UI）。

---

**核对说明**：本报告所有 `file:line` 均在写入后按实际文件内容复核；标"待确认"的 6 处均给出了具体确认方法（Vulkan 校验层、独立编译 `LeaveAllLetters`、跟踪 `SetModifyRegedit` 调用条件、搜索 `RunFunction` 的线程归属、检查 `Tool.h` 中 `const` 的 extern 定义、跟踪 `Interface.cpp:1360-1370` 的触发条件）。未能核实的是"运行时是否真的崩溃"这一层——本机无法编译，上述结论均为静态精读 + 控制流推演，其中 `RemoveExcessiveSpaces("  ")`、`StrName("")`、`StrName("./TTF/msyh.ttf")` 三条给出了逐行走查的推演过程，建议编译后按这三条构造最小复现用例验证。
