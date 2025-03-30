import {defineConfig} from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
    plugins: [react()],
    base: './',
    build: {
        outDir: 'Backboard'
    },
    resolve: {
        alias: {
            '@': path.resolve(__dirname, 'src'),
        }
    },
    server: {
        proxy: {
            "/Backboard": {
                target: "http://localhost:7777", // express 서버 포트
                changeOrigin: true,
            },
        },
    },
})
