# TF 卡配置（Waveshare ESP32-S3-Touch-LCD-3.49 V2）

本目录提供要复制到 **TF 卡根目录** 的文件模板。

1. 将 TF 卡格式化为 FAT32（不要使用 exFAT）。
2. 编辑本目录的 `config.json`，填写真实 Wi-Fi 名称及密码，按实际服务地址修改 AI URL。
3. **只把 `config.json` 复制到 TF 卡根目录**：`/config.json`，不要把整个 `tf卡` 文件夹复制到卡上。
4. 插入 TF 卡，启动或重启设备。启动时读取一次文件；修改后需要重启。
5. Wi-Fi 与 AI 设置页面仅用于查看状态，不能修改配置。

## 配置字段

- `wifi.ssid`：必填，最长 32 字节；支持 2.4 GHz Wi-Fi。
- `wifi.password`：可填空字符串（开放网络），最长 64 字节。
- `ai.url`：本地 AI 服务 HTTP URL，按实际接口填写；当前只用于显示/保留连接参数，**并不代表已经实现 AI 连通性探测**。
- `ha`：可选。可以填写 `url` 与 `token`；设备目前仅保存运行时配置。

## 实现和安全

- 官方 V2 SDMMC 单线连接：CMD=GPIO39、D0=GPIO40、CLK=GPIO41。
- ESP-IDF 将卡挂载到 `/sdcard`，读取 `/sdcard/config.json`。
- 使用 FAT32，禁止挂载失败后自动格式化；配置文件最大 4096 字节。
- TF 卡缺失或 JSON 错误不会启动旧 AP/HTTP/DNS 配网服务，UI 显示错误原因。
- 示例仅使用占位密码。**真实密码及 HA Token 不要提交 GitHub**；TF 卡中的明文配置也应妥善保管。
- 不实现 USB MSC / U 盘模拟。
