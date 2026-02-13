const {app, BrowserWindow, ipcMain, dialog, Menu} = require('electron');
const path = require('path');
const fs = require('fs');
const net = require('net');
const {autoUpdater} = require('electron-updater');

// 전역 변수
let port = 7777;
let mainWindow;
let serverStarted = false;
let projectDirectory = null;  // 사용자가 선택한 프로젝트 디렉토리

// 개발 환경 감지 — 메인 프로세스에서는 window가 없으므로 NODE_ENV 또는 패키지 여부를 사용
const isDev = !app.isPackaged;

if (!process.env.NODE_ENV) {
    process.env.NODE_ENV = isDev ? 'development' : 'production';
}

// 개발/프로덕션 인스턴스 간 Chromium 캐시 충돌 방지
const baseUserData = app.getPath('userData'); // 설정 파일용 기본 경로

try {
    // DEV와 EXE를 구분해 고유 태그 사용
    const instanceTag = isDev ? 'dev' : 'exe';
    const instanceUserData = path.join(baseUserData, instanceTag);

    app.setPath('userData', instanceUserData);
    app.commandLine.appendSwitch('disk-cache-dir', path.join(instanceUserData, 'Cache'));
} catch (e) {
    // 기본 경로 사용
    console.warn('Failed to set isolated userData/disk-cache-dir:', e);
}

// 포트 사용 가능 여부 확인
function checkPortAvailable(port) {
    return new Promise((resolve) => {
        const server = net.createServer();

        server.once('error', () => resolve(false));

        server.once('listening', () => {
            server.close();
            resolve(true);
        });

        server.listen(port);
    });
}

// 사용 가능한 포트 찾기
async function findAvailablePort(startPort) {
    let currentPort = startPort;
    const maxPort = startPort + 100;

    while (!(await checkPortAvailable(currentPort))) {
        currentPort++;

        if (currentPort > maxPort) {
            throw new Error('No available ports found');
        }
    }

    return currentPort;
}

// 서버 프로세스 시작
function startServer() {
    if (serverStarted) {
        return;
    }

    // 서버 실행 환경 변수 설정
    process.env.BACKBOARD_PORT = port.toString();
    process.env.BACKBOARD_OPEN_BROWSER = 'none';
    process.env.ELECTRON_RUN = 'true';
    process.env.ELECTRON_PROJECT_DIR = projectDirectory;

    const serverPath = path.join(__dirname, '../Servers/Launch.js');

    // launch.js는 자체적으로 main()을 호출함
    require(serverPath);
    serverStarted = true;
}

// 메인 윈도우 생성
function createWindow() {
    const iconPath = isDev
        ? path.join(__dirname, '../Bundles/Icons/backboard.ico') :
        path.join(process.resourcesPath, 'Icons/backboard.ico');

    mainWindow = new BrowserWindow({
        title: 'BackBoard',
        fullscreenable: false, // 진짜 fullscreen 금지
        maximizable: true,     // 최대화 허용(프레임리스에서 토글 가능)
        resizable: true,
        frame: false,          // 프레임리스 창으로 커스텀 타이틀바 사용

        webPreferences: {
            preload: path.join(__dirname, 'Preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
            webSecurity: true
        },
        icon: iconPath,
        backgroundColor: '#111111',
        show: false  // 준비될 때까지 숨김
    });

    // 페이지 완전히 로드되면 즉시 표시
    mainWindow.once('ready-to-show', () => {
        mainWindow.show();

        // 창 뜨면 무조건 최대화
        mainWindow.maximize();
    });

    // 프레임리스 창의 경우 최대화/복원 이벤트를 렌더러에 전달
    mainWindow.on('maximize', () => {
        try {
            mainWindow.webContents.send('window-maximized');
        } catch (e) {
            // 무시
        }
    });

    mainWindow.on('unmaximize', () => {
        try {
            mainWindow.webContents.send('window-unmaximized');
        } catch (e) {
            // 무시
        }
    });

    // 즉시 앱 로드
    const targetURL = isDev ? `http://localhost:5173` : `http://localhost:${port}`;
    mainWindow.loadURL(targetURL).then();

    // 개발 환경에서는 키 단축키(F12, Ctrl/Cmd+Shift+I)로 DevTools를 열 수 있게 허용
    try {
        if (isDev) {
            mainWindow.webContents.on('before-input-event', (event, input) => {
                const key = input.key;
                const ctrl = input.control || input.meta;
                const shift = input.shift;

                // F12 또는 Ctrl/Cmd + Shift + I
                if (key === 'F12' || (ctrl && shift && (key === 'I' || key === 'i'))) {
                    try {
                        mainWindow.webContents.openDevTools({mode: 'right'});
                    } catch (e) {
                        console.warn('Failed to open DevTools in dev:', e);
                    }

                    // 렌더러의 preventDefault를 무력화 하기 위해 이벤트는 취소
                    event.preventDefault();
                }
            });
        } else {
            // 프로덕션에서는 키 입력으로 DevTools가 열리지 않도록 차단
            mainWindow.webContents.on('before-input-event', (event, input) => {
                const key = input.key;
                const ctrl = input.control || input.meta;
                const shift = input.shift;

                if (key === 'F12' || (ctrl && shift && (key === 'I' || key === 'i'))) {
                    try {
                        if (mainWindow.webContents.isDevToolsOpened()) {
                            mainWindow.webContents.closeDevTools();
                        }
                    } catch (e) {
                        // 무시
                    }

                    event.preventDefault();
                }
            });
        }
    } catch (e) {
        console.warn('Failed to bind before-input-event for DevTools:', e);
    }

    // 로드 실패 시 재시도
    let retryCount = 0;
    mainWindow.webContents.on('did-fail-load', (event, errorCode) => {
        if ((errorCode === -102 || errorCode === -3) && retryCount < 10) {
            retryCount++;
            setTimeout(() => {
                mainWindow.loadURL(targetURL).then();
            }, 100);
        }
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

// 앱 준비 완료 시 실행
app.whenReady().then(async () => {
    // 업데이트 확인
    try {
        await autoUpdater.checkForUpdatesAndNotify();
    } catch (e) {
        console.warn('Update check failed:', e);
    }

    // 기본 메뉴 제거 (Windows 기본 'File Edit View...' 제거)
    try {
        Menu.setApplicationMenu(null);
    } catch (e) {
        console.warn('Failed to hide application menu:', e);
    }

    // 저장된 프로젝트 폴더 경로 확인
    const configPath = path.join(baseUserData, 'config.json');
    let savedProjectDir = null;

    // 저장된 설정 읽기
    try {
        if (fs.existsSync(configPath)) {
            const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
            savedProjectDir = config.projectDirectory;

            // 저장된 경로가 유효한지 확인
            if (savedProjectDir && fs.existsSync(savedProjectDir)) {
                process.env.PROJECT_DIR = savedProjectDir;
                projectDirectory = savedProjectDir; // 전역 변수에도 설정
            } else {
                savedProjectDir = null;
            }
        }
    } catch (e) {
        savedProjectDir = null;
    }

    // 저장된 경로가 없을 때만 프로젝트 폴더 선택 다이얼로그 표시
    if (!savedProjectDir) {
        const result = await dialog.showOpenDialog({
            properties: ['openDirectory'],
            title: '프로젝트 폴더 선택',
            buttonLabel: '폴더 선택'
        });

        if (result.canceled || result.filePaths.length === 0) {
            app.quit();
            return;
        }

        process.env.PROJECT_DIR = result.filePaths[0];
        projectDirectory = result.filePaths[0];

        // 선택한 프로젝트 폴더 저장
        try {
            const config = {projectDirectory: result.filePaths[0]};
            fs.writeFileSync(configPath, JSON.stringify(config, null, 2), 'utf8');
        } catch (e) {
            // 무시
        }
    }

    // 사용 가능한 포트 찾기 (기본 7777부터 시작)
    try {
        port = await findAvailablePort(7777);
    } catch (e) {
        console.error('Failed to find available port:', e);

        app.quit();
        return;
    }

    // 서버 시작
    startServer();

    // 윈도우 생성
    createWindow();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) {
            createWindow();
        }
    });
});

// 모든 윈도우 닫힘
app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

ipcMain.handle('get-project-directory', () => {
    return projectDirectory;
});

// 윈도우 제어용 IPC
ipcMain.on('window-minimize', () => {
    if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window-maximize', () => {
    if (!mainWindow) {
        return;
    }

    if (mainWindow.isMaximized()) {
        mainWindow.unmaximize();
    } else {
        mainWindow.maximize();
    }
});

ipcMain.on('window-close', () => {
    if (mainWindow) {
        mainWindow.close();
    }
});

ipcMain.handle('window-is-maximized', () => {
    return mainWindow ? mainWindow.isMaximized() : false;
});

// 파일/폴더 선택 다이얼로그
ipcMain.handle('select-path', async (event, mode) => {
    const properties = (mode === 'directory') ? ['openDirectory'] : ['openFile'];

    return await dialog.showOpenDialog(mainWindow, {
        properties: properties
    });
});

// 저장된 설정 초기화 후 앱 재시작 시 프로젝트 폴더 재선택
ipcMain.handle('reset-project-folder', async () => {
    try {
        const configPath = path.join(baseUserData, 'config.json');

        // 설정 파일 삭제
        if (fs.existsSync(configPath)) {
            fs.unlinkSync(configPath);
        }

        // 앱 재시작
        app.relaunch();
        app.quit();

        return {success: true};
    } catch (e) {
        return {success: false, error: e.message};
    }
});

// 간단한 네이티브 확인(Yes/No) 다이얼로그
ipcMain.handle('show-confirm', async (event, {title, message}) => {
    console.log('[Main] show-confirm', {title, message});

    const result = await dialog.showMessageBox(mainWindow, {
        type: 'question',
        buttons: ['확인', '취소'],
        defaultId: 0,   // 확인이 기본 포커스
        cancelId: 1,    // Esc 또는 취소 시 1번 인덱스 반환
        noLink: true,   // Windows에서 버튼을 링크 스타일로 보이지 않게 함
        normalizeAccessKeys: true,
        title: title || 'BackBoard',
        message: message || ''
    });

    // '확인' 버튼(인덱스 0)을 눌렀으면 true 반환
    return result.response === 0;
});
