import {defineConfig} from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
    plugins: [react()],
    base: './',
    build: {
        outDir: 'BackBoard',
        minify: 'terser',
        terserOptions: {
            compress: {
                drop_console: true,
                drop_debugger: true,
            },
        },
        rollupOptions: {
            output: {
                manualChunks: {
                    vendor: ['react', 'react-dom'],
                    charts: ['lightweight-charts', 'react-chartjs-2', 'chart.js'],
                    plotly: ['react-plotly.js'],
                    ui: ['@mui/material', '@emotion/react', '@emotion/styled'],
                },
            },
        },
        chunkSizeWarningLimit: 1000,
        sourcemap: true,
        reportCompressedSize: false,
    },
    esbuild: {
        drop: ['console', 'debugger'],
    },
    optimizeDeps: {
        include: ['react', 'react-dom'],
    },
    resolve: {
        alias: {
            '@': path.resolve(__dirname, '..', 'src'),
        }
    },
    server: {
        proxy: {
            "/BackBoard": {
                target: `http://localhost:${process.env.BACKBOARD_PORT || 7777}`,
                changeOrigin: true,
            },
            "/api": {
                target: `http://localhost:${process.env.BACKBOARD_PORT || 7777}`,
                changeOrigin: true,
            },
            "/force-shutdown": {
                target: `http://localhost:${process.env.BACKBOARD_PORT || 7777}`,
                changeOrigin: true,
            },
        },
    },
})
