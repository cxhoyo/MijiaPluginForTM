# 米家多设备功率 TrafficMonitor 插件

这是 [`cxhoyo/MijiaPluginForTM`](https://github.com/cxhoyo/MijiaPluginForTM) 的多设备版本。每台米家/酷控智能插座注册为独立的 TrafficMonitor 显示项，通过局域网 miIO 协议读取实时功率。

## 多设备显示

在 TrafficMonitor 的任务栏设置中关闭“水平排列”后，显示项按两项一列排列：

```text
电脑插座：100W    第三台：50W
空调插座：200W
```

奇数个设备的最后一项固定在新列上行。关闭插件的“显示设备名称标签”后，每项只显示数值，例如 `86.2W`。

![TrafficMonitor 任务栏中的双设备功率显示](docs/images/multi-device-taskbar.png)

每台设备拥有独立的 TrafficMonitor item ID，因此可以单独显示、隐藏、排序和设置颜色。设备改名不会改变 item ID，也不会丢失 TrafficMonitor 中已有的项目设置。

## 配置

插件设置窗口提供：

- 添加、删除、上移和下移设备；
- 为每台设备配置名称、局域网 IP 和 Token；
- 单独测试选中设备的连接；
- 共享设置采集间隔、历史记录、标签、单位和小数位数；
- 清除选中设备或全部设备的历史记录。

设备增删或排序会改变 TrafficMonitor 枚举到的显示项，需要重启 TrafficMonitor。已有设备的名称、IP、Token 和共享显示设置保存后立即应用。

Token 是敏感凭据。插件只在本机配置文件中保存 Token，设置窗口使用密码掩码，不会把 Token 写入日志。

## 从单设备版本迁移

首次加载时，插件会识别原 `MijiaPower.ini` 中一个或多个重复的 `[Device]` 配置块：

1. 在同目录创建 `MijiaPower.ini.legacy.bak`；
2. 按原出现顺序导入全部设备；
3. 将配置写成带稳定 item ID 的多设备格式；
4. 第一台设备继续使用 `MijiaPowerW`，保留原 TrafficMonitor 显示和颜色设置；
5. 其他设备重启后需在 TrafficMonitor 的“显示项目”中启用一次。

迁移和运行时配置位于 TrafficMonitor 传入的插件配置目录。插件不会把配置或 Token 写进源码目录。

## 安装

1. 完全退出 TrafficMonitor。
2. 将 x64 Release 的 `MijiaPower.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
3. 启动 TrafficMonitor，在插件选项中检查迁移后的设备列表。
4. 在任务栏“显示项目”中启用新增设备，并按需要调整项目顺序和颜色。
5. 设备集合变化后再次重启 TrafficMonitor。

插件通过 UDP `54321` 端口直连设备，电脑和插座必须位于可互通的局域网。当前功率属性沿用上游实现的 `siid=11, piid=2`，已知兼容 `cuco.plug.v3`。

## 构建

需要 Visual Studio 2022 Build Tools，并安装“使用 C++ 的桌面开发”和 Windows SDK：

```powershell
.\compile.ps1 -Configuration Release -Platform x64
```

产物位于 `bin\Release\x64\MijiaPower.dll`。GitHub Actions 也会构建 x64 Release、检查 `TMPluginGetInstance` 导出并上传 DLL artifact。

## 参考

- [TrafficMonitor 插件开发指南](https://github.com/zhongyang219/TrafficMonitor/wiki/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97)
- [cxhoyo/MijiaPluginForTM](https://github.com/cxhoyo/MijiaPluginForTM)
