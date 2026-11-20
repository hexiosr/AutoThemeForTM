# 自动深浅色（TrafficMonitor 插件）

按固定时间或日出日落自动切换 Windows 深浅色主题。任务栏显示当前主题，点击立即切换（单击/双击/不切换可设），右键菜单有「切换深色/浅色」命令。

## 构建

`build_mingw.bat`，使用 `F:\Tools\App\mingw64` 的 MinGW-w64 静态链接。

## 安装

1. 退出 TrafficMonitor
2. 把 `bin\Release\x64\AutoTheme.dll` 复制到 `plugins` 目录（也可直接从 [Releases](https://github.com/hexiosr/AutoThemeForTM/releases) 下载）
3. 在 TrafficMonitor 的显示文本设置里启用「主题模式」
4. 启动 TrafficMonitor

## 设置

在 TrafficMonitor 的插件列表里打开「自动深浅色」设置：

- 固定时间：下拉选择深色开始 / 结束时间（5 分钟步进），默认 19:00 – 07:00
- 日出日落：勾选自动定位会立即按 IP 定位并填入经纬度，取消勾选后可手动填写
- 关闭自动：只保留手动切换
- 任务栏点击切换：不切换 / 单击切换 / 双击切换，选择点击任务栏主题项时的行为；选「不切换」时仍可用右键菜单切换

## 说明

- 自动切换会把「应用」和「系统」的深浅色设为同值（`AppsUseLightTheme` 与 `SystemUsesLightTheme` 一起写入）；习惯于两者分开设置的用户请注意
- 手动切换后，到下一次自动切换时刻前不会被覆盖
- 自动定位依次尝试 ip-api.com（IPv4）、ipinfo.io、ipwho.is，仅首次和每 30 天刷新时联网；国内 IPv6 出口会被 ipwho.is 误判，故放在最后
- 切换方式：写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize` 下的 `AppsUseLightTheme`、`SystemUsesLightTheme`，然后广播 `WM_SETTINGCHANGE`（`ImmersiveColorSet`）
- IP 定位只精确到城市，偏差大时请手动填写坐标
- 任务栏项的文字可在 TrafficMonitor 的显示文本设置里改名或隐藏
