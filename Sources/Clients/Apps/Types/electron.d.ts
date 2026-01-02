// Electron API 타입 정의
export interface ElectronAPI {
    getProjectDirectory: () => Promise<string>;
    isElectron: boolean;
    isDev: boolean;

    // 창 제어
    minimize: () => void;
    toggleMaximize: () => void;
    close: () => void;
    isMaximized: () => Promise<boolean>;
    onWindowMaximized: (cb: () => void) => void;
    onWindowUnmaximized: (cb: () => void) => void;
}

declare global {
    interface Window {
        electronAPI?: ElectronAPI;
    }
}

export {};
