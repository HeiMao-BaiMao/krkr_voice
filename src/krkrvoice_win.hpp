#pragma once
#include "krkrvoice.hpp"
#include <functional> 

namespace krkrvoice {

// SAPI + WinRT をまとめて扱うサービス
class WinTTSService final : public ITTSService {
public:
    std::vector<VoiceInfo>
    GetVoiceList(const std::wstring& lang = L"", const std::wstring& gender = L"") override;

    bool SpeakText(const VoiceInfo& voice,
        const std::wstring& text,
        int  speed,
        bool sync,
        bool overlap,
        std::function<void()> onFinish = {});
};

} // namespace krkrvoice
