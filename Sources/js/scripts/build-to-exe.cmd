@echo off

rem 스크립트 실행 위치와 무관하게 Sources/js를 기준으로 동작
pushd "%~dp0.."

echo [Build] Electron(Portable) 기반 단일 EXE 빌드를 시작합니다.

set NODE_ENV=production

call npm run build:electron

if errorlevel 1 (
    echo [Electron] EXE 생성이 실패했습니다.
    exit /b 1
)

echo [Complete] 단일 EXE 빌드가 완료되었습니다! (dist\BackBoard.exe)

pause

popd
