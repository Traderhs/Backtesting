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

    // 파일/폴더 선택
    selectPath: (mode: 'file' | 'directory') => Promise<{ canceled: boolean; filePaths: string[] }>;

    // 프로젝트 폴더 재선택
    resetProjectFolder: () => Promise<{ success: boolean; error?: string }>;
}

declare global {
    interface Window {
        electronAPI?: ElectronAPI;
    }
}

export {};
