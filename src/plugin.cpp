// -----------------------------------------------------------------------------
// plugin.cpp ― TTSBridge with multi-dictionary + regex replace
// -----------------------------------------------------------------------------
#include "ncbind.hpp"
#include "krkrvoice.hpp"
#include "krkrvoice_win.hpp"

#include <windows.h>
#include <winrt/base.h>

#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <regex>
#include <set>
#include <functional>
#include <stdexcept>

namespace {
struct ComInit {
    ComInit()  { ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                 winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~ComInit() { winrt::uninit_apartment(); ::CoUninitialize(); }
} _gComInit;
}

using namespace krkrvoice;

static std::vector<std::pair<std::wregex, std::wstring>> current_dict;

static tjs_error TJS_INTF_METHOD EnumDictCallback(
    tjs_int flag, const tjs_char* name, tTJSVariant* value,
    iTJSDispatch2* objthis, void* param)
{
    try {
        std::wregex pattern(name);
        current_dict.emplace_back(pattern, value->GetString());
    } catch (...) {}
    return TJS_S_OK;
}

class TTSToken {
public:
    TTSToken() : flag_(std::make_shared<std::atomic_bool>(false)) {}
    std::shared_ptr<std::atomic_bool> flag() { return flag_; }
    void wait() const { while (!flag_->load()) ::Sleep(30); }
    bool done() const { return flag_->load(); }
private:
    std::shared_ptr<std::atomic_bool> flag_;
};

class TTSBridge {
public:
    explicit TTSBridge(tTJSVariant serviceStr = tTJSVariant(),
                       tTJSVariant url = tTJSVariant(),
                       tTJSVariant port = tTJSVariant()) {
        std::wstring name = (serviceStr.Type() != tvtVoid) ? std::wstring(serviceStr.GetString()) : L"win";
        std::wstring endpoint = (url.Type() != tvtVoid) ? std::wstring(url.GetString()) : L"http://127.0.0.1";
        int portnum = (port.Type() != tvtVoid) ? static_cast<int>((tjs_int)port) : 50021;
        svc_ = GetServiceByName(name, endpoint, portnum);
        if (!svc_) throw std::runtime_error("TTS service not available");
    }

    std::vector<std::wstring> list(const tjs_char* lang = L"", const tjs_char* gender = L"") {
        auto v = svc_->GetVoiceList(lang, gender);
        std::vector<std::wstring> out;
        for (size_t i = 0; i < v.size(); ++i) {
            const auto& e = v[i];
            out.emplace_back(std::to_wstring(i) + L":" + e.displayName +
                             L" (" + e.engine + L"," + e.lang + L"," + e.gender + L")");
        }
        return out;
    }

    bool speakSync(int idx, const tjs_char* lang, const tjs_char* gender,
                   const tjs_char* text, int speed = 0, bool overlap = false) {
        auto v = svc_->GetVoiceList(lang, gender);
        if (idx < 0 || static_cast<size_t>(idx) >= v.size()) return false;
        std::wstring processed = applyDictionary(text ? text : L"");
        return svc_->SpeakText(v[idx], processed, speed, true, overlap);
    }

    TTSToken speakAsync(int idx, const tjs_char* lang, const tjs_char* gender,
                        const tjs_char* text, int speed = 0, bool overlap = false) {
        TTSToken tok;
        auto flag = tok.flag();
        auto v = svc_->GetVoiceList(lang, gender);
        if (idx < 0 || static_cast<size_t>(idx) >= v.size()) {
            flag->store(true);
            return tok;
        }
        std::wstring processed = applyDictionary(text ? text : L"");
        svc_->SpeakText(v[idx], processed, speed, false, overlap, [flag] { flag->store(true); });
        return tok;
    }

    void registerDictionary(const std::wstring& name,
        const std::vector<std::pair<std::wregex, std::wstring>>& dict) {
        dictionaries_[name] = dict;
    }

    void enableDictionary(const std::wstring& name, bool enable) {
        if (enable) enabledDictionaries_.insert(name);
        else enabledDictionaries_.erase(name);
    }

private:
    std::shared_ptr<ITTSService> svc_;
    std::map<std::wstring, std::vector<std::pair<std::wregex, std::wstring>>> dictionaries_;
    std::set<std::wstring> enabledDictionaries_;

    std::wstring applyDictionary(const std::wstring& text) const {
        std::wstring result = text;
        for (const auto& name : enabledDictionaries_) {
            auto it = dictionaries_.find(name);
            if (it == dictionaries_.end()) continue;
            for (const auto& [pattern, replacement] : it->second) {
                result = std::regex_replace(result, pattern, replacement);
            }
        }
        return result;
    }

    static std::shared_ptr<ITTSService>
    GetServiceByName(const std::wstring& name,
                     const std::wstring& endpoint,
                     int port) {
        if (name == L"win" || name == L"wintts")
            return std::make_shared<WinTTSService>();
        return nullptr;
    }
};

// ---------------------------- RawCallbacks ----------------------------

// list(lang?, gender?)
tjs_error TJS_INTF_METHOD ListWrapper(tTJSVariant *result, tjs_int numparams,
    tTJSVariant **params, iTJSDispatch2 *objthis) {
    TTSBridge* self = ncbInstanceAdaptor<TTSBridge>::GetNativeInstance(objthis);
    if (!self) return TJS_E_INVALIDPARAM;
    const tjs_char* lang = (numparams >= 1 && params[0]) ? (*params[0]).GetString() : L"";
    const tjs_char* gender = (numparams >= 2 && params[1]) ? (*params[1]).GetString() : L"";
    auto voices = self->list(lang, gender);
    iTJSDispatch2* array = TJSCreateArrayObject();
    for (size_t i = 0; i < voices.size(); ++i) {
        tTJSVariant val(voices[i].c_str());
        array->PropSetByNum(TJS_MEMBERENSURE, static_cast<tjs_int>(i), &val, array);
    }
    *result = tTJSVariant(array, array);
    array->Release();
    return TJS_S_OK;
}

// registerDictionary(name, entriesArray)
// entriesArray: [{pattern: "regex", replacement: "text"}, ...]
tjs_error TJS_INTF_METHOD RegisterDictionaryCallback(
    tTJSVariant *result, tjs_int numparams,
    tTJSVariant **params, iTJSDispatch2 *objthis)
{
    if (numparams < 2) return TJS_E_BADPARAMCOUNT;
    TTSBridge* self = ncbInstanceAdaptor<TTSBridge>::GetNativeInstance(objthis);
    if (!self) return TJS_E_INVALIDPARAM;
    std::wstring name = std::wstring((*params[0]).GetString());
    if (params[1]->Type() != tvtObject) return TJS_E_INVALIDPARAM;
    iTJSDispatch2* arrObj = (*params[1]).AsObjectNoAddRef();
    // get length
    tTJSVariant lenVar;
    arrObj->PropGet(TJS_IGNOREPROP, TJS_W("length"), nullptr, &lenVar, arrObj);
    int len = static_cast<int>(lenVar);
    std::vector<std::pair<std::wregex, std::wstring>> dict;
    for (int i = 0; i < len; ++i) {
        tTJSVariant elemVar;
        arrObj->PropGetByNum(TJS_IGNOREPROP, i, &elemVar, arrObj);
        iTJSDispatch2* entryObj = elemVar.AsObjectNoAddRef();
        tTJSVariant patVar, repVar;
        entryObj->PropGet(TJS_IGNOREPROP, TJS_W("pattern"), nullptr, &patVar, entryObj);
        entryObj->PropGet(TJS_IGNOREPROP, TJS_W("replacement"), nullptr, &repVar, entryObj);
        try {
            dict.emplace_back(std::wregex(patVar.GetString()), repVar.GetString());
        } catch (...) {}
    }
    self->registerDictionary(name, dict);
    return TJS_S_OK;
}

// enableDictionary(name, bool)
tjs_error TJS_INTF_METHOD EnableDictionaryCallback(
    tTJSVariant *result, tjs_int numparams,
    tTJSVariant **params, iTJSDispatch2 *objthis)
{
    if (numparams < 2) return TJS_E_BADPARAMCOUNT;
    TTSBridge* self = ncbInstanceAdaptor<TTSBridge>::GetNativeInstance(objthis);
    if (!self) return TJS_E_INVALIDPARAM;
    std::wstring name = std::wstring((*params[0]).GetString());
    bool enable = (*params[1]).operator bool();
    self->enableDictionary(name, enable);
    return TJS_S_OK;
}

// ---------------------------- ncbind ----------------------------
NCB_REGISTER_CLASS(TTSToken) {
    NCB_CONSTRUCTOR(());
    NCB_METHOD(wait);
    NCB_METHOD(done);
}

NCB_REGISTER_CLASS(TTSBridge) {
    NCB_CONSTRUCTOR((tTJSVariant, tTJSVariant, tTJSVariant));
    RawCallback("list", &ListWrapper, 0);
    RawCallback("registerDictionary", &RegisterDictionaryCallback, 0);
    RawCallback("enableDictionary", &EnableDictionaryCallback, 0);
    NCB_METHOD(speakSync);
    NCB_METHOD(speakAsync);
}