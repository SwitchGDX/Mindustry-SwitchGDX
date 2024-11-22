if not defined DevEnvDir (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not exist ".\build-uwp" mkdir .\build-uwp
@REM if not exist "%USERPROFILE%\.SwitchGDX\" mkdir "%USERPROFILE%\.SwitchGDX"
@REM if not exist "%USERPROFILE%\.SwitchGDX\vcpkg\" (
@REM 	git clone -b 2022.10.19 https://github.com/Microsoft/vcpkg "%USERPROFILE%\.SwitchGDX\vcpkg"
@REM 	call "%USERPROFILE%\.SwitchGDX\vcpkg\bootstrap-vcpkg.bat"
@REM )

@REM "%USERPROFILE%\.SwitchGDX\vcpkg\vcpkg.exe" install angle:x64-uwp sdl2:x64-uwp libffi:x64-uwp curl:x64-uwp freetype:x64-uwp zziplib:x64-uwp zlib:x64-uwp libvorbis:x64-uwp libogg:x64-uwp mpg123:x64-uwp
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\vcpkg.exe" install angle:x64-uwp sdl2:x64-uwp libffi:x64-uwp curl:x64-uwp freetype:x64-uwp zziplib:x64-uwp zlib:x64-uwp libvorbis:x64-uwp libogg:x64-uwp mpg123:x64-uwp

@REM cmake -B build-uwp -S . -DVCPKG_ROOT="%USERPROFILE%\.SwitchGDX\vcpkg" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0"
cmake -B build-uwp -S . -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0"
cmake --open build-uwp
