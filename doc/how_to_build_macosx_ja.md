# MacOSX での開発環境構築

## 必要なソフトウェア

- git
- brew
- Xcode
- cmake (3.2.2以降)
- Qt5 (5.15以降)

## ビルド手順

### Xcode をインストール

### Homebrew をインストール

1. ターミナルウィンドウを起動
2. 下記を実行します：
```
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

### brew で必要なパッケージをインストール

```
$ brew install cmake git-lfs qt@5
$ git lfs install
```

### リポジトリを clone

```
$ git clone https://github.com/opentoonz/iwawarper
$ cd iwawarper
$ git lfs pull
```

### 本体のビルド

1. buildディレクトリの作成
```
$ cd ../../toonz
$ mkdir build
$ cd build
```

2. ビルド
コマンドラインの場合は下記を実行します。
```
$ cmake ../sources -DQTDIR='/usr/local/opt/qt@5'  #replace QT path with your installed QT version#
$ make
```
- Qt をHomebrewでなく http://download.qt.io/official_releases/qt/ からダウンロードして `/Users/ユーザ名/Qt` にインストールしている場合、`QT_PATH`の値は `~/Qt/5.15.2/clang_64/lib` または `~/Qt/5.15.2/clang_32/lib` のようになります。

Xcodeを用いる場合は下記を実行します。
```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
$ cmake -G Xcode ../sources -B. -DQTDIR='/usr/local/opt/qt@5' -DWITH_TRANSLATION=OFF
```
- オプション `-DWITH_TRANSLATION=OFF` はXcode12以降で必要です。
- Xcodeでプロジェクト `/Users/yourlogin/Documents/iwawarper/build/IwaWarper.xcodeproj` を開き、ビルドします。

### ライブラリの準備
1. `$iwawarper/thirdparty/OpenToonz/dylib` 内のファイルを全て `IwaWarper.app/Contents/MacOS` 内にコピーします。
2. 下記を実行します。
```
$ cd build #or build/Release or build/Debug, where IwaWarper.app has generated#
$ /usr/local/opt/qt@5/bin/macdeployqt IwaWarper.app -verbose=1 -always-overwrite -executable=IwaWarper.app/Contents/MacOS/libimage.dylib -executable=IwaWarper.app/Contents/MacOS/libtoonzlib.dylib
```
    - 必要なQtライブラリがIwaWarperバイナリと同じフォルダにコピーされます。
 
### conf.ini (stuffフォルダの場所を指定するファイル) の準備

- `$iwawarper/doc/conf.ini`　を `IwaWarper.app/Contents/MacOS` 内にコピーします。
- `$iwawarper/stuff` を `IwaWarper.app` 内にフォルダごとコピーします。
- ファイル `conf.ini` はstuffフォルダの場所を指定するものです。もしstuffフォルダの場所を他の場所に移す場合は、`conf.ini`に書かれている`IWSTUFFROOT`の値を対応するパスに書き換えてください。

### アプリケーションの実行

```
$ open ~/Documents/iwawarper/build/IwaWarper.app
```

- Xcode でビルドしている場合、アプリケーションは　`/Users/yourlogin/Documents/iwawarper/build/Debug/IwaWarper.app` にあります。