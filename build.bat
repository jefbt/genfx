cls

@echo off
setlocal ENABLEDELAYEDEXPANSION

REM === Encerrar processo do app e limpar pastas antes da build ===
REM Tenta matar o executavel se estiver rodando (ignora erros/saida)
taskkill /IM genfx.exe /F >nul 2>&1

REM Apaga as pastas de build e de pacotes do CPack (ignora erros/saida)
rmdir /s /q "build" >nul 2>&1
rmdir /s /q "_CPack_Packages" >nul 2>&1

set VCPKG_ROOT=C:\Users\JefGod\Documents\Jef\Dev\vcpkg

REM Configure these paths as needed
if not defined VCPKG_ROOT (
  echo VCPKG_ROOT nao definido. Defina a variavel de ambiente ou edite este script.
  exit /b 1
)

REM Ajuste este caminho para sua pasta do FFmpeg (onde estao ffmpeg.exe e ffprobe.exe)
set "FF_DIR=C:\Users\JefGod\Documents\Jef\Apps\FFMPEG\bin"

set "BUILD_DIR=build\vs"

echo(
echo === Configurando CMake (Release, x64, static, vcpkg) ===
echo Usando toolchain do vcpkg em: "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

rem Limpar cache antigo que pode fixar outra toolchain
if exist "%BUILD_DIR%\CMakeCache.txt" del /q "%BUILD_DIR%\CMakeCache.txt"
if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DFF_DIR="%FF_DIR%"
if errorlevel 1 exit /b %errorlevel%

echo(
echo === Compilando (Release) ===
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

set "OUT_DIR=%BUILD_DIR%\Release"

echo(
echo === Conteudo da pasta de release ===
if exist "%OUT_DIR%" (
  dir /b "%OUT_DIR%"
  echo(
  echo Dica: os executaveis e assets foram copiados para "%OUT_DIR%". FFmpeg foi copiado de "%FF_DIR%" se presente.
) else (
  echo Pasta de saida nao encontrada: %OUT_DIR%
)

endlocal
