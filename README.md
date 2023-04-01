# S44EXP.X
ADPCM/PCM/WAV/MP3 player for X680x0/Human68k

以下の形式のファイルを再生するプレーヤーです。

[MP3EXP.X](https://github.com/tantanGH/mp3exp)からMP3のサポートを外して軽量化したものになります。(ファイルサイズ1/3以下)
MP3以外の機能は同じです。使い方などは MP3EXP.X の説明を参考にしてください。

---

### インストール方法

S4EXPXxxx.ZIP をダウンロードして展開し、S44EXP.X をパスの通ったディレクトリに置きます。

---

### 使用方法

- 内蔵ADPCMで鳴らす場合

pcm8a.x を常駐させて、

    s44exp <PCMファイル名>


- Mercury-UNITで鳴らす場合

pcm8pp.x を常駐させて、

    s44exp <PCMファイル名>


- KMD歌詞表示を行う場合

全画面、アートワーク輝度70%

    s44exp -x -t70 <PCMファイル名>


060loadhigh.x を使ったハイメモリ上での実行に対応しています。

X68000Zで動かす場合は必ず pcm8a.x と組み合わせて実行させてください。

---

### Special Thanks

* xdev68k thanks to ファミべのよっしんさん
* HAS060.X on run68mac thanks to YuNKさん / M.Kamadaさん / GOROmanさん
* HLK301.X on run68mac thanks to SALTさん / GOROmanさん

---

### History

* 1.1.0 (2023/04/01) ... 初版
