
# Building on Windows

This software can be built using Visual Studio 2019 and Qt 5.15

## Required Software

### Visual Studio Community 2019
- https://visualstudio.microsoft.com/ja/vs/older-downloads/
- During the installation, make sure to select all the Visual C++ packages.

### CMake
- https://cmake.org/download/
- This will be used to create the `MSVC 2019` project file.

### Qt
- https://www.qt.io/download-open-source/
- Qt is a cross-platform GUI framework.
- Install Qt 5.15.2 64-bit version with the [Qt Online Installer for Windows](http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe).

#### Customized Qt v5.15.2 with WinTab support
- The customized Qt5.15.2 are made with cherry-picking the WinTab feature to be officially introduced from 6.0.
- You can build OT with WinTab support by using the prebuilt package of the customized version of Qt for MSVC2019-x64 provided [here](https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab).

## Acquiring the Source Code
- Note: You can also perform these next commands with Github for Desktop client.
- Clone the base repository.
- Throughout the explanation `$iwawarper` will represent the root for the base repository.
- Visual Studio cannot recognize UTF-8 without BOM source code properly. Furthermore, since the endline character is represented with only the LF character, one line comments in Japanese will often cause the following line to be treated as a comment by `MSVS` as well.
- In order to prevent this, please change the following setting in git so that it will preserve the proper endline characters:
- `git config core.safecrlf true`

### lib and dll
- `lib` and `dll` files are tracked by [Git Large File Storage](https://git-lfs.github.com/).
- Note: git-lfs is also installed with Github Desktop.
- Execute `git lfs pull` after `git clone` by using the lfs client.

### Using CMake to Create a Visual Studio Project
- Launch CMake
- In `Where is the source code`, navigate to `$iwawarper/sources`
- In `Where to build the binaries`, navigate to `$iwawarper/build`
- Or to wherever you usually build to.
- If the build directory is in the git repository, be sure to add the directory to .gitignore
- If the build directory is different from the one above, be sure to change to the specified directory where appropriate below.
-Click on Configure and select the version of Visual Studio you are using.
- If Qt was installed to a directory other than the default, and the error Specify QTDIR properly appears, navigate to the `QTDIR` install folder and specify the path to `msvc2019_64`. Rerun Configure.
- If red lines appear in the bottom box, you can safely ignore them.
- Click Generate
- Should the CMakeLists.txt file change, such as during automatic build cleanup, there is no need to rerun CMake.

## Building
- Open `$iwawarper/build/IwaWarper.sln` and change to `Debug` or `Release` in the top bar.
- Compile the build.
- The output will be in the corresponding folder in `$iwawarper/build/`

## Running the Program
### Setting Up the Program's Path
1. Copy the entire contents of `$iwawarper/build/Release` to an appropriate folder.

1. Copy all files under `$iwawarper/thirdparty/OpenToonz/bin` to the same folder as `IwaWarper.exe`

1. Open a Command Prompt and navigate to `QTDIR/msvc2015_64/bin`. Run the Qt program `windeployqt.exe` with the path for `IwaWarper.exe`, `image.dll` and `toonzlib.dll` as an argument. (Another way to do this is navigate to the exe that was created in your Release folder and drag and drop the IwaWarper.exe + dlls and on top of the windeployqt.exe This will automatically generate the QT files and folders you will need.)
    - The necessary Qt library files should be in the same folder as `IwaWarper.exe`

1. Copy the following file(s) to the same folder as `IwaWarper.exe`
      - `$opentoonz/thirdparty/glut/3.7.6/lib/glut64.dll`
  
### Prepare conf.ini (stuff folder location setting)

- Copy `$iwawarper/doc/conf.ini` to the same folder as `IwaWarper.exe`. The file specifies relative path to the stuff folder (the stuff folder is located at `$iwawarper/stuff` by deafault) . If `IwaWarper.exe` is not located in `$iwawarper/build/Release` or you would like to move the location of the stuff folder, change the value of `IWSTUFFROOT` in `conf.ini` to appropriate relative path.

### Running
`IwaWarper.exe` can now be run.  Congratulations!

## Creating Translation Files
Qt translation files are generated first from the source code to .ts files, then from .ts files to a .qm file.  These files can be created in Visual Studio if the `translation_` project and `Build translation_??? only` (`translation_???`のみをビルド) is used.  These files are not created in the default `Build Project Solution`.
