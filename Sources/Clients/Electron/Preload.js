const {contextBridge, ipcRenderer} = require('electron');

// Electron API를 안전하게 렌더러 프로세스에 노출
contextBridge.exposeInMainWorld('electronAPI', {
    // Electron 환경 여부
    isElectron: true,

    // 개발(DEV) 여부 확인
    isDev: process.env.NODE_ENV === 'development',

    // 창 제어
    minimize: () => ipcRenderer.send('window-minimize'),
    toggleMaximize: () => ipcRenderer.send('window-maximize'),
    close: () => ipcRenderer.send('window-close'),
    isMaximized: () => ipcRenderer.invoke('window-is-maximized'),

    // 창 상태 변경 이벤트 수신
    onWindowMaximized: (cb) => ipcRenderer.on('window-maximized', () => cb()),
    onWindowUnmaximized: (cb) => ipcRenderer.on('window-unmaximized', () => cb()),

    // 파일/폴더 선택 다이얼로그
    selectPath: (mode) => ipcRenderer.invoke('select-path', mode),

    // 네이티브 확인 다이얼로그
    showConfirm: (opts) => ipcRenderer.invoke('show-confirm', opts)
});
