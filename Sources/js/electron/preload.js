const {contextBridge, ipcRenderer} = require('electron');

// Electron API를 안전하게 렌더러 프로세스에 노출
contextBridge.exposeInMainWorld('electronAPI', {
    // 프로젝트 디렉토리 가져오기
    getProjectDirectory: () => ipcRenderer.invoke('get-project-directory'),

    // Electron 환경 여부
    isElectron: true,

    // 개발(DEV) 여부 확인
    isDev: process.env.NODE_ENV === 'development'
});
