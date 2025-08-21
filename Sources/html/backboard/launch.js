const path = require("path")
const { exec } = require("child_process")
const express = require("express")
const {readdir} = require("node:fs");
const app = express()
const port = 7777

// Backboard, Charts, Sources 폴더 전체를 정적으로 제공
app.use("/Backboard", express.static(path.join(process.cwd(), "Backboard")))
app.use("/Charts", express.static(path.join(process.cwd(), "Charts")))
app.use("/Sources", express.static(path.join(process.cwd(), "Sources")))

// 차트 파일 목록 제공
app.get("/chart-files", (req, res) => {
    const chartDir = path.join(process.cwd(), "Charts")
    readdir(chartDir, (err, files) => {
        if (err) {
            res.status(500).send("차트 폴더를 읽을 수 없습니다.")
            return
        }

        const htmlFiles = files.filter(f => f.endsWith(".html"))
        res.json(htmlFiles)
    })
})

// pkg 실행파일 기준으로, 리소스 폴더 위치
const distPath = path.join(process.cwd(), "Backboard")
app.use(express.static(distPath))

// 클라이언트 heartbeat를 위한 /ping 엔드포인트
let lastPing = Date.now()
app.get("/ping", (req, res) => {
    lastPing = Date.now()
    res.send("pong")
})

// 나머지 요청은 index.html로 라우팅
app.get("*", (req, res) => {
    res.sendFile(path.join(distPath, "index.html"))
})

// 서버 실행 및 브라우저 자동 열기
app.listen(port, () => {
    console.log(`Dashboard running at http://localhost:${port}`)
    // 기존 방식: 기본 브라우저 자동 실행 (exec 사용)
    exec(`start http://localhost:${port}`, (err) => {
        if (err) {
            console.error("브라우저 열기 실패:", err)
        }
    })
})

// heartbeat 체크: 1초마다 확인, 10초 이상 ping이 없으면 종료
setInterval(() => {
    if (Date.now() - lastPing > 10000) {
        console.log("No heartbeat received. Exiting...")
        process.exit()
    }
}, 1000)