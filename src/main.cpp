#include "krkrvoice.hpp"
#include "krkrvoice_win.hpp"
#include <windows.h>
#include <winrt/base.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <cctype>

// COM／WinRT 初期化
struct ComInit {
    ComInit() {
        ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~ComInit() {
        winrt::uninit_apartment();
        ::CoUninitialize();
    }
};

// 大文字小文字を区別しない比較
static bool ieq(std::wstring_view a, std::wstring_view b) {
    return _wcsicmp(a.data(), b.data()) == 0;
}

// UTF-8 → UTF-16
static std::wstring WStringFromUTF8(const std::string& s) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(s);
}

// ASCII 小文字化
static std::string toLower(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s)
        r += static_cast<char>(std::tolower(c));
    return r;
}

// コマンドライン解析結果
struct Options {
    std::wstring ttsType = L"win";   // /tts=win, /tts=vox
    std::wstring lang;
    std::wstring gender;
    int          voiceIdx = -1;
    std::wstring text;
    int          speed    = 0;      // 0–100
    bool         async    = false;
    bool         overlap  = false;
    bool         listOnly = false;
};

// 引数解析
static Options ParseArgs(int argc, wchar_t* argv[]) {
    Options o;
    std::vector<std::wstring> freeArgs;
    std::unordered_map<std::string, std::string> opts;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if ((arg[0]=='/'||arg[0]=='-') && arg.size()>=2) {
            auto core = arg.substr(1);
            auto eq = core.find(L'=');
            std::wstring wkey = eq==std::wstring::npos ? core : core.substr(0, eq);
            std::string key = toLower(std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(wkey));
            std::string val;
            if (eq!=std::wstring::npos)
                val = std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(core.substr(eq+1));
            opts[key] = val;
        }
        else {
            freeArgs.push_back(arg);
        }
    }
    if (opts.count("tts"))     o.ttsType  = WStringFromUTF8(opts["tts"]);
    if (opts.count("lang"))    o.lang     = WStringFromUTF8(opts["lang"]);
    if (opts.count("gender"))  o.gender   = WStringFromUTF8(opts["gender"]);
    if (opts.count("voice")||opts.count("v")) {
        auto vs = opts.count("voice") ? opts["voice"] : opts["v"];
        o.voiceIdx = std::stoi(vs);
    }
    if (opts.count("speed"))
        o.speed = std::clamp(std::stoi(opts["speed"]), 0, 100);
    if (opts.count("async"))    o.async    = true;
    if (opts.count("overlap"))  o.overlap  = true;
    if (opts.count("stop")||opts.count("replace")) o.overlap = false;
    if (opts.count("list"))     o.listOnly = true;

    if (!freeArgs.empty()) {
        std::wstring txt;
        for (size_t i = 0; i < freeArgs.size(); ++i) {
            if (i) txt += L' ';
            txt += freeArgs[i];
        }
        o.text = txt;
    }
    return o;
}

int wmain(int argc, wchar_t* argv[]) {
    ComInit comInit;
    auto opt = ParseArgs(argc, argv);

    // TTS サービス取得（文字列版ファクトリを利用）
    auto svc = krkrvoice::GetTTSService(opt.ttsType);
    if (!svc) {
        std::wcerr << L"TTS サービスを初期化できません\n";
        return -1;
    }

    // リスト表示 or インデックス未指定
    if (opt.listOnly || opt.voiceIdx < 0) {
        auto voices = svc->GetVoiceList(opt.lang, opt.gender);
        std::wcout << L"Voice List (" << voices.size() << L")\n";
        for (size_t i = 0; i < voices.size(); ++i) {
            const auto& v = voices[i];
            std::wcout << i << L": " << v.displayName
                       << L" (" << v.engine << L"," << v.lang << L"," << v.gender << L")\n";
        }
        return 0;
    }

    // 再度リスト取得して範囲チェック
    auto voices = svc->GetVoiceList(opt.lang, opt.gender);
    if (static_cast<size_t>(opt.voiceIdx) >= voices.size()) {
        std::wcerr << L"インデックスが範囲外です\n";
        return -1;
    }
    if (opt.text.empty())
        opt.text = L"(テキスト未指定)";

    const auto& vi = voices[opt.voiceIdx];
    bool ok = false;
    if (opt.async) {
        svc->SpeakText(vi, opt.text, opt.speed, false, opt.overlap, []{ /* onFinish */ });
        ok = true;
    }
    else {
        ok = svc->SpeakText(vi, opt.text, opt.speed, true, opt.overlap);
        if (!ok)
            std::wcerr << L"再生に失敗しました\n";
    }
    return ok ? 0 : -1;
}