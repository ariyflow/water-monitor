# 蓝牙配网（BLE Provisioning）说明

本文档说明如何通过蓝牙（BLE）把 WiFi 的 SSID/密码传给单片机，使其连接 WiFi。

## 1. 整体流程

```
+--------+    BLE 写入(特征 0xFFE1)    +-------------+    异步     +---------+
|  手机   | ------------------------> |  WM-BLE 从机 | ---------> |  esp_wifi |
+--------+   [len][len][SSID][PASS]  +-------------+             |  STA 连接 |
                                                                 +---------+
```

1. 开机默认**蓝牙关闭**（不广播）。
2. 按 **KEY_1（SW2，IO45）** 打开蓝牙：LED7（指示位最高位 0x80）点亮，设备开始广播 "WM-BLE"；再次按 KEY_1 关闭蓝牙。
3. 手机用支持 GATT 写的 App（如 nRF Connect、蓝牙串口助手）连接 "WM-BLE"，找到服务 **0xFFE0** 下的写特征 **0xFFE1**，写入配网数据。
4. 单片机解析出 SSID/密码后调用 `wifi_sta_connect()`，凭据保存到 NVS（下次开机自动重连）。
5. 连接成功（获取到 IP）后串口打印 `got ip: x.x.x.x`。

## 2. BLE 数据格式（长度前缀二进制）

载荷为**原始字节**，格式：

```
| 字节0     | 字节1     | 字节2 .. 2+ssid_len-1 | 之后 pass_len 字节     |
| ssid_len  | pass_len  | SSID 字节             | 密码字节               |
```

- `ssid_len`：SSID 长度，1 字节，范围 1~32（WiFi 标准上限）。
- `pass_len`：密码长度，1 字节，范围 0~64。
- 总长度必须满足：`2 + ssid_len + pass_len == len`。
- 采用原始字节而非文本行，天然支持中文 SSID 与特殊字符。

### 示例

SSID = `REDMI Turbo 5 Max`（17 字节），密码 = `6d4rbe2c96xd5z6`（15 字节）：

```
hex 序列（34 字节）:
11 0F 52 45 44 4D 49 20 54 75 72 62 6F 20 35 20 4D 61 78
     36 64 34 72 62 65 32 63 39 36 78 64 35 7A 36
   ^  ^
   |  └─ pass_len = 0x0F (15)
   └──── ssid_len = 0x11 (17)

连续 hex 字符串（用于 App 的 hex 发送模式）:
110F5245444D4920547572626F2035204D6178366434726265326339367864357A36
```

### 校验规则（单片机侧）

- `len < 2`：数据太短，忽略。
- `2 + ssid_len + pass_len != len`：长度不匹配，忽略。
- `ssid_len == 0 || ssid_len > 32 || pass_len > 64`：非法长度，忽略。

## 3. 相关代码

| 文件 | 职责 |
|------|------|
| `main/src/ble.c` / `main/include/ble.h` | NimBLE GATT 从机、收发、开/关广播（KEY_1 控制） |
| `main/src/wifi_sta.c` / `main/include/wifi_sta.h` | WiFi STA 初始化、连接、NVS 持久化、自动重连 |
| `main/main.c` | 按键扫描、`on_ble_data()` 解析配网载荷并调用 `wifi_sta_connect()` |

### 关键接口

- `wm_ble_start()` / `wm_ble_stop()`：开/关蓝牙广播（经命令队列交给独立任务执行，避免占用控制任务栈）。
- `wifi_sta_init()`：初始化 WiFi STA；若 NVS 已有凭据则自动连接。
- `wifi_sta_connect(ssid, password)`：下发配置并异步连接，同时保存凭据到 NVS。
- `wifi_sta_is_connected()`：查询是否已获取 IP。

## 4. WiFi 连接行为

- 连接为**异步**，结果由事件回调上报，不阻塞调用方任务。
- 断开后自动重试最多 5 次（间隔 2 秒）。
- 凭据保存在 NVS（namespace `wifi_cfg`，key `ssid` / `pass`），开机自动重连，无需重复配网。

## 5. 验证步骤

1. 烧录固件（分区表已改为 12MB factory 分区）。
2. 按 KEY_1 打开蓝牙，观察 LED7 点亮。
3. 手机 App 连接 "WM-BLE"，向 0xFFE1 写入第 2 节的载荷。
4. 串口日志确认：

```
I wifi_sta: connecting to REDMI Turbo 5 Max
I wifi_sta: connected to REDMI Turbo 5 Max, got ip: 192.168.x.x
```

5. 断电重启，设备应自动重连 WiFi（无需再次配网）。
