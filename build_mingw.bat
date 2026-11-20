@echo off
setlocal
REM Build AutoTheme.dll with MinGW-w64 (x64, statically linked)
REM Default toolchain: F:\Tools\App\mingw64 ; falls back to tools in PATH

set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "OUTDIR=%ROOT%bin\Release\x64"
set "GXX=F:\Tools\App\mingw64\bin\g++.exe"
if not exist "%GXX%" set "GXX=g++"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

"%GXX%" -std=c++17 -Os -flto -fno-exceptions -fno-rtti ^
  -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--wrap=__cxa_demangle ^
  -DNDEBUG -DWIN32 -DUNICODE -D_UNICODE ^
  -shared -static -static-libgcc -static-libstdc++ -s ^
  -I"%SRC%\core" -I"%SRC%\net" -I"%SRC%\ui" -I"%SRC%\sdk" ^
  "%SRC%\core\AutoThemePlugin.cpp" "%SRC%\core\ThemeConfig.cpp" "%SRC%\core\Solar.cpp" ^
  "%SRC%\core\DemangleStub.cpp" ^
  "%SRC%\net\Http.cpp" "%SRC%\net\Util.cpp" "%SRC%\net\Location.cpp" ^
  "%SRC%\ui\OptionsDlg.cpp" ^
  -lwinhttp -ladvapi32 -luser32 -lgdi32 ^
  -o "%OUTDIR%\AutoTheme.dll"

if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo BUILD OK: %OUTDIR%\AutoTheme.dll