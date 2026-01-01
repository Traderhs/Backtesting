const {app, BrowserWindow, ipcMain, dialog} = require('electron');
const path = require('path');
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
try {
    // DEV와 EXE를 구분해 고유 태그 사용
    const baseUserData = app.getPath('userData');
    const instanceTag = isDev ? `dev-${process.pid}` : `exe-${process.pid}`;
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

    const serverPath = path.join(__dirname, '../server/launch.js');

    // launch.js는 자체적으로 main()을 호출함
    require(serverPath);
    serverStarted = true;
}

// 메인 윈도우 생성
function createWindow() {
    const iconPath = isDev
        ? path.join(__dirname, '../icons/backboard.ico') :
        path.join(process.resourcesPath, 'Icons/backboard.ico');

    mainWindow = new BrowserWindow({
        width: 1400,
        height: 900,
        minWidth: 800,
        minHeight: 600,

        fullscreenable: false, // 진짜 fullscreen 금지
        maximizable: false,    // 버튼으로 최대화 금지
        resizable: false,

        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
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

    // 즉시 앱 로드
    const targetURL = isDev ? `http://localhost:5173` : `http://localhost:${port}`;
    mainWindow.loadURL(targetURL).then();

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

    // 프로젝트 폴더 선택
    const result = await dialog.showOpenDialog({
        properties: ['openDirectory'],
        title: 'Select Project Directory',
        buttonLabel: 'Select Project Folder',
        message: 'Select the project folder where BackBoard/ directory is located.'
    });

    if (result.canceled || result.filePaths.length === 0) {
        app.quit();
        return;
    }

    process.env.PROJECT_DIR = result.filePaths[0];

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
