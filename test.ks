@iscript
{
    //TTSBridgeのインスタンス作成
    tf.tts = new TTSBridge(void, void, void);
    
    // 音声の一覧取得
    tf.voices = tf.tts.list('ja-JP', 'Female');

    Debug.message(tf.voices[0]);
    
    // 同期的に音声を再生
    tf.tts.speakSync(0, 'ja-JP', 'Female', 'こんにちは世界', 50, false);
    
    // 非同期的に音声を再生
    tf.token = tf.tts.speakAsync(3, 'ja-JP', '', 'こんにちは世界', 50, false);
}
@endscript