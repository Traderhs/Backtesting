import { useState, useEffect } from "react"
import { FixedSizeList as List } from "react-window"

export default function Log() {
    const [logLines, setLogLines] = useState<string[]>([])

    useEffect(() => {
        fetch("/Backboard/backtesting.log")
            .then(async (res) => {
                const text = await res.text()
                if (text.startsWith("<!DOCTYPE html>")) return
                setLogLines(text.split("\n"))
            })
            .catch(() => setLogLines([]))
    }, [])

    return (
        <div className="flex flex-col h-full w-full overflow-hidden">
            {/* 헤더 */}
            <div className="flex-none mb-4">
                <h1 className="text-2xl font-bold">🪵 백테스트 로그</h1>
            </div>

            {/* 로그 영역 */}
            <div className="flex-1 overflow-auto">
                {logLines.length === 0 ? (
                    <p className="text-muted-foreground italic">
                        백테스팅 로그 파일을 불러오는 데 실패했습니다.
                    </p>
                ) : (
                    <List
                        height={600}
                        itemCount={logLines.length}
                        itemSize={20}
                        width="100%"
                        overscanCount={100}
                        className="font-mono text-sm"
                    >
                        {({ index, style }) => (
                            <div
                                key={index}
                                style={style}
                                className="px-4 whitespace-pre-wrap"
                            >
                                {logLines[index]}
                            </div>
                        )}
                    </List>
                )}
            </div>
        </div>
    )
}
