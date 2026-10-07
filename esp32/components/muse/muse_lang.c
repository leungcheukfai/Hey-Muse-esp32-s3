/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "muse_lang.h"

#include <string.h>

typedef struct {
    const char *english;
    const char *traditional_chinese;
} entry_t;

static const entry_t STRINGS[] = {
    { "WAKING UP", "正在啟動" },
    { "READY", "就緒" },
    { "LISTENING", "聆聽中" },
    { "THINKING", "思考中" },
    { "SPEAKING", "回覆中" },
    { "ERROR", "錯誤" },
    { "GOODBYE", "再見" },
    { "TAP TO TAKE PHOTO", "點按拍照" },
    { "Pairing code", "配對碼" },
    { "Enter on phone", "在手機輸入" },
    { "Enter it on your phone", "在手機輸入此配對碼" },
    { "SETTINGS", "設定" },
    { "Settings", "設定" },
    { "Wi-Fi", "Wi-Fi" },
    { "Muse", "Muse" },
    { "Bluetooth", "藍牙" },
    { "Sound", "音效" },
    { "Sleep", "休眠" },
    { "Battery", "電池" },
    { "Power off", "關機" },
    { "Volume", "音量" },
    { "Speaker", "揚聲器" },
    { "Brightness", "螢幕亮度" },
    { "Mic gain", "麥克風增益" },
    { "Auto-sleep", "自動休眠" },
    { "Phone setup", "手機設定" },
    { "Status", "狀態" },
    { "Reset pairing", "重設配對" },
    { "Screen off", "關閉螢幕" },
    { "Close menu", "關閉選單" },
    { "Change", "調整" },
    { "Toggle", "切換" },
    { "Open", "開啟" },
    { "Select", "選擇" },
    { "Close", "關閉" },
    { "On", "開啟" },
    { "Off", "關閉" },
    { "Never", "永不" },
    { "30 seconds", "30 秒" },
    { "1 minute", "1 分鐘" },
    { "2 minutes", "2 分鐘" },
    { "5 minutes", "5 分鐘" },
    { "10 minutes", "10 分鐘" },
    { "WIFI", "Wi-Fi" },
    { "MUSE", "Muse 帳戶" },
    { "BLUETOOTH", "藍牙" },
    { "SOUND", "音效" },
    { "SLEEP", "休眠" },
    { "BATTERY", "電池" },
    { "POWER", "電源" },
    { "Network name", "網絡名稱" },
    { "Password", "密碼" },
    { "Join", "連線" },
    { "Forget all networks", "忘記所有網絡" },
    { "Scanning...", "正在掃描…" },
    { "Scan for networks", "掃描 Wi-Fi 網絡" },
    { "Not set", "尚未設定" },
    { "Joining", "連線中" },
    { "Failed", "失敗" },
    { "Not found", "找不到網絡" },
    { "Not nearby", "附近找不到網絡" },
    { "Server", "伺服器" },
    { "VM ID", "VM ID" },
    { "Device token", "裝置 Token" },
    { "Test connection", "測試連線" },
    { "Pair with the Muse app to use your account; a device token here overrides it, and a long one is easier to send over Bluetooth. The VM ID picks one of your VMs. Reset pairing forgets Wi-Fi and the app pairing, then restarts.",
      "使用 Muse App 配對即可連結帳戶；在此輸入的裝置 Token 會取代 App 配對。較長的 Token 可透過藍牙傳送。VM ID 用來選擇虛擬機。重設配對會清除 Wi-Fi 與 App 配對，然後重新啟動。" },
    { "Forget paired phones", "忘記已配對手機" },
    { "When on, Muse is visible to phones nearby. Open tools/ble_setup.html in Chrome, connect, and enter the code Muse shows to pair.",
      "開啟後，附近手機可找到 Muse。請用 Chrome 開啟 tools/ble_setup.html 並連線，再輸入 Muse 螢幕上的配對碼。" },
    { "Mic level", "麥克風音量" },
    { "Muted", "已靜音" },
    { "Language & speech", "語言與語音" },
    { "LANGUAGE & SPEECH", "語言與語音" },
    { "Language", "語言" },
    { "Cantonese", "廣東話" },
    { "Mandarin", "普通話" },
    { "Reply voice", "回覆語音" },
    { "Cantonese voice", "廣東話語音" },
    { "Mandarin voice", "普通話語音" },
    { "Selected", "已選擇" },
    { "Traditional Chinese captions stay on screen. Choose the language Muse speaks in.",
      "螢幕字幕會維持繁體中文。請選擇 Muse 的回覆語音。" },
    { "Google Gemini API key", "Google Gemini API 金鑰" },
    { "Gemini TTS: key saved", "Gemini 語音：金鑰已儲存" },
    { "Gemini TTS: not set", "Gemini 語音：尚未設定金鑰" },
    { "Enter your key in Phone setup on the phone page.",
      "請在手機的 Phone setup 頁面輸入自己的 API 金鑰。" },
    { "With no Gemini key, Muse shows replies as captions without speech.",
      "尚未設定 Gemini 金鑰時，Muse 只會顯示字幕，不會朗讀。" },
    { "Connected", "已連線" },
    { "Not paired", "尚未配對" },
    { "Not set up", "尚未設定" },
    { "Offline", "離線" },
    { "Saved", "已儲存" },
    { "Connecting", "連線中" },
    { "Can't connect", "無法連線" },
    { "Pair in the Muse app", "請在 Muse App 配對" },
    { "Waiting for Wi-Fi", "等待 Wi-Fi" },
    { "Connects when you talk", "開始對話時連線" },
    { "Cancel", "取消" },
    { "Power Muse off completely?", "要完全關閉 Muse 嗎？" },
    { "Press the %s button to turn it back on. To just turn the screen off, press the %s button.",
      "按下 %s 按鈕重新開機。若只想關閉螢幕，請按下 %s 按鈕。" },
    { "No battery", "沒有電池" },
    { "Unplug USB to start measuring.", "拔除 USB 後開始測量。" },
    { "WI-FI OFF", "Wi-Fi 已關閉" },
    { "SET UP WI-FI", "請設定 Wi-Fi" },
    { "NO WI-FI", "找不到 Wi-Fi" },
    { "RECONNECTING", "重新連線中" },
    { "CONNECTING", "連線中" },
    { "CHARGING", "充電中" },
    { "Battery level", "電量" },
    { "CHARGING %d%%", "充電中 %d%%" },
    { "BATTERY %d%%", "電量 %d%%" },
    { "WAKING UP...", "正在啟動…" },
    { "CAN'T REACH MUSE", "無法連接 Muse" },
    { "MUSE NOT SET UP", "尚未設定 Muse" },
    { "DIDN'T CATCH THAT", "沒有聽清楚" },
    { "MUSE STOPPED LISTENING", "Muse 停止聆聽" },
    { "MUSE COULDN'T LISTEN", "Muse 無法聆聽" },
    { "SAY HEY MUSE", "請說 Hey Muse" },
    { "PRESS TALK", "請按下通話按鈕" },
    { "measuring", "測量中" },
    { "Also kept awake by: %s", "仍在運作：%s" },
    { "Enter", "確認" },
    { "Volume %d%%", "音量 %d%%" },
    { "Vol %d%%", "音量 %d%%" },
    { "USB POWER", "USB 供電" },
    { "USB", "USB" },
};

const char *muse_lang_get(const char *english)
{
    if (!english) {
        return "";
    }
    for (size_t i = 0; i < sizeof(STRINGS) / sizeof(STRINGS[0]); i++) {
        if (strcmp(english, STRINGS[i].english) == 0) {
            return STRINGS[i].traditional_chinese;
        }
    }
    return english;
}

const char *muse_lang_chat_instruction(int cantonese)
{
    return cantonese
        ? "Reply in Traditional Chinese using natural Hong Kong written Cantonese. Keep names and technical terms clear."
        : "Reply in Traditional Chinese using natural Mandarin wording. Keep names and technical terms clear.";
}

const char *muse_lang_tts_style(int cantonese)
{
    return cantonese
        ? "Speak naturally in Hong Kong Cantonese (廣東話), with Cantonese pronunciation. Read only the provided reply text."
        : "Speak naturally in Mandarin Chinese (普通話), with standard Mandarin pronunciation. Read only the provided reply text.";
}
