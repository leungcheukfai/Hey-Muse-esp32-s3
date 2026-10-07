# Muse Gadget 繁體中文擴充

這個專案以 Muse ESP32 Device SDK 為基礎，加入繁體中文介面、Muse 回覆語言選項，以及由 Gemini 直接產生的廣東話或普通話語音。它沿用上游裝置韌體架構；刷寫會更新裝置韌體，不是安裝手機 App 外掛。

## 使用流程

1. **編譯對應板型的韌體。** 目前先以 Waveshare ESP32-S3-Touch-AMOLED-1.75C 作為測試目標；另有 Waveshare 1.75 和 M5Stack CoreS3 的介面設定。這三種板型的映像不能互換。
2. **刷寫並完成 Muse 配對。** 使用自己的 Muse 帳戶和 SDK token。SDK token 與 Gemini API 金鑰是兩種不同憑證；本專案不附帶作者的 token。上游 SDK 目前要求在編譯時設定自己的 `mgst_…` SDK token，尚未提供刷機後輸入此 token 的流程。
3. **用手機設定 Wi-Fi 和 Gemini 金鑰。** 在 Muse 裝置開啟「設定 > 藍牙 > 手機設定」，再以支援 Web Bluetooth 的 Chrome 開啟 [`esp32/tools/muse/ble_setup.html`](esp32/tools/muse/ble_setup.html)。連線並完成畫面上的配對驗證後，在「Gemini 語音」區貼上自己的 Gemini API 金鑰。小螢幕只需輸入 Wi-Fi 密碼和金鑰，不必用裝置上的按鍵逐字輸入。金鑰只會傳到裝置並存入 NVS；裝置透過 HTTPS 直接呼叫 Google Gemini，不經本專案的伺服器。
4. **選擇回覆語言。** 在裝置「設定 > 語言與語音」選擇「廣東話」或「普通話」。畫面字幕維持繁體中文；Gemini 依選擇朗讀 Muse 的文字回覆。清除金鑰後仍可使用字幕。
5. **分開驗證收音和語音輸出。** 先確認 Muse 能收到語音並回覆文字，再單獨確認 Gemini 語音，最後測試完整對話。Gemini 的廣東話 TTS 不代表裝置的廣東話語音辨識已改用 Gemini：目前收音、轉錄仍走 Muse 原有的 dictation 路徑，需另外測試它對粵語的辨識效果。

## 憑證與裝置安全

- 每位使用者使用自己的 Muse SDK token 和 Gemini API 金鑰；不要把金鑰提交到 Git、放進公開韌體，或貼到聊天訊息。
- Gemini 金鑰透過完成驗證的 BLE 設定連線傳送，保存於 NVS，呼叫 Gemini 時以 HTTPS 傳送。1.75C 測試板型已啟用加密 NVS；此設定會在第一次啟動時使用 eFuse 的 HMAC 金鑰區塊 0。寫入 eFuse 是不可逆的。刷寫前應確認該區塊未被既有設定佔用；之後也要維持相同的 NVS 加密設定，否則韌體可能無法讀取已加密的設定。
- 1.75 和 CoreS3 設定目前未啟用 NVS 加密。在為這兩種板型啟用加密以前，不要在其中儲存 Gemini 金鑰。
- Gemini 金鑰不會由韌體回傳，也不會寫入連線記錄。清除金鑰可在手機設定頁操作。

## 建置

韌體目前使用 ESP-IDF 6.0.1。從 [`esp32/AGENTS.md`](esp32/AGENTS.md) 查看完整依賴與建置說明；三個 UI 板型分別是 `s3`、`s3n` 和 `cores3`。例如先建置 Waveshare 1.75C：

```sh
cd esp32
tools/muse/board.sh build s3
```

設定自己的 Muse SDK token 時，請用本機 `sdkconfig` 或 `menuconfig`，不要提交產生的設定檔。此版本的測試韌體啟用了 1.75C 的 NVS 加密設定。燒錄前必須再讀取板上的 eFuse 狀態，確認金鑰區塊 0 可安全使用；本專案不會在未檢查實體板的情況下自動燒錄。

## 目前測試範圍

目前可在主機執行語言表測試並建置 1.75C 韌體。實際顯示效果、揚聲器音質、Gemini API 呼叫，以及 Muse 對粵語收音的辨識效果仍需連接實體板後驗證。
