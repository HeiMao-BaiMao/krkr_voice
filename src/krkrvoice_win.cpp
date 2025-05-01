// -----------------------------------------------------------------------------
// krkrvoice_win.cpp   ―  WinTTS (SAPI + WinRT) 実装
// -----------------------------------------------------------------------------
#include "krkrvoice_win.hpp"

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <atlbase.h>

#include <vector>
#include <algorithm>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Core.h>

using namespace krkrvoice;

// -----------------------------------------------------------------------------
// 内部ユーティリティ
// -----------------------------------------------------------------------------
namespace {

std::mutex g_mutex;
winrt::Windows::Media::Playback::MediaPlayer g_singlePlayer{ nullptr };   // 上書き用
std::vector<winrt::Windows::Media::Playback::MediaPlayer> g_players;      // 重ね再生保持

// 速度 0–100 → 0.5×–2.0×
static float NormalizeSpeed(int s)
{
    s = std::clamp(s, 0, 100);
    return s == 0 ? 1.0f : (0.5f + (s - 1) * (1.5f / 99.0f));
}

// 文字列 LCID → BCP-47
static std::wstring LocaleNameToHexLCID(std::wstring_view name)
{
    if (name.empty()) return L"";
    LCID id = ::LocaleNameToLCID(std::wstring(name).c_str(), 0);
    if (!id) return L"";
    wchar_t buf[8]{};
    swprintf_s(buf, L"%X", id);
    return buf;
}

// SAPI トークンから Gender
static std::wstring GetGenderStringSAPI(CComPtr<ISpObjectToken> tok)
{
    CComPtr<ISpDataKey> attr;
    if (FAILED(tok->OpenKey(L"Attributes", &attr)) || !attr) return L"Unknown";
    WCHAR* v = nullptr;
    if (SUCCEEDED(attr->GetStringValue(L"Gender", &v)) && v) {
        std::wstring g = v; ::CoTaskMemFree(v); return g;
    }
    return L"Unknown";
}

// SAPI 音声列挙
static void EnumSapiVoices(std::vector<VoiceInfo>& out,
                           const std::wstring& langF,
                           const std::wstring& genF)
{
    std::wstring attr;
    if (!langF.empty()) attr = L"Language=" + LocaleNameToHexLCID(langF);

    CComPtr<IEnumSpObjectTokens> en;
    SpEnumTokens(SPCAT_VOICES, attr.empty() ? nullptr : attr.c_str(), nullptr, &en);
    if (!en) return;

    ULONG f = 0;
    while (true) {
        CComPtr<ISpObjectToken> tok;
        if (en->Next(1, &tok, &f) != S_OK || !tok) break;

        // Language → ja-JP 等
        std::wstring tag = L"?";
        {
            CComPtr<ISpDataKey> k;
            if (SUCCEEDED(tok->OpenKey(L"Attributes", &k)) && k) {
                WCHAR* v = nullptr;
                if (SUCCEEDED(k->GetStringValue(L"Language", &v)) && v) {
                    std::wstring hex = std::wcstok(v, L";");
                    LCID id = static_cast<LCID>(wcstoul(hex.c_str(), nullptr, 16));
                    WCHAR buf[LOCALE_NAME_MAX_LENGTH]{};
                    if (::LCIDToLocaleName(id, buf, _countof(buf), 0) > 0) tag = buf;
                    ::CoTaskMemFree(v);
                }
            }
        }

        std::wstring g = GetGenderStringSAPI(tok);
        if (!genF.empty() && _wcsicmp(g.c_str(), genF.c_str()) != 0) continue;

        WCHAR* desc = nullptr; SpGetDescription(tok, &desc);

        out.push_back(VoiceInfo{
            TTSService::WinTTS, L"SAPI",
            desc ? desc : L"<NoDesc>", tag, g });

        ::CoTaskMemFree(desc);
    }
}

// WinRT 音声列挙
static void EnumWinRTVoices(std::vector<VoiceInfo>& out,
                            const std::wstring& langF,
                            const std::wstring& genF)
{
    namespace SS = winrt::Windows::Media::SpeechSynthesis;
    SS::SpeechSynthesizer sy;
    for (auto const& vi : sy.AllVoices()) {
        if (!langF.empty() && vi.Language() != langF) continue;

        std::wstring g = (vi.Gender() == SS::VoiceGender::Male) ? L"Male"
                        : (vi.Gender() == SS::VoiceGender::Female) ? L"Female" : L"Other";
        if (!genF.empty() && _wcsicmp(g.c_str(), genF.c_str()) != 0) continue;

        out.push_back(VoiceInfo{
            TTSService::WinTTS, L"WinRT",
            vi.DisplayName().c_str(), vi.Language().c_str(), g });
    }
}

} // unnamed namespace

// -----------------------------------------------------------------------------
// WinTTSService 実装
// -----------------------------------------------------------------------------
std::vector<VoiceInfo>
WinTTSService::GetVoiceList(const std::wstring& lang, const std::wstring& gen)
{
    std::vector<VoiceInfo> v;
    EnumSapiVoices(v, lang, gen);
    EnumWinRTVoices(v, lang, gen);
    return v;
}

bool
WinTTSService::SpeakText(const VoiceInfo& voice,
                         const std::wstring& text,
                         int  speed,
                         bool sync,
                         bool overlap,
                         std::function<void()> onFinish)
{
    float rate = NormalizeSpeed(speed);

    //--------------------------- SAPI ----------------------------------
    if (voice.engine == L"SAPI") {
        CComPtr<ISpVoice> sp; sp.CoCreateInstance(CLSID_SpVoice);
        sp->SetRate(static_cast<long>((rate - 1.0f) * 10));

        if (sync) {                                  // 同期
            HRESULT hr = sp->Speak(text.c_str(), SPF_DEFAULT, nullptr);
            if (onFinish) onFinish();
            return SUCCEEDED(hr);
        }

        // 非同期
        sp->Speak(text.c_str(), SPF_ASYNC, nullptr);

        if (onFinish) {
            std::thread([sp, onFinish] {
                SPVOICESTATUS st{};
                do { sp->GetStatus(&st, nullptr); ::Sleep(40); }
                while (st.dwRunningState != SPRS_DONE);
                onFinish();
            }).detach();
        }
        return true;
    }

    //--------------------------- WinRT ---------------------------------
    if (voice.engine == L"WinRT") {
        namespace SS  = winrt::Windows::Media::SpeechSynthesis;
        namespace WP  = winrt::Windows::Media::Playback;
        namespace WC  = winrt::Windows::Media::Core;

        SS::SpeechSynthesizer sy;
        for (auto const& vi : sy.AllVoices())
            if (vi.DisplayName() == voice.displayName &&
                vi.Language()   == voice.lang) { sy.Voice(vi); break; }

        auto stream = sy.SynthesizeTextToStreamAsync(text).get();
        WP::MediaPlayer pl = overlap ? WP::MediaPlayer() : g_singlePlayer;

        std::atomic_bool ended{ false };

        {   // クリティカル領域
            std::lock_guard<std::mutex> lk(g_mutex);

            if (!overlap) {
                if (!pl) pl = g_singlePlayer = WP::MediaPlayer();
                pl.Source(nullptr);                       // 旧再生停止
            } else {
                g_players.emplace_back(pl);               // 保持して破棄防止
            }

            pl.Source(WC::MediaSource::CreateFromStream(stream, L"audio/wav"));
            pl.PlaybackSession().PlaybackRate(rate);

            pl.PlaybackSession().PlaybackStateChanged(
                [pl, &ended, onFinish](auto&&, auto&&) {
                    auto st = pl.PlaybackSession().PlaybackState();
                    if (st == WP::MediaPlaybackState::Paused || st == WP::MediaPlaybackState::None) {
                        if (!ended.exchange(true) && onFinish) onFinish();
                    }
                });

            pl.Play();
        }

        if (sync) while (!ended.load()) ::Sleep(40);
        return true;
    }

    return false;
}