@iscript
if(typeof global.ttsBridge === "undefined") {
    global.ttsBridge = new TTSBridge();
}
global.ttsToken = null;

// 音声分解関数
function parseVoiceInfo(s) {
    var p = s.indexOf(":");
    var idx = s.slice(0, p);
    var rest = s.slice(p + 1).split(",");
    return {
        index: idx,
        displayName: rest[0],
        engine: rest[1],
        lang: rest[2],
        gender: rest[3]
    };
}

// 辞書登録関数
function ttsRegisterDict(name, entries) {
    if(global.ttsBridge && name && entries && entries.length > 0) {
        global.ttsBridge.registerDictionary(name, entries);
    }
}

// 辞書有効化/無効化関数
function ttsEnableDict(name, enable) {
    if(global.ttsBridge && name) {
        global.ttsBridge.enableDictionary(name, enable);
    }
}
@endscript

[macro name="tts_sync"]
    @iscript
    if(global.ttsToken && global.ttsToken.cancel) {
        global.ttsToken.cancel();
        global.ttsToken = null;
    }
    var text = %text;
    var idx = typeof %voiceIdx !== "undefined" ? %voiceIdx : 0;
    var speed = typeof %speed !== "undefined" ? %speed : 0;
    if(global.ttsBridge) {
        global.ttsToken = global.ttsBridge.speakSync(idx, "ja-JP", "Female", text, speed, false);
    }
    @endscript
[endmacro]

[macro name="tts_async"]
    @iscript
    if(global.ttsToken && global.ttsToken.cancel) {
        global.ttsToken.cancel();
        global.ttsToken = null;
    }
    var text = %text;
    var idx = typeof %voiceIdx !== "undefined" ? %voiceIdx : 0;
    var speed = typeof %speed !== "undefined" ? %speed : 0;
    if(global.ttsBridge) {
        global.ttsToken = global.ttsBridge.speakAsync(idx, "ja-JP", "Female", text, speed, false);
    }
    @endscript
[endmacro]

[macro name="tts_cancel"]
    @iscript
    if(global.ttsToken && global.ttsToken.cancel) {
        global.ttsToken.cancel();
        global.ttsToken = null;
    }
    @endscript
[endmacro]

[macro name="tts_register_dict"]
    @iscript
    ; nameとentries（配列）をマクロ引数から受け取る
    ttsRegisterDict(%name, %entries);
    @endscript
[endmacro]

[macro name="tts_enable_dict"]
    @iscript
    ; nameとenable（bool）をマクロ引数から受け取る
    ttsEnableDict(%name, typeof %enable !== "undefined" ? %enable : true);
    @endscript
[endmacro]