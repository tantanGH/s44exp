# S44EXP.X
ADPCM/PCM/WAV player for X680x0/Human68k

[MP3EXP.X](https://github.com/tantanGH/mp3exp)からMP3のサポートを外して軽量化したものになります。(ファイルサイズ1/3以下)
MP3以外の機能は同じです。使い方などは MP3EXP.X の説明を参考にしてください。

---

### インストール方法

S4EXPxxx.ZIP をダウンロードして展開し、S44EXP.X をパスの通ったディレクトリに置きます。

---

### 使用方法

- 内蔵ADPCMで鳴らす場合

pcm8a.x を常駐させて、

    s44exp <PCMファイル名>


- Mercury-UNITで鳴らす場合

pcm8pp.x を常駐させて、

    s44exp <PCMファイル名>


- KMD歌詞表示を行う場合

pcm8a.x または pcm8pp.x を常駐させて、
全画面、アートワーク輝度70% なら

    s44exp -x -t70 <PCMファイル名>


060loadhigh.x を使ったハイメモリ上での実行に対応しています。

pcm8a.x 無しでも内蔵ADPCMでの再生は可能ですが、16bitPCMデータでKMD歌詞表示を行う場合は歌詞の遅れが目立つことがあります。

---

### Special Thanks

* xdev68k thanks to ファミべのよっしんさん
* HAS060.X on run68mac thanks to YuNKさん / M.Kamadaさん / GOROmanさん
* HLK301.X on run68mac thanks to SALTさん / GOROmanさん

---

### History

* 1.1.1 (2023/04/11) ... PCM8A.X 無しだと X68000Z EAK 1.1.3 で正常に再生できない症状への対策を行なった
* 1.1.0 (2023/04/01) ... 初版
