#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace krkrvoice {

// どの TTS サービスで再生するか
enum class TTSService {
    WinTTS,   // SAPI + WinRT のセット
    VoiceVox, // 将来追加予定
};

// 音声 1 件分の情報
struct VoiceInfo {
    TTSService   service;      // 取得元サービス
    std::wstring engine;       // "SAPI" / "WinRT" / "VOICEVOX" など
    std::wstring displayName;  // UI 表示名
    std::wstring lang;         // "ja-JP" など
    std::wstring gender;       // "Male" / "Female" / "Other"
};

// サービス共通抽象クラス
class ITTSService {
public:
    virtual ~ITTSService() = default;

    // 音声の一覧取得（言語・性別フィルタは空文字列なら無視）
    virtual std::vector<VoiceInfo>
    GetVoiceList(const std::wstring& lang = L"", const std::wstring& gender = L"") = 0;

    // 指定音声で再生（速度 0-100、0 は等速）
    virtual bool
    SpeakText(const VoiceInfo& voice,
              const std::wstring& text,
              int speed,
              bool sync,
              bool overlap,
              std::function<void()> onFinish = {}) = 0;
};

// enum→サービス取得
std::shared_ptr<ITTSService>
GetTTSService(TTSService service,
              const std::wstring& url = L"http://127.0.0.1",
              int port = 50021);

// 文字列→サービス取得（オーバーロード）
std::shared_ptr<ITTSService>
GetTTSService(const std::wstring& name,
              const std::wstring& url = L"http://127.0.0.1",
              int port = 50021);

} // namespace krkrvoice