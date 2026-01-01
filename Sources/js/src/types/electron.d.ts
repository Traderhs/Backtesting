// Electron API 타입 정의
export interface ElectronAPI {
    getProjectDirectory: () => Promise<string>;
    isElectron: boolean;
    isDev: boolean;
}

declare global {
    interface Window {
        electronAPI?: ElectronAPI;
    }
}

export {};
