#include "krkrvoice.hpp"
#include "krkrvoice_win.hpp" // WinTTSService 用

namespace krkrvoice
{

    // 新しいオーバーロード：TTSService を直接指定する形式
    std::shared_ptr<ITTSService> GetTTSService(TTSService service, const std::wstring &url, int port)
    {
        switch (service)
        {
        case TTSService::WinTTS:
            return std::make_shared<WinTTSService>();
        case TTSService::VoiceVox:
            // 将来的に VoiceVoxService を追加する予定
            return nullptr;
        default:
            return nullptr;
        }
    }

    // 既存の name 文字列指定の形式はそのまま保持
    std::shared_ptr<ITTSService> GetTTSService(const std::wstring &name, const std::wstring &url, int port)
    {
        if (name == L"win" || name == L"wintts")
        {
            return std::make_shared<WinTTSService>();
        }
        // 将来 VOICEVOX 対応時:
        // if (name == L"vox") {
        //     return std::make_shared<VoiceVoxService>(url, port);
        // }
        return nullptr;
    }

} // namespace krkrvoice
