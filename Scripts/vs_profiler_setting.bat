@echo off
:: 관리자 권한 확인
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo 이 스크립트는 관리자 권한이 필요합니다.
    echo 마우스 우클릭 후 "관리자 권한으로 실행"을 선택하세요.
    pause
    exit /b 1
)

echo ====================================
echo Visual Studio 프로파일러 임시 폴더 심볼릭 링크 생성
echo ====================================
echo.

:: 변수 설정
set "TEMP_DH=%TEMP%\dh"
set "TARGET_DH=E:\dh"

:: E 드라이브에 dh 폴더가 없으면 생성
if not exist "%TARGET_DH%" (
    echo E:\dh 폴더 생성 중...
    mkdir "%TARGET_DH%"
)

:: 기존 TEMP\dh가 있으면 처리
if exist "%TEMP_DH%" (
    echo 기존 %TEMP_DH% 발견
    
    :: 심볼릭 링크인지 확인
    fsutil reparsepoint query "%TEMP_DH%" >nul 2>&1
    if %errorlevel% equ 0 (
        echo 이미 심볼릭 링크입니다. 삭제 후 재생성합니다.
        rmdir "%TEMP_DH%"
    ) else (
        echo 일반 폴더입니다. 백업 후 삭제합니다.
        set "BACKUP_NAME=%TEMP%\dh_backup_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%"
        set "BACKUP_NAME=!BACKUP_NAME: =0!"
        echo 백업 위치: !BACKUP_NAME!
        move "%TEMP_DH%" "!BACKUP_NAME!"
    )
)

:: 심볼릭 링크 생성
echo.
echo 심볼릭 링크 생성 중...
echo %TEMP_DH% ==^> %TARGET_DH%
mklink /D "%TEMP_DH%" "%TARGET_DH%"

if %errorlevel% equ 0 (
    echo.
    echo ====================================
    echo 성공! 심볼릭 링크가 생성되었습니다.
    echo ====================================
    echo TEMP\dh 폴더에 저장되는 모든 파일은
    echo 실제로 E:\dh에 저장됩니다.
    echo.
) else (
    echo.
    echo 오류: 심볼릭 링크 생성 실패
    echo.
)

pause