@echo off

rem 스크립트 실행 위치와 무관하게 Sources/js를 기준으로 동작
pushd "%~dp0.."

echo [Build] PKG 기반 단일 EXE 빌드를 시작합니다.

echo [Clean] 기존 빌드 파일을 정리합니다.
if exist BackBoard rmdir /s /q BackBoard
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

echo [Copy] BackBoard 폴더를 복사합니다.
xcopy /E /I /H /Y BackBoard dist\BackBoard\

echo [Complete] PKG 빌드가 완료되었습니다!

pause

popd
