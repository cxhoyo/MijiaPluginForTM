# 米家插座功率 TrafficMonitor 插件

## 简介

这是一个 TrafficMonitor 插件，可以在 Windows 任务栏实时显示米家/酷控（cuco）智能插座的功率数值，并可选开启功率历史记录功能。
<img width="533" height="598" alt="插件截图" src="https://github.com/user-attachments/assets/90fd0cb1-d807-4bc9-a64a-b7a10b8ae9c9" />



**主要功能：**
- 📊 实时在任务栏显示功率（W）
- 💾 可选启用功率历史记录（按分钟采样，最多保存7天）
- 📈 鼠标悬停提示：显示10分钟/1小时/24小时最大/最小/平均功率
- ⚙️ 设置对话框：配置设备IP、Token，调整显示格式
- 🔗 支持断线自动重连

---

## 🚀 快速安装

1. **获取 IP 和 Token** → 使用 [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor/releases)
2. **打开插件目录** → TrafficMonitor 右键 → 选项 → 常规设置 → 下滑找到插件管理 → 打开插件目录
3. **获取 DLL** → 从 GitHub Release 下载，或从 GitHub Actions 的构建产物中获取 `MijiaPower.dll`
4. **放入 DLL** → 将 `MijiaPower.dll` 复制到目录内
5. **重启 TrafficMonitor** → 完全退出后重新启动
6. **填写配置** → 在插件选项中输入 IP 和 Token，点击"测试连接"

---

## 支持说明

- 插件会通过 `miIO.info` 获取设备型号，并使用**精确型号匹配**选择功率属性
- 当前已知兼容型号：`cuco.plug.v3`、米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`）
- 米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`）的功率属性为 `siid=5, piid=6`，原始值除以 `100` 后得到瓦特值
- 非精确匹配型号不会套用米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`）的映射，避免误读未知型号的属性

---

## 配置说明

首次使用需要配置设备信息：

1. 在 TrafficMonitor 任务栏右键 → **选项**，或直接右键插件显示区域 → **选项**
2. 填写以下内容：
   - **设备 IP**：米家插座的局域网 IP 地址（如 `192.168.1.100`）
   - **Token**：32位十六进制字符串（获取方法见下方）
   - **名称**：设备昵称，显示在提示信息中
3. 点击 **测试连接** 验证配置是否正确
4. 点击 **确定** 保存并生效

### 如何获取 Token

#### 🌟 推荐方案：使用 Xiaomi Cloud Tokens Extractor

最便捷和推荐的方法是使用以下项目，它可以自动从米家云提取所有设备的 Token 和 IP：

**项目地址**: [Xiaomi-cloud-tokens-extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor)

**使用步骤**：

1. **下载工具**
   - 访问 [Release 页面](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor/releases)
   - 下载最新版本（Windows 用户下载 `.exe` 文件）

2. **运行提取器**
   ```
   点击双击 .exe 文件运行
   或在命令行中：
   xiaomi_cloud_tokens_extractor.exe
   ```

3. **输入米家账户信息**
   ```
   Username: 你的米家账户（邮箱或手机号）
   Password: 你的米家密码
   Server:   China (中国用户选择此项)
   ```

4. **查看结果**
   - 工具会自动列出所有米家设备
   - 复制你的插座设备的 **IP** 和 **Token**
   ```
   设备名称: 客厅插座
   Model:    cuco.plug.v3
   IP:       192.168.1.100
   Token:    a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6
   ```

5. **保存信息**
   - 在记事本或文档中保存好 IP 和 Token
   - ⚠️ Token 是敏感信息，请妥善保管，不要分享给他人

#### 📖 其他获取方法

如果上述方法不可用，也可以尝试：

**方法 B - 使用 Python miio 工具**
```bash
# 安装 python-miio
pip install python-miio

# 提取所有设备的 Token
python -m miio.extract_tokens

# 会显示所有设备的信息
```

**方法 C - 使用 MiToolKit**
- 项目地址: [MiToolKit](https://github.com/ultraicy/MiToolKit)
- 下载运行后，选择"提取 Token"功能
- 选择要提取的设备

**方法 D - 米家 APP 抓包提取**
- 使用 Fiddler 或 Charles 代理
- 在米家 APP 中操作设备
- 在代理日志中查找相关请求
- 从 JSON 响应中提取 Token 字段

---

## 📖 使用教程

### 第一步：获取 IP 和 Token

1. 下载 [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor/releases)
2. 运行 `.exe` 文件
3. 输入米家账户（邮箱/手机号）和密码，选择 China 服务器
4. 等待提取完成，记录你的插座设备的 **IP** 和 **Token**

### 第二步：安装插件

1. 右键 TrafficMonitor 任务栏 → **选项**
2. 选择 **常规设置** → 下滑找到 **插件管理**
3. 点击 **打开插件目录**
4. 将 `MijiaPower.dll` 复制到打开的目录中
5. **完全关闭 TrafficMonitor**，重新启动

### 第三步：配置插件

1. 重启后，右键 TrafficMonitor → **选项**
2. 左侧菜单选择 **MijiaPowerPlugin**
3. 填写设备信息：
   - **设备 IP**: 从第一步获取
   - **Token**: 从第一步获取  
   - **名称**: 自定义设备名称（如"客厅插座"）
4. 点击 **测试连接** 验证
5. 点击 **确定** 保存

![插件配置界面](image.png)

### 配置选项说明

| 选项 | 说明 |
|------|------|
| 启用功率历史记录 | 开启后按分钟采样，保存7天数据 |
| 显示"功率:"标签 | 任务栏是否显示"功率:"前缀 |
| 显示 W 单位 | 数值后是否显示"W"单位符号 |
| 采集间隔（秒） | 查询设备的时间间隔，建议 3-5 秒 |
| 小数位数 | 显示精度：0=整数，1=一位小数，2=两位小数 |

---

## 📖 详细使用教程

### 安装前检查清单

- ✅ TrafficMonitor 已安装（v1.74 或更高版本）
- ✅ 米家账户可正常登录
- ✅ 智能插座已添加到米家 APP
- ✅ 智能插座与电脑在同一局域网

### 完整安装和配置步骤

#### 第一步：获取设备 IP 和 Token

1. 下载 [Xiaomi Cloud Tokens Extractor](https://github.com/PiotrMachowski/Xiaomi-cloud-tokens-extractor/releases)

2. 运行 `.exe` 文件

3. 输入米家账户信息：
   ```
   Username: 你的米家账号（邮箱/手机号）
   Password: 你的米家密码
   Server:   China
   ```

4. 等待提取完成，找到你的插座设备：
   ```
   设备: 客厅插座
   IP: 192.168.1.100
   Token: a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6
   ```

5. 用记事本记录下 **IP** 和 **Token**

#### 第二步：安装插件 DLL

1. **找到 TrafficMonitor 目录**
   - 通常在：`C:\Program Files\TrafficMonitor\`
   - 或右键 TrafficMonitor 快捷方式 → 打开文件位置

2. **创建 plugins 文件夹**（如果不存在）
   ```
   C:\Program Files\TrafficMonitor\plugins\
   ```

3. **放入 DLL 文件**
   - 将 `MijiaPower.dll` 复制到 `plugins` 文件夹

4. **完全关闭 TrafficMonitor**
   - 右键任务栏图标 → 退出

5. **重新启动 TrafficMonitor**

#### 第三步：配置插件

1. **打开插件选项**
   - 右键 TrafficMonitor 任务栏区域 → 选项
   - 或左侧菜单 → MijiaPowerPlugin → 选项

2. **填写设备信息**
   ```
   设备 IP:  192.168.1.100    (从第一步获取)
   Token:    a1b2c3d4...      (从第一步获取)
   名称:     客厅插座          (可选，自定义名称)
   ```

3. **点击"测试连接"验证**
   - 若显示连接成功 ✅ → 配置正确
   - 若显示连接失败 ❌ → 重新检查 IP 和 Token

4. **点击"确定"保存**

#### 第四步：开始使用

- 插件会立即显示实时功率
- 可以看到任务栏中显示：`功率: XX.X W`
- 鼠标悬停会显示详细的统计信息

### 常见问题快速解决

| 问题 | 症状 | 解决方案 |
|------|------|---------|
| 连接失败 | 显示"连接失败" | 检查 IP 和 Token 是否正确 |
| 没有显示 | 看不到功率数值 | 确认 DLL 文件在 plugins 目录中 |
| 数据异常 | 显示 0W 或错误值 | 检查设备是否开启，重启插件 |
| Token 失败 | 工具无法提取 | 确保米家账户密码正确，网络正常 |
| 型号不兼容 | 测试连接成功但功率异常 | 先确认 `miIO.info` 返回的型号是否为 `cuco.plug.v3` 或米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`） |

### 高级配置选项

安装完成后，可以在插件选项中调整：

| 选项 | 默认值 | 推荐值 | 说明 |
|------|--------|--------|------|
| 启用历史记录 | 关闭 | 开启 | 记录功率数据用于分析 |
| 显示功率标签 | 开启 | 开启 | 任务栏显示"功率:"文字 |
| 显示 W 单位 | 开启 | 开启 | 数值后显示"W" |
| 采集间隔(秒) | 3 | 3-5 | 越小更新越快，但消耗更多资源 |
| 小数位数 | 1 | 1 | 显示精度（0-2位） |

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `PluginInterface.h` | TrafficMonitor 插件接口定义 |
| `MiioDevice.h/.cpp` | 纯C++ miIO协议实现（AES-128-CBC + UDP） |
| `PowerHistory.h/.cpp` | 功率历史采样与统计 |
| `PluginConfig.h/.cpp` | 插件配置（INI文件读写） |
| `MijiaPowerPlugin.h/.cpp` | 插件主类（ITMPlugin/IPluginItem实现） |
| `OptionsDlg.h/.cpp` | 设置对话框（纯Win32） |
| `pch.h/.cpp` | 预编译头 |

---

## 配置文件位置

插件配置文件默认保存在 TrafficMonitor 的插件配置目录：
```
%AppData%\TrafficMonitor\plugins\MijiaPower\
  MijiaPower.ini          ← 设备信息和选项
  MijiaPower_history.json ← 功率历史记录（如果启用）
```

说明：
- `MijiaPower_history.json` 会在启用历史记录且插件采集到功率数据后自动创建
- 历史记录会定期保存，并在插件正常停止时再次保存

---

## 注意事项

1. 插件通过 UDP 协议（端口 54321）直接与设备通信，**设备必须与电脑在同一局域网**
2. miIO 协议需要正确的 Token，错误的 Token 会导致连接失败
3. 采集间隔不建议设置低于 2 秒，以免对设备造成过多请求
4. 不同型号的插座功率属性可能不同。当前插件支持基于精确型号匹配的属性映射

---

## 兼容的设备

基于原项目 `mijia_plug` 的设备属性定义：

| 属性 | siid | piid | 说明 |
|------|------|------|------|
| 开关 | 2 | 1 | 插座开关状态 |
| 功率 | 11 | 2 | `cuco.plug.v3` 当前功率（W） |
| 功率 | 5 | 6 | 米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`）当前功率（原始值除以 100 得到 W） |
| 能耗 | 11 | 1 | 累计用电量 |
| 温度 | 12 | 2 | 插座温度 |

已知兼容型号：`cuco.plug.v3`（米家智能插座3）、米家智能插座2 蓝牙网关版（`chuangmi.plug.212a01`）

---

## 开发依赖

插件完全无第三方依赖，所有加密算法（AES-128-CBC、MD5）均为纯 C++ 实现，仅依赖 Windows 系统 API（ws2_32.lib、comctl32.lib）。

---

## 构建与发布

### GitHub Actions

- `Build DLL` 会在推送到 `main` 或 `support/**` 分支时运行，也支持手动触发
- 构建产物包含 `MijiaPower.dll` 和 `MijiaPower.pdb`
- `Release DLL` 会在推送 `v*` 标签时创建 GitHub Release，也支持手动输入 `tag_name` 触发

### 本地编译

- 运行仓库根目录下的 `compile.ps1`
- 或使用 Visual Studio / MSBuild 编译 `Release | x64`
- 默认输出目录为 `bin/Release/x64/`
