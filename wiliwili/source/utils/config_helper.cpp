//
// Created by fang on 2022/7/10.
//

#ifdef IOS
#include <CoreFoundation/CoreFoundation.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <unistd.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#if defined(_WIN32)
#include <shlobj.h>
#endif
#endif

#include <cmath>
#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/touch/pan_gesture.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <cpr/filesystem.h>

#include "bilibili.h"
#include "utils/number_helper.hpp"
#include "utils/thread_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/crash_helper.hpp"
#include "utils/vibration_helper.hpp"
#include "utils/ban_list.hpp"
#include "utils/string_helper.hpp"
#include "presenter/video_detail.hpp"
#include "activity/player_activity.hpp"
#include "activity/search_activity_tv.hpp"
#include "view/danmaku_core.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"

#ifdef PS4
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <arpa/inet.h>

extern "C" {
extern int ps4_mpv_use_precompiled_shaders;
extern int ps4_mpv_dump_shaders;
extern in_addr_t primary_dns;
extern in_addr_t secondary_dns;
}
#endif

#ifdef __PSV__
#include <mbedtls/platform.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/vshbridge.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
extern "C"
{
unsigned int _newlib_heap_size_user      = 220 * 1024 * 1024;
unsigned int _pthread_stack_default_user = 2 * 1024 * 1024;
#ifndef BOREALIS_USE_GXM
unsigned int sceLibcHeapSize             = 24 * 1024 * 1024;
#endif
}
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#ifdef __PSV__
#ifdef BOREALIS_USE_GXM
// 720P
#define WILI_VIDEO_QUALITY_DEFAULT 64
#define WILI_VIDEO_QUALITY_LANDSCAPE_MAX 64
// 480P
#define WILI_VIDEO_QUALITY_PORTRAIT_MAX 32
#else
#define WILI_VIDEO_QUALITY_DEFAULT 32
#define WILI_VIDEO_QUALITY_LANDSCAPE_MAX 32
#define WILI_VIDEO_QUALITY_PORTRAIT_MAX 32
#endif
#define WILI_WINDOW_WIDTH_DEFAULT 960
#define WILI_WINDOW_HEIGHT_DEFAULT 544
// 默认 UI 缩放 (0 为 960x544)
#define WILI_UI_SCALE_DEFAULT 0
// 默认音频质量 (2 为 低, PSV 的喇叭质量差，音质高低无区别，设置成低可以减少流量)
#define WILI_AUDIO_QUALITY_DEFAULT 2
#define WILI_DNS_CACHE_TIMEOUT 3600000
#else
// 默认清晰度 (116 为 1080P@60)
#define WILI_VIDEO_QUALITY_DEFAULT 116
// 横屏视频最高清晰度 (127 为 8K, 128 即无限制)
#define WILI_VIDEO_QUALITY_LANDSCAPE_MAX 128
// 竖屏视频最高清晰度
#define WILI_VIDEO_QUALITY_PORTRAIT_MAX 128
// 默认窗口大小 (不配置 ui 缩放时的窗口大小)
#define WILI_WINDOW_WIDTH_DEFAULT 1280
#define WILI_WINDOW_HEIGHT_DEFAULT 720
// 默认 UI 缩放 (1 为 1280x720)
#define WILI_UI_SCALE_DEFAULT 1
// 默认音频质量 (0 为 高)
#define WILI_AUDIO_QUALITY_DEFAULT 0
// DNS 缓存时间
#define WILI_DNS_CACHE_TIMEOUT 60000
#endif

using namespace brls::literals;

std::unordered_map<SettingItem, ProgramOption> ProgramConfig::SETTING_MAP = {
    /// string
    {SettingItem::CUSTOM_UPDATE_API, {"custom_update_api", {}, {}, 0}},
    {SettingItem::APP_LANG,
     {"app_lang",
      {
#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
          brls::LOCALE_AUTO,
#endif
          brls::LOCALE_EN_US,
          brls::LOCALE_JA,
          brls::LOCALE_RYU,
          brls::LOCALE_ZH_HANT,
          brls::LOCALE_ZH_HANS,
          brls::LOCALE_Ko,
          brls::LOCALE_IT,
      },
      {},
#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
      0}},
#else
      4}},
#endif
    {SettingItem::APP_THEME, {"app_theme", {"auto", "light", "dark"}, {}, 0}},
    {SettingItem::APP_RESOURCES, {"app_resources", {}, {}, 0}},
    {SettingItem::APP_UI_SCALE, {"app_ui_scale", {"544p", "720p", "900p", "1080p"}, {}, WILI_UI_SCALE_DEFAULT}},
    {SettingItem::KEYMAP, {"keymap", {"xbox", "ps", "keyboard"}, {}, 0}},
    {SettingItem::HOME_WINDOW_STATE, {"home_window_state", {}, {}, 0}},
    {SettingItem::DLNA_IP, {"dlna_ip", {}, {}, 0}},
    {SettingItem::DLNA_NAME, {"dlna_name", {}, {}, 0}},
    {SettingItem::PLAYER_ASPECT, {"player_aspect", {"-1", "-2", "-3", "4:3", "16:9"}, {}, 0}},
    {SettingItem::HTTP_PROXY, {"http_proxy", {}, {}, 0}},
    {SettingItem::DANMAKU_STYLE_FONT, {"danmaku_style_font", {"stroke", "incline", "shadow", "pure"}, {}, 0}},

    /// bool
    {SettingItem::APP_SWAP_ABXY, {"app_swap_abxy", {}, {}, 0}},
    {SettingItem::GAMEPAD_VIBRATION, {"gamepad_vibration", {}, {}, 1}},
#if defined(IOS) || defined(__PSV__)
    {SettingItem::HIDE_BOTTOM_BAR, {"hide_bottom_bar", {}, {}, 1}},
#else
    {SettingItem::HIDE_BOTTOM_BAR, {"hide_bottom_bar", {}, {}, 0}},
#endif
    {SettingItem::HIDE_FPS, {"hide_fps", {}, {}, 1}},
#if defined(__APPLE__) || !defined(NDEBUG)
    // mac使用原生全屏按钮效果更好，不通过软件来控制
    // win32 debug 模式不全屏，调试时会挡住 vs
    {SettingItem::FULLSCREEN, {"fullscreen", {}, {}, 0}},
#else
    // release 下默认全屏
    {SettingItem::FULLSCREEN, {"fullscreen", {}, {}, 1}},
#endif
    {SettingItem::HISTORY_REPORT, {"history_report", {}, {}, 1}},
    {SettingItem::PLAYER_AUTO_PLAY, {"player_auto_play", {}, {}, 1}},
    {SettingItem::PLAYER_BOTTOM_BAR, {"player_bottom_bar", {}, {}, 1}},
    {SettingItem::PLAYER_HIGHLIGHT_BAR, {"player_highlight_bar", {}, {}, 0}},
    {SettingItem::PLAYER_SKIP_OPENING_CREDITS, {"player_skip_opening_credits", {}, {}, 1}},
    {SettingItem::PLAYER_LOW_QUALITY, {"player_low_quality", {}, {}, 1}},
#if defined(IOS) || defined(__PSV__) || defined(__SWITCH__)
    {SettingItem::PLAYER_HWDEC, {"player_hwdec", {}, {}, 1}},
#else
    {SettingItem::PLAYER_HWDEC, {"player_hwdec", {}, {}, 0}},
#endif
    {SettingItem::PLAYER_HWDEC_CUSTOM, {"player_hwdec_custom", {}, {}, 0}},
    {SettingItem::PLAYER_EXIT_FULLSCREEN_ON_END, {"player_exit_fullscreen_on_end", {}, {}, 1}},
    {SettingItem::PLAYER_OSD_TV_MODE, {"player_osd_tv_mode", {}, {}, 0}},
    {SettingItem::OPENCC_ON, {"opencc", {}, {}, 1}},
    {SettingItem::DANMAKU_ON, {"danmaku", {}, {}, 1}},
    {SettingItem::DANMAKU_FILTER_BOTTOM, {"danmaku_filter_bottom", {}, {}, 1}},
    {SettingItem::DANMAKU_FILTER_TOP, {"danmaku_filter_top", {}, {}, 1}},
    {SettingItem::DANMAKU_FILTER_SCROLL, {"danmaku_filter_scroll", {}, {}, 1}},
    {SettingItem::DANMAKU_FILTER_COLOR, {"danmaku_filter_color", {}, {}, 1}},
    {SettingItem::DANMAKU_FILTER_ADVANCED, {"danmaku_filter_advanced", {}, {}, 0}},
    {SettingItem::DANMAKU_SMART_MASK, {"danmaku_smart_mask", {}, {}, 1}},
    {SettingItem::SEARCH_TV_MODE, {"search_tv_mode", {}, {}, 1}},
    {SettingItem::HTTP_PROXY_STATUS, {"http_proxy_status", {}, {}, 0}},
    {SettingItem::TLS_VERIFY,
     {"tls_verify",
      {},
      {},
#if defined(__PSV__) || defined(__SWITCH__) || defined(PS4)
      0}},
#else
      1}},
#endif

/// number
#if defined(__PSV__)
    {SettingItem::PLAYER_INMEMORY_CACHE, {"player_inmemory_cache", {"0MB", "1MB", "5MB", "10MB"}, {0, 1, 5, 10}, 0}},
#elif defined(__SWITCH__)
    {SettingItem::PLAYER_INMEMORY_CACHE,
     {"player_inmemory_cache", {"0MB", "10MB", "20MB", "50MB", "100MB"}, {0, 10, 20, 50, 100}, 0}},
#else
    {SettingItem::PLAYER_INMEMORY_CACHE,
     {"player_inmemory_cache", {"0MB", "10MB", "20MB", "50MB", "100MB"}, {0, 10, 20, 50, 100}, 1}},
#endif
    {
        SettingItem::PLAYER_DEFAULT_SPEED,
        {"player_default_speed",
         {"4.0x", "3.0x", "2.0x", "1.75x", "1.5x", "1.25x", "1.0x", "0.75x", "0.5x", "0.25x"},
         {400, 300, 200, 175, 150, 125, 100, 75, 50, 25},
         2},
    },
    {SettingItem::PLAYER_VOLUME, {"player_volume", {}, {}, 0}},
    {SettingItem::TEXTURE_CACHE_NUM, {"texture_cache_num", {}, {}, 0}},
    {SettingItem::VIDEO_QUALITY, {"video_quality", {}, {}, 116}},
    {SettingItem::VIDEO_QUALITY_LANDSCAPE_MAX, {"video_quality_landscape_max", {}, {}, 128}},
    {SettingItem::VIDEO_QUALITY_PORTRAIT_MAX, {"video_quality_portrait_max", {}, {}, 128}},
    {SettingItem::IMAGE_REQUEST_THREADS,
     {"image_request_threads",
#if defined(__SWITCH__) || defined(__PSV__)
      {"1", "2", "3", "4"},
      {1, 2, 3, 4},
      1}},
#else
      {"1", "2", "3", "4", "8", "12", "16"},
      {1, 2, 3, 4, 8, 12, 16},
      3}},
#endif
    {SettingItem::VIDEO_FORMAT, {"file_format", {"Dash (AVC/HEVC/AV1)", "FLV/MP4"}, {4048, 0}, 0}},
    {SettingItem::VIDEO_CODEC, {"video_codec", {"AVC/H.264", "HEVC/H.265", "AV1"}, {7, 12, 13}, 0}},
    {SettingItem::AUDIO_QUALITY,
     {"audio_quality", {"High", "Medium", "Low"}, {30280, 30232, 30216}, WILI_AUDIO_QUALITY_DEFAULT}},
    {SettingItem::DANMAKU_FILTER_LEVEL,
     {"danmaku_filter_level", {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 0}},
    {SettingItem::DANMAKU_STYLE_AREA, {"danmaku_style_area", {"1/4", "1/2", "3/4", "1"}, {25, 50, 75, 100}, 3}},
    {SettingItem::DANMAKU_STYLE_ALPHA,
     {"danmaku_style_alpha",
      {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"},
      {10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
      7}},
    {SettingItem::DANMAKU_STYLE_FONTSIZE,
     {"danmaku_style_fontsize", {"50%", "75%", "100%", "125%", "150%", "175%"}, {15, 22, 30, 37, 45, 50}, 2}},
    {SettingItem::DANMAKU_STYLE_LINE_HEIGHT,
     {"danmaku_style_line_height",
      {"100%", "120%", "140%", "160%", "180%", "200%"},
      {100, 120, 140, 160, 180, 200},
      1}},
    {SettingItem::DANMAKU_STYLE_SPEED,
     {"danmaku_style_speed", {"0.5", "0.75", "1.0", "1.25", "1.5"}, {150, 125, 100, 75, 50}, 2}},
    {SettingItem::DANMAKU_RENDER_QUALITY,
     {"danmaku_render_quality", {"100%", "95%", "90%", "80%", "70%", "60%", "50%"}, {100, 95, 90, 80, 70, 60, 50}, 0}},
    {SettingItem::LIMITED_FPS, {"limited_fps", {"0", "30", "60", "90", "120"}, {0, 30, 60, 90, 120}, 0}},
    {SettingItem::SWAP_INTERVAL, {"swap_interval", {"0", "1", "2", "3", "4"}, {0, 1, 2, 3, 4}, 1}},
    {SettingItem::DEACTIVATED_TIME, {"deactivated_time", {}, {}, 0}},
    {SettingItem::DEACTIVATED_FPS, {"deactivated_fps", {}, {}, 0}},
    {SettingItem::DLNA_PORT, {"dlna_port", {}, {}, 0}},
    {SettingItem::PLAYER_STRATEGY, {"player_strategy", {"rcmd", "next", "loop", "single"}, {0, 1, 2, 3}, 0}},
    {SettingItem::PLAYER_BRIGHTNESS, {"player_brightness", {}, {}, 0}},
    {SettingItem::PLAYER_CONTRAST, {"player_contrast", {}, {}, 0}},
    {SettingItem::PLAYER_SATURATION, {"player_saturation", {}, {}, 0}},
    {SettingItem::PLAYER_HUE, {"player_hue", {}, {}, 0}},
    {SettingItem::PLAYER_GAMMA, {"player_gamma", {}, {}, 0}},
    {SettingItem::PLAYER_OSD_HIDE, {"player_osd_hide", {}, {}, 0}},
    {SettingItem::MINIMUM_WINDOW_WIDTH, {"minimum_window_width", {"480"}, {480}, 0}},
    {SettingItem::MINIMUM_WINDOW_HEIGHT, {"minimum_window_height", {"270"}, {270}, 0}},
    {SettingItem::ON_TOP_WINDOW_WIDTH, {"on_top_window_width", {"480"}, {480}, 0}},
    {SettingItem::ON_TOP_WINDOW_HEIGHT, {"on_top_window_height", {"270"}, {270}, 0}},
    {SettingItem::ON_TOP_MODE, {"on_top_mode", {"off", "always", "auto"}, {0, 1, 2}, 0}},
    {SettingItem::SCROLL_SPEED, {"scroll_speed", {}, {}, 0}},
    {SettingItem::HTTP_TIMEOUT, {"http_timeout", {}, {}, 0}},
    {SettingItem::HTTP_CONNECTION_TIMEOUT, {"http_connection_timeout", {}, {}, 0}},
    {SettingItem::HTTP_DNS_CACHE_TIMEOUT, {"http_dns_cache_timeout", {}, {}, 0}},

    /// Custom
    {SettingItem::UP_FILTER, {"up_filter", {}, {}, 0}},
};

ProgramConfig::ProgramConfig() = default;

ProgramConfig::ProgramConfig(const ProgramConfig& conf) {
    this->cookie        = conf.cookie;
    this->setting       = conf.setting;
    this->device        = conf.device;
    this->client        = conf.client;
    this->refreshToken  = conf.refreshToken;
    this->searchHistory = conf.searchHistory;
    this->seasonCustom  = conf.seasonCustom;
}

void ProgramConfig::setProgramConfig(const ProgramConfig& conf) {
    this->cookie        = conf.cookie;
    this->setting       = conf.setting;
    this->client        = conf.client;
    this->device        = conf.device;
    this->refreshToken  = conf.refreshToken;
    this->searchHistory = conf.searchHistory;
    this->seasonCustom  = conf.seasonCustom;
    brls::Logger::info("setting: {}", conf.setting.dump());
}

void ProgramConfig::setCookie(const Cookie& data) {
    this->cookie = data;
    if (data.empty()) this->refreshToken.clear();
    this->save();
}

Cookie ProgramConfig::getCookie() const { return this->cookie; }

void ProgramConfig::addHistory(const std::string& key) {
    if (key.empty()) return;
    std::string newItem = wiliwili::base64Encode(key);
    auto it             = this->searchHistory.begin();
    for (; it != this->searchHistory.end(); it++) {
        if (*it == newItem) break;
    }
    if (it != this->searchHistory.end()) {
        this->searchHistory.erase(it);
    }
    if (this->searchHistory.size() == 50) {
        this->searchHistory.erase(this->searchHistory.begin());
    }
    this->searchHistory.emplace_back(newItem);
    this->save();
}

std::vector<std::string> ProgramConfig::getHistoryList() {
    std::vector<std::string> res;
    for (auto it = this->searchHistory.rbegin(); it != this->searchHistory.rend(); it++) {
        std::string out;
        if (wiliwili::base64Decode(*it, out) == 0) {
            res.emplace_back(out);
        } else {
            res.emplace_back(*it);
        }
    }
    return res;
}

void ProgramConfig::setHistory(const std::vector<std::string>& list) {
    this->searchHistory.clear();
    for (auto& i : list) {
        this->searchHistory.emplace_back(wiliwili::base64Encode(i));
    }
    this->save();
}

void ProgramConfig::setRefreshToken(const std::string& token) {
    this->refreshToken = token;
    this->save();
}

std::string ProgramConfig::getRefreshToken() const { return this->refreshToken; }

std::string ProgramConfig::getCSRF() {
    if (this->cookie.count("bili_jct") == 0) {
        return "";
    }
    return this->cookie["bili_jct"];
}

std::string ProgramConfig::getUserID() {
    if (this->cookie.count("DedeUserID") == 0) {
        return "";
    }
    return this->cookie["DedeUserID"];
}

std::string ProgramConfig::getBuvid3() {
    if (this->cookie.count("buvid3") == 0) {
        return "";
    }
    return this->cookie["buvid3"];
}

bool ProgramConfig::hasLoginInfo() { return !getUserID().empty() && (getUserID() != "0") && !getCSRF().empty(); }

std::string ProgramConfig::getClientID() {
    if (this->client.empty()) {
        this->client = fmt::format("{}.{}", wiliwili::getRandomNumber(), wiliwili::getUnixTime());
        this->save();
    }
    return this->client;
}

std::string ProgramConfig::getDeviceID() {
    if (this->device.empty()) {
        this->device = fmt::format("{}-{}-{}-{}-{}", wiliwili::getRandomHex(8), wiliwili::getRandomHex(4),
                                   wiliwili::getRandomHex(4), wiliwili::getRandomHex(4), wiliwili::getRandomHex(12));
        this->save();
    }
    return this->device;
}

void ProgramConfig::loadHomeWindowState() {
    std::string homeWindowStateData = getSettingItem(SettingItem::HOME_WINDOW_STATE, std::string{""});

    if (homeWindowStateData.empty()) return;

    uint32_t hWidth, hHeight;
    int hXPos, hYPos;
    int monitor;

    sscanf(homeWindowStateData.c_str(), "%d,%ux%u,%dx%d", &monitor, &hWidth, &hHeight, &hXPos, &hYPos);

    if (hWidth == 0 || hHeight == 0) return;

    uint32_t minWidth  = getIntOption(SettingItem::MINIMUM_WINDOW_WIDTH);
    uint32_t minHeight = getIntOption(SettingItem::MINIMUM_WINDOW_HEIGHT);
    if (hWidth < minWidth) hWidth = minWidth;
    if (hHeight < minHeight) hHeight = minHeight;

    VideoContext::sizeH        = hHeight;
    VideoContext::sizeW        = hWidth;
    VideoContext::posX         = (float)hXPos;
    VideoContext::posY         = (float)hYPos;
    VideoContext::monitorIndex = monitor;

    brls::Logger::info("Load window state: {}x{},{}x{}", hWidth, hHeight, hXPos, hYPos);
}

void ProgramConfig::saveHomeWindowState() {
    if (std::isnan(VideoContext::posX) || std::isnan(VideoContext::posY)) return;
    if (VideoContext::FULLSCREEN) return;
    auto videoContext = brls::Application::getPlatform()->getVideoContext();

    uint32_t width  = VideoContext::sizeW;
    uint32_t height = VideoContext::sizeH;
    int xPos        = VideoContext::posX;
    int yPos        = VideoContext::posY;

    int monitor = videoContext->getCurrentMonitorIndex();
    if (width == 0) width = brls::Application::ORIGINAL_WINDOW_WIDTH;
    if (height == 0) height = brls::Application::ORIGINAL_WINDOW_HEIGHT;
    brls::Logger::info("Save window state: {},{}x{},{}x{}", monitor, width, height, xPos, yPos);
    setSettingItem(SettingItem::HOME_WINDOW_STATE, fmt::format("{},{}x{},{}x{}", monitor, width, height, xPos, yPos));
}

void ProgramConfig::load() {
    const std::string path = this->getConfigDir() + "/wiliwili_config.json";

    std::ifstream readFile(path);
    if (readFile) {
        try {
            nlohmann::json content;
            readFile >> content;
            readFile.close();
            this->setProgramConfig(content.get<ProgramConfig>());
        } catch (const std::exception& e) {
            brls::Logger::error("ProgramConfig::load: {}", e.what());
        }
        brls::Logger::info("Load config from: {}", path);
    }

    // 初始化代理
    // 默认加载环境变量
    const char* http_proxy  = getenv("http_proxy");
    const char* https_proxy = getenv("https_proxy");
    if (http_proxy) {
        this->httpProxy = http_proxy;
        brls::Logger::info("Load http proxy from env: {}", this->httpProxy);
    }
    if (https_proxy) {
        this->httpsProxy = https_proxy;
        brls::Logger::info("Load https proxy from env: {}", this->httpsProxy);
    }
    // 如果设置开启了自定义代理，则读取配置文件中的代理设置
    if (getBoolOption(SettingItem::HTTP_PROXY_STATUS)) {
        this->httpProxy  = getSettingItem(SettingItem::HTTP_PROXY, this->httpProxy);
        this->httpsProxy = getSettingItem(SettingItem::HTTP_PROXY, this->httpsProxy);
    }

    // 初始化自定义手柄按键映射
#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    brls::DesktopPlatform::GAMEPAD_DB = getConfigDir() + "/gamecontrollerdb.txt";
#endif

    // 初始化自定义布局
    std::string customThemeID = getSettingItem(SettingItem::APP_RESOURCES, std::string{""});
    if (!customThemeID.empty()) {
        for (auto& theme : customThemes) {
            if (theme.id == customThemeID) {
                brls::View::CUSTOM_RESOURCES_PATH = theme.path;
                break;
            }
        }
        if (brls::View::CUSTOM_RESOURCES_PATH.empty()) {
            brls::Logger::warning("Custom theme not found: {}", customThemeID);
        }
    }

    // 初始化 UI 缩放
    std::string UIScale = getSettingItem(SettingItem::APP_UI_SCALE, std::string{""});
    if (UIScale == "544p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 960;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 544;
    } else if (UIScale == "720p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1280;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 720;
    } else if (UIScale == "900p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1600;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 900;
    } else if (UIScale == "1080p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1920;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 1080;
    } else {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = WILI_WINDOW_WIDTH_DEFAULT;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = WILI_WINDOW_HEIGHT_DEFAULT;
    }

    // 初始化视频清晰度最高限制
    VideoDetail::landscapeQualityMax = getSettingItem(SettingItem::VIDEO_QUALITY_LANDSCAPE_MAX,
                                                      WILI_VIDEO_QUALITY_LANDSCAPE_MAX);
    VideoDetail::portraitQualityMax  = getSettingItem(SettingItem::VIDEO_QUALITY_PORTRAIT_MAX,
                                                      WILI_VIDEO_QUALITY_PORTRAIT_MAX);

    // 初始化视频清晰度
    VideoDetail::defaultQuality = getSettingItem(SettingItem::VIDEO_QUALITY,
                                                 WILI_VIDEO_QUALITY_DEFAULT);
    if (!hasLoginInfo()) {
        // 用户未登录时跟随官方将默认清晰度设置到 360P
        VideoDetail::defaultQuality = 16;
    }

    // 加载完成后自动播放
    MPVCore::AUTO_PLAY = getBoolOption(SettingItem::PLAYER_AUTO_PLAY);

    // 初始化默认的倍速设定
    MPVCore::VIDEO_SPEED = getIntOption(SettingItem::PLAYER_DEFAULT_SPEED);

    // 初始化视频比例
    MPVCore::VIDEO_ASPECT = getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"});

    // 初始化均衡器
    MPVCore::VIDEO_BRIGHTNESS = getSettingItem(SettingItem::PLAYER_BRIGHTNESS, 0);
    MPVCore::VIDEO_CONTRAST   = getSettingItem(SettingItem::PLAYER_CONTRAST, 0);
    MPVCore::VIDEO_SATURATION = getSettingItem(SettingItem::PLAYER_SATURATION, 0);
    MPVCore::VIDEO_HUE        = getSettingItem(SettingItem::PLAYER_HUE, 0);
    MPVCore::VIDEO_GAMMA      = getSettingItem(SettingItem::PLAYER_GAMMA, 0);

    // 初始化弹幕相关内容
    DanmakuCore::DANMAKU_ON                   = getBoolOption(SettingItem::DANMAKU_ON);
    DanmakuCore::DANMAKU_SMART_MASK           = getBoolOption(SettingItem::DANMAKU_SMART_MASK);
    DanmakuCore::DANMAKU_FILTER_SHOW_TOP      = getBoolOption(SettingItem::DANMAKU_FILTER_TOP);
    DanmakuCore::DANMAKU_FILTER_SHOW_BOTTOM   = getBoolOption(SettingItem::DANMAKU_FILTER_BOTTOM);
    DanmakuCore::DANMAKU_FILTER_SHOW_SCROLL   = getBoolOption(SettingItem::DANMAKU_FILTER_SCROLL);
    DanmakuCore::DANMAKU_FILTER_SHOW_COLOR    = getBoolOption(SettingItem::DANMAKU_FILTER_COLOR);
    DanmakuCore::DANMAKU_FILTER_SHOW_ADVANCED = getBoolOption(SettingItem::DANMAKU_FILTER_ADVANCED);
    DanmakuCore::DANMAKU_FILTER_LEVEL         = getIntOption(SettingItem::DANMAKU_FILTER_LEVEL);
    DanmakuCore::DANMAKU_STYLE_AREA           = getIntOption(SettingItem::DANMAKU_STYLE_AREA);
    DanmakuCore::DANMAKU_STYLE_ALPHA          = getIntOption(SettingItem::DANMAKU_STYLE_ALPHA);
    DanmakuCore::DANMAKU_STYLE_FONTSIZE       = getIntOption(SettingItem::DANMAKU_STYLE_FONTSIZE);
    DanmakuCore::DANMAKU_STYLE_LINE_HEIGHT    = getIntOption(SettingItem::DANMAKU_STYLE_LINE_HEIGHT);
    DanmakuCore::DANMAKU_STYLE_SPEED          = getIntOption(SettingItem::DANMAKU_STYLE_SPEED);
    DanmakuCore::DANMAKU_STYLE_FONT           = DanmakuFontStyle{getStringOptionIndex(SettingItem::DANMAKU_STYLE_FONT)};

    DanmakuCore::DANMAKU_RENDER_QUALITY = getIntOption(SettingItem::DANMAKU_RENDER_QUALITY);

    // 初始化是否支持手柄振动
    VibrationHelper::GAMEPAD_VIBRATION = getBoolOption(SettingItem::GAMEPAD_VIBRATION);

    // 初始化视频格式
    BILI::FNVAL         = std::to_string(getIntOption(SettingItem::VIDEO_FORMAT));
    BILI::VIDEO_CODEC   = getIntOption(SettingItem::VIDEO_CODEC);
    BILI::AUDIO_QUALITY = getIntOption(SettingItem::AUDIO_QUALITY);

    // 初始化搜索页样式
    TVSearchActivity::TV_MODE = getBoolOption(SettingItem::SEARCH_TV_MODE);

    // 初始化线程数
    ImageHelper::REQUEST_THREADS = getIntOption(SettingItem::IMAGE_REQUEST_THREADS);

    // 初始化底部栏
    brls::AppletFrame::HIDE_BOTTOM_BAR = getBoolOption(SettingItem::HIDE_BOTTOM_BAR);

    // 初始化FPS
    brls::Application::setFPSStatus(!getBoolOption(SettingItem::HIDE_FPS));

    // 初始化是否全屏，必须在创建窗口前设置此值
    VideoContext::FULLSCREEN = getBoolOption(SettingItem::FULLSCREEN);

    // 初始化是否上传历史记录
    VideoDetail::REPORT_HISTORY = getBoolOption(SettingItem::HISTORY_REPORT);

    // 初始化播放策略
    BasePlayerActivity::PLAYER_STRATEGY = getIntOption(SettingItem::PLAYER_STRATEGY);

    // 是否自动跳过片头片尾
    BasePlayerActivity::PLAYER_SKIP_OPENING_CREDITS = getBoolOption(SettingItem::PLAYER_SKIP_OPENING_CREDITS);

    // 初始化是否固定显示底部进度条
    VideoView::BOTTOM_BAR = getBoolOption(SettingItem::PLAYER_BOTTOM_BAR);

    // 初始化是否固定显示底部高能进度条
    VideoView::HIGHLIGHT_PROGRESS_BAR = getBoolOption(SettingItem::PLAYER_HIGHLIGHT_BAR);

    // 初始化是否使用硬件加速
#if defined(__PSV__) && defined(BOREALIS_USE_OPENGL)
    MPVCore::HARDWARE_DEC = true;
#else
    MPVCore::HARDWARE_DEC = getBoolOption(SettingItem::PLAYER_HWDEC);
#endif

    // 初始化自定义的硬件加速方案
    MPVCore::PLAYER_HWDEC_METHOD = getSettingItem(SettingItem::PLAYER_HWDEC_CUSTOM, MPVCore::PLAYER_HWDEC_METHOD);

    // 播放结束时自动退出全屏
    VideoView::EXIT_FULLSCREEN_ON_END = getBoolOption(SettingItem::PLAYER_EXIT_FULLSCREEN_ON_END);

    // 初始化播放器 OSD 自动隐藏时间
    VideoView::OSD_SHOW_TIME = getSettingItem(SettingItem::PLAYER_OSD_HIDE, 5000);

    // 初始化内存缓存大小
    MPVCore::INMEMORY_CACHE = getIntOption(SettingItem::PLAYER_INMEMORY_CACHE);

    // 初始化是否使用opencc自动转换简体
    brls::Label::OPENCC_ON = getBoolOption(SettingItem::OPENCC_ON);

    // 是否使用低质量解码
    MPVCore::LOW_QUALITY = getBoolOption(SettingItem::PLAYER_LOW_QUALITY);

    // 初始化滑动速度
#ifdef _WIN32
    int scrollSpeed = getSettingItem(SettingItem::SCROLL_SPEED, 150);
#else
    int scrollSpeed = getSettingItem(SettingItem::SCROLL_SPEED, 100);
#endif
    brls::PanGestureRecognizer::panFactor = scrollSpeed * 0.01f;

    // 初始化i18n
    std::set<std::string> i18nData{
        brls::LOCALE_AUTO,    brls::LOCALE_EN_US,   brls::LOCALE_JA, brls::LOCALE_RYU,
        brls::LOCALE_ZH_HANS, brls::LOCALE_ZH_HANT, brls::LOCALE_Ko, brls::LOCALE_IT,
    };
    std::string langData = getSettingItem(SettingItem::APP_LANG, brls::LOCALE_AUTO);

    if (langData != brls::LOCALE_AUTO && i18nData.count(langData)) {
        brls::Platform::APP_LOCALE_DEFAULT = langData;
    } else {
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_ZH_HANS;
#endif
    }
#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    // 初始化上一次窗口位置
    loadHomeWindowState();
#endif

    // 初始化FPS限制
    int limitedFPS = getSettingItem(SettingItem::LIMITED_FPS, 0);
    brls::Application::setLimitedFPS(limitedFPS);
    VideoContext::swapInterval = limitedFPS == 0 ? getSettingItem(SettingItem::SWAP_INTERVAL, 1) : 0;

    // 初始化进入闲置状态需要的时间 (ms);
    int deactivatedTime = getSettingItem(SettingItem::DEACTIVATED_TIME, 0);
    if (deactivatedTime > 0) {
        // Reduce FPS to a lower value after a period of inactivity
        brls::Application::setAutomaticDeactivation(true);
        brls::Application::setDeactivatedTime(deactivatedTime);
    }

    // 初始化闲置状态 FPS
    brls::Application::setDeactivatedFPS(getSettingItem(SettingItem::DEACTIVATED_FPS, 5));

    // 初始化一些在创建窗口之后才能初始化的内容
    brls::Application::getWindowCreationDoneEvent()->subscribe([this]() {
        // 是否交换按键
        if (getBoolOption(SettingItem::APP_SWAP_ABXY)) {
            // 对于 PSV/PS4 来说，初始化时会加载系统设置，可能在那时已经交换过按键
            // 所以这里需要读取 isSwapInputKeys 的值，而不是直接设置为 true
            brls::Application::setSwapInputKeys(!brls::Application::isSwapInputKeys());
        }

        // 初始化弹幕字体
        std::string danmakuFont = getConfigDir() + "/danmaku.ttf";
        // 只在应用模式下加载自定义字体 减少switch上的内存占用
        if (brls::Application::getPlatform()->isApplicationMode() && access(danmakuFont.c_str(), F_OK) != -1 &&
            brls::Application::loadFontFromFile("danmaku", danmakuFont)) {
            // 自定义弹幕字体
            int danmakuFontId = brls::Application::getFont("danmaku");
            nvgAddFallbackFontId(brls::Application::getNVGContext(), danmakuFontId,
                                 brls::Application::getDefaultFont());
            DanmakuCore::DANMAKU_FONT = danmakuFontId;
        } else {
            // 使用默认弹幕字体
            DanmakuCore::DANMAKU_FONT = brls::Application::getDefaultFont();
        }

        // 初始化主题
        std::string themeData = getSettingItem(SettingItem::APP_THEME, std::string{"auto"});
        if (themeData == "light") {
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);
        } else if (themeData == "dark") {
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
        }

        // 初始化纹理缓存数量
#if defined(__PSV__) || defined(PS4)
        brls::TextureCache::instance().cache.setCapacity(1);
#else
        brls::TextureCache::instance().cache.setCapacity(getSettingItem(SettingItem::TEXTURE_CACHE_NUM, 200));
#endif

        // 初始化播放器音量
        MPVCore::VIDEO_VOLUME = getSettingItem(SettingItem::PLAYER_VOLUME, 100);

        // 设置窗口最小尺寸
#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
        int minWidth  = getIntOption(SettingItem::MINIMUM_WINDOW_WIDTH);
        int minHeight = getIntOption(SettingItem::MINIMUM_WINDOW_HEIGHT);
        brls::Application::getPlatform()->setWindowSizeLimits(minWidth, minHeight, 0, 0);
        checkOnTop();
#endif

        // Init keyboard shortcut
        brls::Application::getPlatform()->getInputManager()->getKeyboardKeyStateChanged()->subscribe(
            [](brls::KeyState state) {
                if (!state.pressed) return;
                switch (state.key) {
#ifndef __APPLE__
                    case brls::BRLS_KBD_KEY_F11:
                        ProgramConfig::instance().toggleFullscreen();
                        break;
#endif
                    case brls::BRLS_KBD_KEY_F: {
                        // 在编辑框弹出时不触发
                        auto activityStack  = brls::Application::getActivitiesStack();
                        brls::Activity* top = activityStack[activityStack.size() - 1];
                        if(!dynamic_cast<brls::EditTextDialog*>(top->getContentView())){
                            ProgramConfig::instance().toggleFullscreen();
                        }
                        break;
                    }
                    case brls::BRLS_KBD_KEY_SPACE: {
                        // 只在顶部的 Activity 中存在播放器组件时触发
                        auto activityStack  = brls::Application::getActivitiesStack();
                        brls::Activity* top = activityStack[activityStack.size() - 1];
                        VideoView* video    = dynamic_cast<VideoView*>(top->getContentView()->getView("video"));
                        if (video) {
                            video->togglePlay();
                        }
                        break;
                    }
                    default:
                        break;
                }
            });
    });

#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    // 窗口将要关闭时, 保存窗口状态配置
    brls::Application::getExitEvent()->subscribe([this]() { saveHomeWindowState(); });
#endif

    // 加载屏蔽的up主
    ProgramConfig::instance().upFilter = getSettingItem(SettingItem::UP_FILTER, std::unordered_set<uint64_t>{});

    // 检查不欢迎名单
    wiliwili::checkBanList();
}

ProgramOption ProgramConfig::getOptionData(SettingItem item) { return SETTING_MAP[item]; }

size_t ProgramConfig::getIntOptionIndex(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            int option = this->setting.at(optionData.key).get<int>();
            for (size_t i = 0; i < optionData.rawOptionList.size(); i++) {
                if (optionData.rawOptionList[i] == option) return i;
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

int ProgramConfig::getIntOption(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            return this->setting.at(optionData.key).get<int>();
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.rawOptionList[optionData.defaultOption];
        }
    }
    return optionData.rawOptionList[optionData.defaultOption];
}

bool ProgramConfig::getBoolOption(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            return this->setting.at(optionData.key).get<bool>();
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

int ProgramConfig::getStringOptionIndex(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            std::string option = this->setting.at(optionData.key).get<std::string>();
            for (size_t i = 0; i < optionData.optionList.size(); ++i)
                if (optionData.optionList[i] == option) return i;
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

void ProgramConfig::save() {
    const std::string path = this->getConfigDir() + "/wiliwili_config.json";
    // fs is defined in cpr/cpr.h
#ifndef IOS
    cpr::fs::create_directories(this->getConfigDir());
#endif
    nlohmann::json content(*this);
    std::ofstream writeFile(path);
    if (!writeFile) {
        brls::Logger::error("Cannot write config to: {}", path);
        return;
    }
    writeFile << content.dump(2);
    writeFile.close();
    brls::Logger::info("Write config to: {}", path);
}

void ProgramConfig::checkOnTop() {
    switch (getIntOption(SettingItem::ON_TOP_MODE)) {
        case 0:
            // 关闭
            brls::Application::getPlatform()->setWindowAlwaysOnTop(false);
            return;
        case 1:
            // 开启
            brls::Application::getPlatform()->setWindowAlwaysOnTop(true);
            return;
        case 2: {
            // 自动模式，根据窗口大小判断是否需要切换到置顶模式
            double factor     = brls::Application::getPlatform()->getVideoContext()->getScaleFactor();
            uint32_t minWidth = ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_WINDOW_WIDTH) * factor + 0.1;
            uint32_t minHeight =
                ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_WINDOW_HEIGHT) * factor + 0.1;
            bool onTop = brls::Application::windowWidth <= minWidth || brls::Application::windowHeight <= minHeight;
            brls::Application::getPlatform()->setWindowAlwaysOnTop(onTop);
            break;
        }
        default:
            break;
    }
}

#ifdef __PSV__
#define MEM_POOL_SIZE (26 * 1024 * 1024)
#define MEM_POOL_TYPE SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW
static void *s_mspace = nullptr;
static SceUID mempool_id = 0;
static void *mempool_addr = nullptr;
static size_t mempool_size = MEM_POOL_SIZE;

int __attribute__((optimize("no-optimize-sibling-calls"))) malloc_finalize() {
    if (s_mspace)
        sceClibMspaceDestroy(s_mspace);
    if (mempool_addr)
        sceGxmUnmapMemory(mempool_addr);
    if (mempool_id)
        sceKernelFreeMemBlock(mempool_id);
    return 0;
}

int malloc_init() {
    int res;
    if (s_mspace)
        return 0;
    mempool_id = sceKernelAllocMemBlock("curl_mempool", MEM_POOL_TYPE, mempool_size, NULL);
    sceKernelGetMemBlockBase(mempool_id, &mempool_addr);
    if (!mempool_addr)
        goto error;
    res = sceGxmMapMemory(mempool_addr, mempool_size, SCE_GXM_MEMORY_ATTRIB_RW);
    if (res != SCE_OK)
        goto error;
    s_mspace = sceClibMspaceCreate(mempool_addr, mempool_size);
    if (!s_mspace)
        goto error;

    return 0;
error:
    malloc_finalize();
    return 1;
}

void __attribute__((optimize("no-optimize-sibling-calls"))) *sce_malloc(size_t size) {
    if (!s_mspace)
        malloc_init();
    return sceClibMspaceMalloc(s_mspace, size);
}

void __attribute__((optimize("no-optimize-sibling-calls"))) sce_free(void *ptr) {
    if (!ptr || !s_mspace)
        return;
    sceClibMspaceFree(s_mspace, ptr);
}

void __attribute__((optimize("no-optimize-sibling-calls"))) *sce_calloc(size_t nelem, size_t size) {
    if (!s_mspace)
        malloc_init();
    return sceClibMspaceCalloc(s_mspace, nelem, size);
}

void __attribute__((optimize("no-optimize-sibling-calls"))) *sce_realloc(void *ptr, size_t size) {
    if (!s_mspace)
        malloc_init();
    return sceClibMspaceRealloc(s_mspace, ptr, size);
}

char __attribute__((optimize("no-optimize-sibling-calls"))) *sce_strdup(const char *str) {
    size_t len;
    char *newstr;
    if(!str)
        return (char *)nullptr;
    len = strlen(str) + 1;
    newstr = (char *)sce_malloc(len);
    if(!newstr)
        return (char *)nullptr;
    sceClibMemcpy(newstr, str, len);
    return newstr;
}
#endif

void ProgramConfig::init() {
    brls::Logger::info("wiliwili {}", APPVersion::instance().git_tag);
    wiliwili::initCrashDump();

    // 在窗口大小改变时检查是否需要切换到置顶模式
    brls::Application::getWindowSizeChangedEvent()->subscribe([]() { ProgramConfig::instance().checkOnTop(); });

    // Set min_threads and max_threads of http thread pool
#ifdef BOREALIS_USE_GXM
    // TODO: 不确定为什么 gles 版无法使用 libheap, 当 gxm 稳定后会移除 gles 版本，所以暂时忽略
    mbedtls_platform_set_calloc_free(sce_calloc, sce_free);
    curl_global_init_mem(CURL_GLOBAL_DEFAULT, sce_malloc, sce_free, sce_realloc, sce_strdup, sce_calloc);
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    cpr::async::startup(THREAD_POOL_MIN_THREAD_NUM, THREAD_POOL_MAX_THREAD_NUM, std::chrono::milliseconds(5000));

#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) brls::Logger::error("WSAStartup failed with error: {}", result);
#endif
#if defined(_MSC_VER)
#elif defined(__PSV__)
    int search_unk[2];
    if(_vshKernelSearchModuleByName("CapUnlocker", search_unk) >= 0) {
        brls::sync([]() {
            brls::Application::notify("CapUnlocker found");
        });
        sceKernelChangeThreadPriority(SCE_KERNEL_THREAD_ID_SELF, 64);
        sceKernelChangeThreadCpuAffinityMask(SCE_KERNEL_THREAD_ID_SELF, SCE_KERNEL_CPU_MASK_SYSTEM);
    }
#elif defined(PS4)
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) brls::Logger::error("cannot load net module");
    primary_dns                     = inet_addr(primaryDNSStr.c_str());
    secondary_dns                   = inet_addr(secondaryDNSStr.c_str());
    ps4_mpv_use_precompiled_shaders = 1;
    ps4_mpv_dump_shaders            = 0;
    // 在加载第一帧之后隐藏启动画面
    brls::sync([]() { sceSystemServiceHideSplashScreen(); });
#else
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        brls::Logger::info("Current working directory: {}", cwd);
    }
#endif

    // load custom theme
    this->loadCustomThemes();

    // load config from disk
    this->load();

    // init custom font path
    brls::FontLoader::USER_FONT_PATH = getConfigDir() + "/font.ttf";
    brls::FontLoader::USER_ICON_PATH = getConfigDir() + "/icon.ttf";

    if (access(brls::FontLoader::USER_ICON_PATH.c_str(), F_OK) == -1) {
        // 自定义字体不存在，使用内置字体
#if defined(__PSV__) || defined(PS4)
        brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
#else
        std::string icon = getSettingItem(SettingItem::KEYMAP, std::string{"xbox"});
        if (icon == "xbox") {
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_xbox.ttf");
        } else if (icon == "ps") {
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
        } else {
            if (getBoolOption(SettingItem::APP_SWAP_ABXY)) {
                brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard_swap.ttf");
            } else {
                brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard.ttf");
            }
        }
#endif
    }

    brls::FontLoader::USER_EMOJI_PATH = getConfigDir() + "/emoji.ttf";
    if (access(brls::FontLoader::USER_EMOJI_PATH.c_str(), F_OK) == -1) {
        // 自定义emoji不存在，使用内置emoji
        brls::FontLoader::USER_EMOJI_PATH = BRLS_ASSET("font/emoji.ttf");
    }

    // set bilibili cookie and cookie update callback
    Cookie diskCookie = this->getCookie();
    BILI::init(
        diskCookie,
        [](const Cookie& newCookie, const std::string& token) {
            brls::Logger::info("======== write cookies to disk");
            ProgramConfig::instance().setCookie(newCookie);
            ProgramConfig::instance().setRefreshToken(token);
            // 用户重新登录后，恢复默认清晰度设置
            VideoDetail::defaultQuality = WILI_VIDEO_QUALITY_DEFAULT;
        });
    BILI::setProxy(httpProxy, httpsProxy);
    BILI::setTlsVerify(getBoolOption(SettingItem::TLS_VERIFY));
    BILI::setHttpTimeout(getSettingItem(SettingItem::HTTP_TIMEOUT, 5000));
    BILI::setConnectionTimeout(getSettingItem(SettingItem::HTTP_CONNECTION_TIMEOUT, 0));
    BILI::setDnsCacheTimeout(getSettingItem(SettingItem::HTTP_DNS_CACHE_TIMEOUT, WILI_DNS_CACHE_TIMEOUT));
}

std::string ProgramConfig::getHomePath() {
#if defined(__SWITCH__)
    return "/";
#elif defined(_WIN32)
    return std::string(getenv("HOMEPATH"));
#else
    return std::string(getenv("HOME"));
#endif
}

std::string ProgramConfig::getConfigDir() {
#ifdef __SWITCH__
    return "/config/wiliwili";
#elif defined(PS4)
    return "/data/wiliwili";
#elif defined(__PSV__)
    return "ux0:/data/wiliwili";
#elif defined(IOS)
    CFURLRef homeURL = CFCopyHomeDirectoryURL();
    if (homeURL != nullptr) {
        char buffer[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(homeURL, true, reinterpret_cast<UInt8*>(buffer), sizeof(buffer))) {
        }
        CFRelease(homeURL);
        return std::string{buffer} + "/Library/Preferences";
    }
    return "../Library/Preferences";
#else
#ifdef _DEBUG
    char currentPathBuffer[PATH_MAX];
    std::string currentPath = getcwd(currentPathBuffer, sizeof(currentPathBuffer));
#ifdef _WIN32
    return currentPath + "\\config\\wiliwili";
#else
    return currentPath + "/config/wiliwili";
#endif /* _WIN32 */
#else
#ifdef __APPLE__
    return std::string(getenv("HOME")) + "/Library/Application Support/wiliwili";
#endif
#ifdef __linux__
    std::string config = "";
    char* config_home  = getenv("XDG_CONFIG_HOME");
    if (config_home) config = std::string(config_home);
    if (config.empty()) config = std::string(getenv("HOME")) + "/.config";
    return config + "/wiliwili";
#endif
#ifdef _WIN32
    WCHAR wpath[MAX_PATH];
    std::vector<char> lpath(MAX_PATH);
    SHGetSpecialFolderPathW(0, wpath, CSIDL_LOCAL_APPDATA, false);
    WideCharToMultiByte(CP_UTF8, 0, wpath, std::wcslen(wpath), lpath.data(), lpath.size(), nullptr, nullptr);
    return std::string(lpath.data()) + "\\xfangfang\\wiliwili";
#endif
#endif /* _DEBUG */
#endif /* __SWITCH__ */
}

void ProgramConfig::exit(char* argv[]) {
    cpr::async::cleanup();
    curl_global_cleanup();

#ifdef _WIN32
    WSACleanup();
#endif
#ifdef IOS
#elif defined(PS4)
#elif __PSV__
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    if (!brls::DesktopPlatform::RESTART_APP) return;
#ifdef __linux__
    char filePath[PATH_MAX + 1];
    ssize_t count = readlink("/proc/self/exe", filePath, PATH_MAX);
    if (count <= 0)
        strcpy(filePath, argv[0]);
    else
        filePath[count] = 0;
#else
    char* filePath = argv[0];
#endif

    brls::Logger::info("Restart app {}", filePath);

    execv(filePath, argv);
#endif
}

void ProgramConfig::loadCustomThemes() {
    customThemes.clear();
    std::string directoryPath = getConfigDir() + "/theme";
    if (!cpr::fs::exists(directoryPath)) return;

    for (const auto& entry : cpr::fs::directory_iterator(getConfigDir() + "/theme")) {
#if USE_BOOST_FILESYSTEM
        if (!cpr::fs::is_directory(entry)) continue;
#else
        if (!entry.is_directory()) continue;
#endif
        std::string subDirectory = entry.path().string();
        std::string jsonFilePath = subDirectory + "/resources_meta.json";
        if (!cpr::fs::exists(jsonFilePath)) continue;

        std::ifstream readFile(jsonFilePath);
        if (readFile) {
            try {
                nlohmann::json content;
                readFile >> content;
                readFile.close();
                CustomTheme customTheme;
                customTheme.path = subDirectory + "/";
                customTheme.id   = entry.path().filename().string();
                content.get_to(customTheme);
                customThemes.emplace_back(customTheme);
                brls::Logger::info("Load custom theme \"{}\" from: {}", customTheme.name, jsonFilePath);
            } catch (const std::exception& e) {
                brls::Logger::error("CustomTheme::load: {}", e.what());
                continue;
            }
        }
    }
}

std::vector<CustomTheme> ProgramConfig::getCustomThemes() { return customThemes; }

std::string ProgramConfig::getProxy() {
    if (!httpsProxy.empty()) return httpsProxy;
    return httpProxy;
}

void ProgramConfig::setProxy(const std::string& proxy) {
    this->httpsProxy = proxy;
    this->httpProxy  = proxy;
    BILI::setProxy(httpProxy, httpsProxy);
}

void ProgramConfig::setTlsVerify(bool verify) { BILI::setTlsVerify(verify); }

void ProgramConfig::addSeasonCustomSetting(const std::string& key, const SeasonCustomItem& item) {
    this->seasonCustom[key] = item;
    this->save();
}

SeasonCustomSetting ProgramConfig::getSeasonCustomSetting() const { return this->seasonCustom; }

SeasonCustomItem ProgramConfig::getSeasonCustom(const std::string& key) const {
    if (this->seasonCustom.count(key) == 0) {
        return SeasonCustomItem{};
    }
    return this->seasonCustom.at(key);
}

SeasonCustomItem ProgramConfig::getSeasonCustom(unsigned int key) const {
    return this->getSeasonCustom(std::to_string(key));
}

void ProgramConfig::addSeasonCustomSetting(unsigned int key, const SeasonCustomItem& item) {
    this->addSeasonCustomSetting(std::to_string(key), item);
}

void ProgramConfig::setSeasonCustomSetting(const SeasonCustomSetting& value) {
    this->seasonCustom = value;
    this->save();
}

void ProgramConfig::toggleFullscreen() {
    bool value = !getBoolOption(SettingItem::FULLSCREEN);
    setSettingItem(SettingItem::FULLSCREEN, value);
    VideoContext::FULLSCREEN = value;
    brls::Application::getPlatform()->getVideoContext()->fullScreen(value);
    GA("player_setting", {{"fullscreen", value ? "true" : "false"}});
}