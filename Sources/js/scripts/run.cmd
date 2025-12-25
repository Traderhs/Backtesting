@echo off
setlocal enabledelayedexpansion

rem 스크립트 실행 위치와 무관하게 Sources/js를 기준으로 동작
pushd "%~dp0.."

echo [Tailwind] 빌드 여부를 확인합니다.
set TW_SRC_TICKS=0
set OUT_TICKS=0
if not exist src\output.css (
  set NEEDS_TW_BUILD=true
  goto TW_DONE
)
for /f "delims=" %%A in ('powershell -NoProfile -Command "(Get-ChildItem src -Recurse -Include *.css,*.js,*.ts,*.jsx,*.tsx -File -Exclude output.css | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTimeUtc.Ticks"') do set TW_SRC_TICKS=%%A
for /f "delims=" %%B in ('powershell -NoProfile -Command "(Get-Item src\output.css).LastWriteTimeUtc.Ticks"') do set OUT_TICKS=%%B
call :CompareBigNumbers "!TW_SRC_TICKS!" "!OUT_TICKS!" RES
if "!RES!"=="GTR" (
  set NEEDS_TW_BUILD=true
) else (
  set NEEDS_TW_BUILD=false
)
:TW_DONE
if "%NEEDS_TW_BUILD%"=="true" (
  echo [Tailwind] 빌드를 시작합니다.
  if not exist node_modules\\.bin\\tailwindcss-cli.cmd (
    echo [Tailwind] node_modules가 없습니다. 먼저 npm install을 실행하세요.
    popd
    exit /b 1
  )
  call node_modules\\.bin\\tailwindcss-cli.cmd -c config\tailwind.config.js -i src\index.css -o src\output.css
  if errorlevel 1 (
    echo [Tailwind] 빌드가 실패했습니다.
    popd
    exit /b 1
  ) else (
    echo [Tailwind] 빌드가 완료되었습니다.
  )
) else (
  echo [Tailwind] 빌드를 스킵합니다.
)

echo.
echo [npm] 빌드 여부를 확인합니다.
if not exist BackBoard (
  set NEEDS_NPM_BUILD=true
) else (
  set SRCB_TICKS=0
  for /f "delims=" %%C in ('powershell -NoProfile -Command "(Get-ChildItem src -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTimeUtc.Ticks"') do set SRCB_TICKS=%%C
  set BACKB_TICKS=0
  for /f "delims=" %%D in ('powershell -NoProfile -Command "(Get-ChildItem BackBoard -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTimeUtc.Ticks"') do set BACKB_TICKS=%%D
  call :CompareBigNumbers "!SRCB_TICKS!" "!BACKB_TICKS!" RES
  if "!RES!"=="GTR" (
    set NEEDS_NPM_BUILD=true
  ) else (
    set NEEDS_NPM_BUILD=false
  )
)
if "%NEEDS_NPM_BUILD%"=="true" (
  echo [npm] 빌드를 시작합니다.
  call npm run build
  if errorlevel 1 (
    echo [npm] 빌드가 실패했습니다.
    exit /b 1
  ) else (
    echo [npm] 빌드가 완료되었습니다.
  )
) else (
  echo [npm] 빌드를 스킵합니다.
)

echo.
echo [Launch] 기존 실행 중인 node 프로세스를 확인합니다.
rem launch.js 를 포함하는 node 프로세스 종료 (이미 실행 중이면)
for /f "skip=1 tokens=1" %%p in ('wmic process where "CommandLine like '%%launch.js%%'" get ProcessId') do (
  set "pid=%%p"
  if not "!pid!"=="" (
    rem pid 앞글자가 숫자인지 확인 (빈 줄이나 헤더 무시)
    set "first=!pid:~0,1!"
    if "!first!" GEQ "0" if "!first!" LEQ "9" (
      echo [Launch] 기존 프로세스 PID: !pid! 종료 중...
      taskkill /F /PID !pid! >nul 2>&1
    )
  )
)

echo.
echo [Launch] 앱을 실행합니다.
start "" /B node launch.js

popd

exit /b 0

:CompareBigNumbers
set "_A=%~1"
set "_B=%~2"
set "_RES="
set "_A=%_A: =%"
set "_B=%_B: =%"
set LEN_A=0
set LEN_B=0
for /f "delims=" %%x in ('echo %_A%^|find /v /c ""') do set /a LEN_A=%%x
for /f "delims=" %%y in ('echo %_B%^|find /v /c ""') do set /a LEN_B=%%y
if %LEN_A% GTR %LEN_B% ( set "_RES=GTR" & goto :done )
if %LEN_A% LSS %LEN_B% ( set "_RES=LSS" & goto :done )
if "%_A%"=="%_B%" ( set "_RES=EQU" & goto :done )
if "%_A%" GTR "%_B%" ( set "_RES=GTR" ) else ( set "_RES=LSS" )
:done
set "%3=%_RES%"
goto :eof
