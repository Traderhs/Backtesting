import { useEffect, useState } from "react"

interface ChartProps {
    filename?: string
}

export default function Chart({ filename }: ChartProps) {
    const [status, setStatus] = useState<"loading" | "valid" | "error">("loading")
    const [errorMessage, setErrorMessage] = useState<string | null>(null)

    useEffect(() => {
        const validateFile = async () => {
            if (!filename) {
                setStatus("error")
                setErrorMessage("차트 파일명이 지정되지 않았습니다.")
                return
            }

            try {
                const res = await fetch(`/Charts/${filename}.html`)
                if (!res.ok) {
                    setStatus("error")
                    setErrorMessage(`파일을 찾을 수 없습니다.`)
                    return
                }

                const reader = res.body?.getReader()
                if (!reader) {
                    setStatus("error")
                    setErrorMessage(`스트리밍을 지원하지 않습니다.`)
                    return
                }

                const decoder = new TextDecoder()
                let result = ""
                let lines: string[] = []

                while (lines.length < 4) {
                    const { done, value } = await reader.read()
                    if (done) break
                    result += decoder.decode(value, { stream: true })

                    lines = result.split("\n").map((line) => line.trim())
                    if (lines.length >= 4) break
                }

                const expectedLines = [
                    "<!--",
                    "TradingView Lightweight Charts™",
                    "Copyright (с) 2025 TradingView, Inc. https://www.tradingview.com/",
                    "-->"
                ]
                const firstFour = lines.slice(0, 4)
                const isValid = expectedLines.every((line, idx) => firstFour[idx] === line)

                if (!isValid) {
                    setStatus("error")
                    setErrorMessage(`차트 파일이 유효하지 않습니다.`)
                    return
                }

                setStatus("valid")
            } catch {
                setStatus("error")
                setErrorMessage(`파일 검증 중 오류가 발생했습니다.`)
            }
        }

        void validateFile()
    }, [filename])

    if (status === "loading") {
        return (
            <div className="p-8 text-sm text-gray-500 italic">
                차트 유효성 검사 중...
            </div>
        )
    }

    if (status === "error") {
        return (
            <div className="p-8 text-red-500 font-semibold">
                {errorMessage}
            </div>
        )
    }

    return (
        // 차트 렌더링
        <iframe
            src={`/Charts/${filename}.html`}
            className="w-full h-full border-none"
        />
    )
}
