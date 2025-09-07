@echo off

set PATH=C:\Users\0908r\AppData\Roaming\JetBrains\WebStorm2025.1\node\versions\22.18.0;%PATH%

echo [Build] PKG 기반 단일 EXE 빌드를 시작합니다.

echo [Clean] 기존 빌드 파일을 정리합니다.
if exist Backboard rmdir /s /q Backboard
if exist dist rmdir /s /q dist

echo [React] React 앱 빌드를 시작합니다.
set NODE_ENV=production
call npm run build
if errorlevel 1 (
    echo [React] 빌드가 실패했습니다.
    exit /b 1
)

echo [PKG] 단일 EXE 파일 생성을 시작합니다.
call npm run pkg:build
if errorlevel 1 (
    echo [PKG] EXE 생성이 실패했습니다.
    exit /b 1
)

echo [Copy] Backboard 폴더를 복사합니다.
xcopy /E /I /H /Y Backboard dist\Backboard\

echo [Complete] PKG 빌드가 완료되었습니다!

pause
