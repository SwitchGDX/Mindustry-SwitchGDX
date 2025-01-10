![Logo](core/assets-raw/sprites/ui/logo.png)

[![Build Status](https://github.com/Anuken/Mindustry/workflows/Tests/badge.svg?event=push)](https://github.com/Anuken/Mindustry/actions)
[![Discord](https://img.shields.io/discord/391020510269669376.svg?logo=discord&logoColor=white&logoWidth=20&labelColor=7289DA&label=Discord&color=17cf48)](https://discord.gg/mindustry)  

The automation tower defense RTS, written in Java.

_[Trello Board](https://trello.com/b/aE2tcUwF/mindustry-40-plans)_  
_[Wiki](https://mindustrygame.github.io/wiki)_  
_[Javadoc](https://mindustrygame.github.io/docs/)_ 

## SwitchGDX
The [SwitchGDX](https://github.com/TheLogicMaster/switch-gdx) backend enables support for more portable
target platforms by transpiling to C++ using [Clearwing VM](https://github.com/TheLogicMaster/clearwing-vm).
The primary target is a Nintendo Switch Homebrew application.

### Limitations
- Performance is not amazing
- No multiplayer/networking support
- Poor mod support
- No controller support

### Mods
- This port targets the development version of Mindustry, so mod compatibility is poor ([Asthosus](https://github.com/Catana791/Asthosus) is known working)
- JavaScript mods currently have issues
- Mods must be extracted into `mods` directory due to a runtime bug with ZipFile extraction or string encoding
- Java mods must be extracted into `Mindustry/switch/mods` to be included in the built program (No JIT support)
and have relevant class packages added to the `switch.json` `reflective` section to prevent classes being optimized out.

### Windows
- Install MSYS2
- Open a mingw64 shell: `C:\msys64\msys2_shell.cmd -mingw64`
- Install dependencies: `pacman -S gcc git rsync texinfo mingw-w64-x86_64-cmake mingw-w64-x86_64-zziplib mingw-w64-x86_64-glew mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-freetype mingw-w64-x86_64-bullet`
- Install [devkitPro Updater](https://github.com/devkitPro/installer/releases/latest) with Switch packages selected (Leave downloaded files)
- Open DevKitPro's MSYS2: `C:\devkitPro\msys2\msys2_shell.cmd -mingw64`
- Install dependencies: `pacman -S switch-zlib switch-zziplib switch-sdl2_mixer switch-libvorbis switch-freetype switch-glad switch-curl dkp-toolchain-vars texinfo`
- Build LibFFI for Switch

### Linux
- Install CMake, Ninja, Rsync, Texinfo, SDL2, SDL2_Mixer, GLEW, libffi, zlib, zziplib, freetype
- With APT: `sudo apt install build-essential texinfo rsync cmake ninja-build libffi-dev libzzip-dev libsdl2-dev libsdl2-mixer-dev zlib1g-dev libglew-dev libfreetype-dev libcurl4-gnutls-dev`
- Install [devkitPro pacman](https://github.com/devkitPro/pacman/releases/tag/v1.0.2)
- `dkp-pacman -S switch-zlib switch-sdl2 switch-sdl2_mixer switch-freetype switch-glad switch-curl switch-bulletphysics dkp-toolchain-vars`
- Build LibFFI for Switch

### Switch Homebrew
- The `NRO` task builds a Homebrew application
- The `Deploy` task uses NXLink to deploy to a Switch running the Homebrew Launcher

### UWP
- Gets in-game but has awful load times and performance, plus no controller support, so probably not useful
- Install CMake for Windows
- Install Visual Studio 2022 and C++/UWP support (`Desktop development with C++`, `Windows application development`)
- Run twice for DLLs to properly be copied for some reason

### LibFFI
This is a library that has to be compiled and installed manually for Switch. Run this for Linux normally and on Windows under MSYS2. Ensure that the working directory doesn't contain any spaces.
```bash
git clone https://github.com/libffi/libffi.git
cd libffi
./autogen.sh
source $DEVKITPRO/switchvars.sh
source $DEVKITPRO/devkita64.sh
CFLAGS="-g -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec" CHOST=aarch64-none-elf ./configure --prefix="$DEVKITPRO/portlibs/switch" --host=aarch64-none-elf
make install
```

## Contributing

See [CONTRIBUTING](CONTRIBUTING.md).

## Building

Bleeding-edge builds are generated automatically for every commit. You can see them [here](https://github.com/Anuken/MindustryBuilds/releases).

If you'd rather compile on your own, follow these instructions.
First, make sure you have [JDK 17](https://adoptium.net/archive.html?variant=openjdk17&jvmVariant=hotspot) installed. **Other JDK versions will not work.** Open a terminal in the Mindustry directory and run the following commands:

### Windows

_Running:_ `gradlew desktop:run`  
_Building:_ `gradlew desktop:dist`  
_Sprite Packing:_ `gradlew tools:pack`

### Linux/Mac OS

_Running:_ `./gradlew desktop:run`  
_Building:_ `./gradlew desktop:dist`  
_Sprite Packing:_ `./gradlew tools:pack`

### Server

Server builds are bundled with each released build (in Releases). If you'd rather compile on your own, replace 'desktop' with 'server', e.g. `gradlew server:dist`.

### Android

1. Install the Android SDK [here.](https://developer.android.com/studio#command-tools) Make sure you're downloading the "Command line tools only", as Android Studio is not required.
2. In the unzipped Android SDK folder, find the cmdline-tools directory. Then create a folder inside of it called `latest` and put all of its contents into the newly created folder.
3. In the same directory run the command `sdkmanager --licenses` (or `./sdkmanager --licenses` if on linux/mac)
4. Set the `ANDROID_HOME` environment variable to point to your unzipped Android SDK directory.
5. Enable developer mode on your device/emulator. If you are on testing on a phone you can follow [these instructions](https://developer.android.com/studio/command-line/adb#Enabling), otherwise you need to google how to enable your emulator's developer mode specifically.
6. Run `gradlew android:assembleDebug` (or `./gradlew` if on linux/mac). This will create an unsigned APK in `android/build/outputs/apk`.

To debug the application on a connected device/emulator, run `gradlew android:installDebug android:run`.

### Troubleshooting

#### Permission Denied

If the terminal returns `Permission denied` or `Command not found` on Mac/Linux, run `chmod +x ./gradlew` before running `./gradlew`. *This is a one-time procedure.*

---

Gradle may take up to several minutes to download files. Be patient. <br>
After building, the output .JAR file should be in `/desktop/build/libs/Mindustry.jar` for desktop builds, and in `/server/build/libs/server-release.jar` for server builds.

## Feature Requests

Post feature requests and feedback [here](https://github.com/Anuken/Mindustry-Suggestions/issues/new/choose).

## Downloads

| [![](https://static.itch.io/images/badge.svg)](https://anuke.itch.io/mindustry)    |    [![](https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png)](https://play.google.com/store/apps/details?id=io.anuke.mindustry)   |    [![](https://fdroid.gitlab.io/artwork/badge/get-it-on.png)](https://f-droid.org/packages/io.anuke.mindustry)	| [![](https://flathub.org/assets/badges/flathub-badge-en.svg)](https://flathub.org/apps/details/com.github.Anuken.Mindustry)  
|---	|---	|---	|---	|
