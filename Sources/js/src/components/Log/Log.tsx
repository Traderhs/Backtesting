import React, {useEffect, useState, useCallback, useRef, useMemo} from 'react';
import {motion} from 'framer-motion';
import {VariableSizeList as List} from 'react-window';
import AutoSizer from 'react-virtualized-auto-sizer';
import LogSpinner from './LogSpinner';
import NoDataMessage from '@/components/Common/NoDataMessage';

const LOG_FILE_PATH = '/Backboard/backtesting.log'; // 상대 경로로 파일 요청 (서버 static 또는 API에 맞게 조정)

interface SearchResult {
    lineIndex: number;
    startIndex: number;
    endIndex: number;
}

// 한 줄 렌더러
const LogRow: React.FC<{
    index: number;
    style: React.CSSProperties;
    data: {
        lines: string[],
        searchTerm: string,
        currentSearchIndex: number,
        searchResults: SearchResult[],
        currentChunkStart: number,
        allLogLinesLength: number,
        forceRenderKey?: number,
        maxTextWidth?: number // 현재 청크의 최대 텍스트 너비
    }
}> = React.memo(({index, style, data}) => {
    // 인덱스 범위 체크
    if (index >= data.lines.length || index < 0) {
        return (
            <div style={{...style, visibility: 'hidden'}}>
                {/* 빈 플레이스홀더 */}
            </div>
        );
    }

    const line = data.lines[index];
    if (!line && line !== '') {
        return (
            <div style={{...style, visibility: 'hidden'}}>
                {/* 빈 플레이스홀더 */}
            </div>
        );
    }

    // style.top은 string(px) 형태. 20px 내려서 첫 줄 여백 확보
    const topOffset = typeof style.top === 'string' ? parseFloat(style.top) + 20 : (style.top as number) + 20;

    // 청크의 마지막 라인인지 확인
    const isChunkLastItem = index === data.lines.length - 1;

    const paddingAmount = 20;

    const adjustedStyle = {
        ...style,
        top: `${topOffset}px`
    } as React.CSSProperties;

    const searchTerm = data.searchTerm;
    const currentSearchIndex = data.currentSearchIndex;
    const searchResults = data.searchResults;
    const currentSearchResult = searchResults[currentSearchIndex];
    const isCurrentSearchLine = currentSearchResult && currentSearchResult.lineIndex === index;
    const isSeparator = /^=+$/.test(line.trim());

    if (isSeparator) {
        // 현재 청크의 최대 텍스트 너비를 기준으로 구분선 길이 계산
        const maxTextWidth = data.maxTextWidth || 1000; // 기본값 1000px
        
        // 일반 텍스트와 동일한 패딩 사용 (0px 10px 0px 25px)
        const paddingLeft = 25;
        const paddingRight = 10;
        
        // 실제 텍스트가 차지하는 공간 = 측정된 픽셀 너비
        // 구분선 문자 개수는 실제 텍스트 영역에만 맞춤
        const charWidth = 8.4;
        const minWidth = 1500; // 최소 구분선 너비 (픽셀)
        const desiredWidth = Math.max(minWidth, maxTextWidth); // 최소 너비 보장
        const textCharCount = Math.ceil(desiredWidth / charWidth);
        const separatorLine = '─'.repeat(textCharCount);

        return (
            <div
                style={{
                    ...adjustedStyle,
                    whiteSpace: 'pre',
                    fontFamily: "'Inter', 'Pretendard', monospace",
                    textRendering: 'optimizeLegibility',
                    WebkitFontSmoothing: 'antialiased',
                    MozOsxFontSmoothing: 'grayscale',
                    fontSize: '14px',
                    lineHeight: '1.4',
                    color: 'rgba(255, 215, 0, 0.4)',
                    padding: `0px ${paddingRight}px 0px ${paddingLeft}px`, // 일반 텍스트와 동일한 패딩
                    boxSizing: 'border-box',
                    display: 'flex',
                    alignItems: 'center',
                    // 계산된 너비 + 패딩으로 정확한 너비 설정
                    width: `${desiredWidth + paddingLeft + paddingRight}px`,
                    minWidth: `${desiredWidth + paddingLeft + paddingRight}px`,
                    overflow: 'hidden',
                    // 추가 안티엘리어싱 설정
                    fontSmooth: 'always',
                    WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                    fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                    textSizeAdjust: '100%',
                    WebkitTextSizeAdjust: '100%',
                }}
            >
                {separatorLine}
            </div>
        );
    }

    // 로그 레벨에 따른 색상 결정
    const getLogLevelColor = () => {
        if (line.includes('[DEBUG]')) {
            return 'rgb(156, 220, 254)';
        } else if (line.includes('[WARN]')) {
            return 'rgb(229, 192, 123)';
        } else if (line.includes('[ERROR]')) {
            return 'rgb(224, 108, 117)';
        } else if (line.includes('[BALANCE]')) {
            return 'rgb(128, 128, 128)';
        }
        return '#ffffff';
    };

    // 검색어 하이라이트와 날짜 및 파일:라인 패턴에 따른 색상 하이라이트
    const highlightSegments = () => {
        const elements: React.ReactNode[] = [];
        const logLevelColor = getLogLevelColor();

        // 첫 번째 '|' 문자 위치 찾기
        const firstPipeIndex = line.indexOf('|');

        // 날짜 또는 파일:라인 패턴을, 대괄호가 포함된 경우 함께 매칭
        const regex = /(\[?\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}]?)|(\[?[\w.\/\\-]+\.(?:cpp|hpp):\d+]?)/g;
        let lastIndex = 0;
        let key = 0;

        // 검색어가 있을 때 하이라이트 처리를 위한 함수
        const highlightSearchTerm = (text: string, color: string, textStartIndex: number) => {
            if (!searchTerm) {
                return <span style={{color}}>{text}</span>;
            }

            const escapedSearchTerm = searchTerm.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
            const regex = new RegExp(`(${escapedSearchTerm})`, 'gi');
            const parts = text.split(regex);
            let currentIndex = textStartIndex;

            return parts.map((part, i) => {
                const isMatch = part.toLowerCase() === searchTerm.toLowerCase();
                const partStartIndex = currentIndex;
                const partEndIndex = currentIndex + part.length;
                currentIndex = partEndIndex;

                // 현재 활성 검색 결과인지 확인
                const isCurrentMatch = isMatch &&
                    isCurrentSearchLine &&
                    currentSearchResult &&
                    partStartIndex >= currentSearchResult.startIndex &&
                    partEndIndex <= currentSearchResult.endIndex;

                return (
                    <span
                        key={i}
                        style={{
                            color,
                            backgroundColor: isMatch ?
                                (isCurrentMatch ? 'rgba(255, 165, 0, 0.6)' : 'rgba(255, 215, 0, 0.3)') : 'transparent',
                            padding: isMatch ? '1px 2px' : '0',
                            borderRadius: '2px',
                            border: isMatch && isCurrentMatch ? '1px solid rgba(255, 165, 0, 0.8)' : 'none',
                        }}
                    >
            {part}
          </span>
                );
            });
        };

        for (const match of line.matchAll(regex)) {
            const index = match.index ?? 0;

            // 일반 텍스트
            if (lastIndex < index) {
                const text = line.slice(lastIndex, index);
                const shouldColorize = (lastIndex < firstPipeIndex && firstPipeIndex !== -1) ||
                    (lastIndex >= firstPipeIndex && firstPipeIndex !== -1);

                elements.push(
                    <span key={key++}>
            {highlightSearchTerm(text, shouldColorize && logLevelColor !== '#ffffff' ? logLevelColor : '#ffffff', lastIndex)}
          </span>
                );
            }

            const token = match[0];
            // 대괄호 제거 후 판단용 문자열
            const stripped = token.replace(/^\[/, '').replace(/]$/, '');
            if (/^\d{4}-\d{2}-\d{2}/.test(stripped)) {
                // 날짜 패턴
                elements.push(
                    <span key={key++}>
            {highlightSearchTerm(token, 'rgb(106, 153, 85)', index)}
          </span>
                );
            } else {
                // 파일:라인 패턴
                elements.push(
                    <span key={key++}>
            {highlightSearchTerm(token, 'rgb(86, 156, 214)', index)}
          </span>
                );
            }

            lastIndex = index + match[0].length;
        }

        // 남은 일반 텍스트
        if (lastIndex < line.length) {
            const text = line.slice(lastIndex);
            const shouldColorize = (lastIndex < firstPipeIndex && firstPipeIndex !== -1) ||
                (lastIndex >= firstPipeIndex && firstPipeIndex !== -1);

            elements.push(
                <span key={key++}>
          {highlightSearchTerm(text, shouldColorize && logLevelColor !== '#ffffff' ? logLevelColor : '#ffffff', lastIndex)}
        </span>
            );
        }

        return elements;
    };

    return (
        <div
            style={{
                ...adjustedStyle,
                whiteSpace: 'pre',
                fontFamily: "'Inter', 'Pretendard', monospace",
                textRendering: 'optimizeLegibility',
                WebkitFontSmoothing: 'antialiased',
                MozOsxFontSmoothing: 'grayscale',
                fontSize: '14px',
                lineHeight: '1.4',
                color: '#ffffff', // 기본 흰색
                padding: isChunkLastItem ? `0px 10px ${paddingAmount}px 25px` : '0px 10px 0px 25px', // 각 청크의 마지막 아이템에 패딩, 마지막 청크는 중복 방지
                boxSizing: 'border-box',
                display: 'flex',
                alignItems: 'center',
                backgroundColor: isCurrentSearchLine ? 'rgba(255, 165, 0, 0.1)' : 'transparent',
                // 텍스트 뿌옇게 되는 문제 방지 - 더 강력한 설정
                opacity: 1,
                filter: 'none',
                backfaceVisibility: 'hidden',
                WebkitBackfaceVisibility: 'hidden',
                transform: 'translate3d(0, 0, 0)',
                willChange: 'auto',
                WebkitTransform: 'translate3d(0, 0, 0)',
                // 강화된 안티엘리어싱 설정
                fontSmooth: 'always',
                WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                textSizeAdjust: '100%',
                WebkitTextSizeAdjust: '100%',
            }}
        >
            {highlightSegments()}
        </div>
    );
});

// 컴포넌트 displayName 설정
LogRow.displayName = 'LogRow';

// Log 컴포넌트 Props 인터페이스
interface LogProps {
    isTextOptimizing?: boolean;
}

// Log 컴포넌트
const Log: React.FC<LogProps> = ({isTextOptimizing = false}) => {
    const [allLogLines, setAllLogLines] = useState<string[]>([]);
    const [loading, setLoading] = useState<boolean>(true);
    const [error, setError] = useState<string | null>(null);
    const [searchTerm, setSearchTerm] = useState<string>('');
    const [currentSearchIndex, setCurrentSearchIndex] = useState<number>(0);
    const [visibleStopIndex, setVisibleStopIndex] = useState<number>(0);
    const scrollOffsetRef = useRef(0);
    const listHeightRef = useRef(0);
    const animationFrameRef = useRef<number | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const [forceRenderKey, setForceRenderKey] = useState<number>(0);

    // 청킹 관련 상태 - 대용량 로그 처리를 위한 순차 로딩
    const CHUNK_SIZE = 300000; // 한 번에 렌더링할 최대 라인 수
    const [currentChunkStart, setCurrentChunkStart] = useState<number>(0);
    const [isChunkLoading, setIsChunkLoading] = useState<boolean>(false);
    const [totalLines, setTotalLines] = useState<number>(0);
    const [loadedLines, setLoadedLines] = useState<number>(0);

    // 탭 변경 감지 및 뿌옇게 되는 문제 해결을 위한 useEffect
    useEffect(() => {
        const handleTabActive = () => {
            // 탭이 활성화될 때 텍스트 렌더링 강제 갱신
            setTimeout(() => {
                if (listRef.current) {
                    listRef.current.resetAfterIndex(0, true);
                    setForceRenderKey(prev => prev + 1);
                }
            }, 100);
        };

        // 탭 활성화 이벤트 리스너 등록
        if (containerRef.current) {
            containerRef.current.addEventListener('tabActive', handleTabActive);
        }

        // 컴포넌트가 마운트되거나 다시 활성화될 때 CSS transform 강제 리셋
        const resetStyles = () => {
            if (containerRef.current) {
                const container = containerRef.current;

                // 1단계: 컨테이너 스타일 강제 리셋
                container.style.transform = 'translate3d(0, 0, 0)';
                container.style.opacity = '1';
                container.style.filter = 'none';
                container.style.backfaceVisibility = 'hidden';
                container.style.webkitBackfaceVisibility = 'hidden';

                // 2단계: react-window의 모든 가상화된 아이템 DOM 강제 제거 후 재생성
                if (listRef.current) {
                    // 기존 캐시 완전 삭제
                    listRef.current.resetAfterIndex(0, true);

                    // react-window 내부 DOM 구조 찾기
                    const listElement = container.querySelector('[style*="position: relative"]');
                    if (listElement) {
                        // 모든 가상화된 아이템들을 찾아서 스타일 강제 적용
                        const virtualItems = listElement.querySelectorAll('[style*="position: absolute"]');
                        virtualItems.forEach((item: any) => {
                            item.style.transform = 'translate3d(0px, ' + item.style.top + ', 0px)';
                            item.style.opacity = '1';
                            item.style.filter = 'none';
                            item.style.backfaceVisibility = 'hidden';
                            item.style.webkitBackfaceVisibility = 'hidden';
                            item.style.webkitFontSmoothing = 'antialiased';
                            item.style.mozOsxFontSmoothing = 'grayscale';
                        });
                    }
                }

                // 3단계: 리플로우 강제 실행
                container.offsetHeight;

                // 4단계: 약간의 지연 후 다시 한번 리셋 및 컴포넌트 강제 리렌더링
                setTimeout(() => {
                    if (listRef.current) {
                        listRef.current.resetAfterIndex(0, true);
                    }
                    // 리스트 컴포넌트 완전히 새로 렌더링하도록 키 변경
                    setForceRenderKey(prev => prev + 1);
                }, 50);
            }
        };

        // 즉시 실행
        resetStyles();

        // 탭 변경 후 지연된 실행들
        const timeoutIds = [
            setTimeout(() => {
                resetStyles();
                setForceRenderKey(prev => prev + 1);
            }, 100),
            setTimeout(() => {
                resetStyles();
                setForceRenderKey(prev => prev + 1);
            }, 300),
            setTimeout(() => {
                if (listRef.current) {
                    // 마지막으로 한번 더 강제 리렌더링
                    listRef.current.resetAfterIndex(0, true);
                }
                setForceRenderKey(prev => prev + 1);
            }, 600)
        ];

        return () => {
            // 이벤트 리스너 제거
            if (containerRef.current) {
                containerRef.current.removeEventListener('tabActive', handleTabActive);
            }
            timeoutIds.forEach(id => clearTimeout(id));
        };
    }, []); // 컴포넌트 마운트 시에만 실행

    // 현재 청크의 로그 라인들 (실제 렌더링되는 부분)
    const logLines = useMemo(() => {
        if (allLogLines.length <= CHUNK_SIZE) {
            return allLogLines; // 작은 파일은 그대로 사용
        }

        // 대용량 파일의 경우 현재 청크만 사용
        const start = currentChunkStart;
        const end = Math.min(start + CHUNK_SIZE, allLogLines.length);
        return allLogLines.slice(start, end);
    }, [allLogLines, currentChunkStart, CHUNK_SIZE]);

    // 현재 청크의 최대 텍스트 너비 계산
    const maxTextWidth = useMemo(() => {
        if (logLines.length === 0) return 1000; // 기본값

        // 캔버스를 사용하여 실제 텍스트 너비 측정
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d');
        if (!context) return 1000; // 기본값

        // 로그에서 사용하는 폰트 설정
        context.font = "14px 'Inter', 'Pretendard', monospace";

        let maxWidth = 0;
        // 구분선이 아닌 일반 텍스트 라인들만 측정
        for (const line of logLines) {
            if (!/^=+$/.test(line.trim())) { // 구분선이 아닌 경우만
                const textWidth = context.measureText(line).width;
                maxWidth = Math.max(maxWidth, textWidth);
            }
        }

        return maxWidth; 
    }, [logLines]);

    // 청크 관련 계산값들
    const totalChunks = Math.ceil(allLogLines.length / CHUNK_SIZE);
    const currentChunk = Math.floor(currentChunkStart / CHUNK_SIZE) + 1;
    const canNavigateToStart = currentChunkStart > 0;
    const canNavigateToEnd = currentChunkStart + CHUNK_SIZE < allLogLines.length;

    // 로그 파일 청크 단위 순차 로딩
    const fetchLog = useCallback(async () => {
        setLoading(true);
        setError(null);
        setAllLogLines([]);
        setTotalLines(0);
        setLoadedLines(0);

        try {
            // 1단계: 파일 메타데이터 확인 (HEAD 요청)
            const headResponse = await fetch(LOG_FILE_PATH, {method: 'HEAD'});
            if (!headResponse.ok) {
                throw new Error('로그 파일을 찾을 수 없습니다.');
            }

            // 2단계: 전체 파일 크기 확인
            const contentLength = headResponse.headers.get('content-length');
            const fileSize = contentLength ? parseInt(contentLength) : 0;

            // 3단계: 파일이 작은 경우 전체 로딩, 큰 경우 청크 로딩
            if (fileSize < 50 * 1024 * 1024) { // 50MB 미만
                // 작은 파일은 전체 로딩
                const response = await fetch(LOG_FILE_PATH);
                const text = await response.text();

                if (text.includes('<!doctype html>') || text.includes('<html')) {
                    throw new Error('로그 파일을 찾을 수 없습니다.');
                }

                const lines = text.split(/\r?\n/);
                setAllLogLines(lines);
                setTotalLines(lines.length);
                setLoadedLines(lines.length);
            } else {
                // 대용량 파일은 청크 단위 로딩
                await loadFileInChunks(fileSize);
            }

            // 항상 첫 번째 청크에서 시작
            setCurrentChunkStart(0);

        } catch (err: any) {
            console.error('로그 파일 불러오기 오류:', err);
            setError(err.message || '로그 파일을 불러오는 중 오류가 발생했습니다.');
        } finally {
            setLoading(false);
        }
    }, []);

    // 대용량 파일을 청크 단위로 로딩하는 함수
    const loadFileInChunks = useCallback(async (fileSize: number) => {
        const chunkSize = 512 * 1024; // 512KB 청크
        let loadedData = '';
        let offset = 0;
        let allLines: string[] = [];

        setIsChunkLoading(true);

        while (offset < fileSize) {
            try {
                const endByte = Math.min(offset + chunkSize - 1, fileSize - 1);

                // Range 요청으로 청크 데이터 가져오기
                const response = await fetch(LOG_FILE_PATH, {
                    headers: {
                        'Range': `bytes=${offset}-${endByte}`
                    }
                });

                if (!response.ok) {
                    throw new Error(`청크 로딩 실패: ${response.status}`);
                }

                const chunk = await response.text();
                loadedData += chunk;

                // 줄바꿈 기준으로 라인 분할, 마지막 불완전한 줄은 다음 청크로 이월
                const lines = loadedData.split(/\r?\n/);
                const completeLines = lines.slice(0, -1); // 마지막 줄 제외
                const incompleteLine = lines[lines.length - 1]; // 마지막 줄

                // 완성된 라인들을 결과에 추가
                allLines.push(...completeLines);

                // 진행 상황 업데이트
                setLoadedLines(allLines.length);
                setTotalLines(Math.floor(fileSize / 50)); // 추정값

                // 불완전한 줄은 다음 청크를 위해 보관
                loadedData = incompleteLine;

                offset = endByte + 1;

                // UI 응답성을 위한 짧은 지연
                if (offset < fileSize) {
                    await new Promise(resolve => setTimeout(resolve, 10));
                }

            } catch (chunkError) {
                console.error('청크 로딩 오류:', chunkError);
                // 청크 로딩 실패 시 전체 파일 로딩 시도
                const fallbackResponse = await fetch(LOG_FILE_PATH);
                const fallbackText = await fallbackResponse.text();
                allLines = fallbackText.split(/\r?\n/);
                break;
            }
        }

        // 마지막 불완전한 줄이 있으면 추가
        if (loadedData.trim()) {
            allLines.push(loadedData);
        }

        setAllLogLines(allLines);
        setTotalLines(allLines.length);
        setLoadedLines(allLines.length);
        setIsChunkLoading(false);
    }, []);

    // 높이 계산 함수 - 일관성을 위해 별도 함수로 분리
    const calculateItemHeight = useCallback((index: number) => {
        if (index >= logLines.length || index < 0) return 28; // 기본값
        const line = logLines[index];
        if (!line) return 28; // 안전 체크
        const isSeparator = /^=+$/.test(line.trim());
        return isSeparator ? 14 : 28; // 기본 높이만 반환, 패딩은 LogRow에서 처리
    }, [logLines]);

    // 각 아이템의 offset 계산 - 메모이제이션 최적화
    const itemOffsets = useMemo(() => {
        if (logLines.length === 0) return [];
        const offsets = new Array(logLines.length);
        let currentOffset = 0;
        for (let i = 0; i < logLines.length; i++) {
            offsets[i] = currentOffset;
            const height = calculateItemHeight(i);
            currentOffset += height;
        }
        return offsets;
    }, [logLines, calculateItemHeight]);

    // 검색 결과 배열 계산 - 각 개별 검색어 위치를 저장
    const searchResults = useMemo(() => {
        if (!searchTerm) return [];

        const results: SearchResult[] = [];
        const escapedSearchTerm = searchTerm.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

        logLines.forEach((line, lineIndex) => {
            let match;
            // regex.lastIndex 초기화를 위해 새로운 정규식 객체 생성
            const localRegex = new RegExp(escapedSearchTerm, 'gi');
            while ((match = localRegex.exec(line)) !== null) {
                // 유효한 검색 결과인지 확인
                if (typeof match.index === 'number' && match[0]) {
                    results.push({
                        lineIndex,
                        startIndex: match.index,
                        endIndex: match.index + match[0].length
                    });
                }

                // 무한 루프 방지
                if (localRegex.lastIndex === match.index) {
                    localRegex.lastIndex++;
                }
            }
        });

        return results;
    }, [logLines, searchTerm]);

    // 검색어 변경 시 첫 번째 결과로 이동
    useEffect(() => {
        if (searchResults.length > 0) {
            setCurrentSearchIndex(0);
        } else {
            setCurrentSearchIndex(0);
        }
    }, [searchResults]);

    // 청크 변경 시 검색 결과 재계산 및 검색 인덱스 초기화
    useEffect(() => {
        // 청크가 변경되면 항상 검색 인덱스를 0으로 초기화 (검색어가 있든 없든)
        setCurrentSearchIndex(0);
    }, [currentChunkStart]);

    // currentSearchIndex가 searchResults 범위를 벗어나지 않도록 보장
    useEffect(() => {
        if (searchResults.length > 0 && currentSearchIndex >= searchResults.length) {
            setCurrentSearchIndex(searchResults.length - 1);
        } else if (searchResults.length === 0 && currentSearchIndex !== 0) {
            setCurrentSearchIndex(0);
        }
    }, [searchResults.length, currentSearchIndex]);

    const listRef = useRef<List>(null);
    const rowHeights = useRef<{ [key: number]: number }>({});

    useEffect(() => {
        fetchLog().then();
    }, [fetchLog]);

    // 로그 로딩 완료 후 상단으로 스크롤
    useEffect(() => {
        if (!loading && listRef.current && logLines.length > 0) {
            setTimeout(() => {
                if (listRef.current) {
                    listRef.current.scrollToItem(0, 'start');
                }
            }, 100);
        }
    }, [loading, logLines.length]);

    // 부드러운 스크롤 함수
    const smoothScrollTo = useCallback((to: number) => {
        if (animationFrameRef.current) {
            cancelAnimationFrame(animationFrameRef.current);
        }

        const from = scrollOffsetRef.current;
        const distance = to - from;
        const duration = 100; // ms
        let startTime: number | null = null;

        const easeInOutCubic = (t: number) => t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;

        const animateScroll = (timestamp: number) => {
            if (!startTime) startTime = timestamp;
            const elapsed = timestamp - startTime;
            const progress = Math.min(elapsed / duration, 1);
            const easedProgress = easeInOutCubic(progress);

            const newScrollOffset = from + distance * easedProgress;
            listRef.current?.scrollTo(newScrollOffset);

            if (elapsed < duration) {
                animationFrameRef.current = requestAnimationFrame(animateScroll);
            }
        };

        animationFrameRef.current = requestAnimationFrame(animateScroll);
    }, []);

    // 자동 스크롤 제거 - 모든 청크에서 항상 상단에서 시작
    // 검색 결과로 스크롤하는 경우만 유지

    // 현재 검색 결과로 스크롤 - 청크 변경 후에도 자동으로 첫 번째 검색 결과로 이동
    useEffect(() => {
        // 약간의 지연을 두어 청크 변경 후 searchResults가 업데이트되도록 함
        const scrollToSearch = () => {
            if (listRef.current && searchResults.length > 0 && currentSearchIndex >= 0 && currentSearchIndex < searchResults.length) {
                const targetResult = searchResults[currentSearchIndex];
                // targetResult와 lineIndex의 유효성 검사
                if (!targetResult || typeof targetResult.lineIndex !== 'number') {
                    console.warn('Invalid search result:', targetResult);
                    return;
                }

                const targetLineIndex = targetResult.lineIndex;
                // targetLineIndex가 현재 logLines 범위 내에 있는지 확인
                if (targetLineIndex < 0 || targetLineIndex >= logLines.length) {
                    console.warn('Search result lineIndex out of bounds:', targetLineIndex, 'logLines.length:', logLines.length);
                    return;
                }

                // itemOffsets 배열이 존재하고 인덱스가 유효한지 확인
                if (!itemOffsets || targetLineIndex >= itemOffsets.length) {
                    console.warn('itemOffsets not ready or index out of bounds:', targetLineIndex, 'itemOffsets.length:', itemOffsets?.length);
                    return;
                }

                const targetOffset = itemOffsets[targetLineIndex];
                const itemHeight = getRowHeight(targetLineIndex);
                const centeredOffset = targetOffset - listHeightRef.current / 2 + itemHeight / 2;

                smoothScrollTo(Math.max(0, centeredOffset));
            }
        };

        // 청크 변경 직후에는 약간의 지연을 두어 DOM이 업데이트되도록 함
        if (searchTerm && searchResults.length > 0) {
            setTimeout(scrollToSearch, 150);
        } else {
            scrollToSearch();
        }
    }, [currentSearchIndex, searchResults, itemOffsets, smoothScrollTo, searchTerm]);

    const getRowHeight = useCallback(
        (index: number) => {
            // 범위 체크
            if (index < 0 || index >= logLines.length) {
                return 28;
            }

            // 캐시된 높이가 있으면 사용, 없으면 계산해서 캐시
            if (rowHeights.current[index] !== undefined) {
                return rowHeights.current[index];
            }

            const baseHeight = calculateItemHeight(index);
            // 각 청크의 마지막 아이템에 패딩 추가, 마지막 청크는 중복 방지
            const isChunkLastItem = index === logLines.length - 1;
            if (isChunkLastItem) {
                const paddingAmount = 20;
                const height = baseHeight + paddingAmount;
                rowHeights.current[index] = height;
                return height;
            } else {
                const height = baseHeight;
                rowHeights.current[index] = height;
                return height;
            }
        },
        [calculateItemHeight, logLines.length]
    );

    // 로그 라인이 변경될 때마다 높이 캐시 초기화 및 강제 리렌더링
    useEffect(() => {
        rowHeights.current = {};
        // react-window 리스트에 캐시 재설정을 알림
        if (listRef.current && logLines.length > 0) {
            // 모든 아이템의 크기를 다시 계산하도록 강제
            listRef.current.resetAfterIndex(0, true);

            // 강제 리렌더링을 위한 추가 지연
            setTimeout(() => {
                if (listRef.current) {
                    listRef.current.resetAfterIndex(0, true);
                }
            }, 50);
        }
    }, [logLines]);

    // 컴포넌트 언마운트 시 애니메이션 프레임 정리
    useEffect(() => {
        return () => {
            if (animationFrameRef.current) {
                cancelAnimationFrame(animationFrameRef.current);
            }
        };
    }, []);

    const handlePrevSearch = () => {
        if (searchResults.length > 0) {
            setCurrentSearchIndex(prev => {
                const newIndex = prev === 0 ? searchResults.length - 1 : prev - 1;
                // 유효한 인덱스인지 확인
                return (newIndex >= 0 && newIndex < searchResults.length) ? newIndex : 0;
            });
        }
    };

    const handleNextSearch = () => {
        if (searchResults.length > 0) {
            setCurrentSearchIndex(prev => {
                const newIndex = prev === searchResults.length - 1 ? 0 : prev + 1;
                // 유효한 인덱스인지 확인
                return (newIndex >= 0 && newIndex < searchResults.length) ? newIndex : 0;
            });
        }
    };

    const handleSearchChange = (value: string) => {
        setSearchTerm(value);
        setCurrentSearchIndex(0);
    };

    // 청크 네비게이션 함수들

    const goToFirstChunk = () => {
        setCurrentChunkStart(0);
        setCurrentSearchIndex(0); // 검색 인덱스 초기화
        // 검색어가 있으면 첫 번째 검색 결과로 이동, 없으면 첫 번째 줄로 스크롤
        setTimeout(() => {
            if (listRef.current) {
                if (!searchTerm) {
                    listRef.current.scrollToItem(0, 'start');
                }
                // 검색어가 있으면 useEffect에서 자동으로 첫 번째 검색 결과로 스크롤됨
            }
        }, 100);
    };

    const goToLastChunk = () => {
        // 마지막 청크의 시작점 계산
        const lastChunkStart = (totalChunks - 1) * CHUNK_SIZE;
        setCurrentChunkStart(lastChunkStart);
        setCurrentSearchIndex(0); // 검색 인덱스 초기화
        // 검색어가 있으면 첫 번째 검색 결과로 이동, 없으면 첫 번째 줄로 스크롤
        setTimeout(() => {
            if (listRef.current) {
                if (!searchTerm) {
                    listRef.current.scrollToItem(0, 'start');
                }
                // 검색어가 있으면 useEffect에서 자동으로 첫 번째 검색 결과로 스크롤됨
            }
        }, 100);
    };

    const goToPrevChunk = () => {
        const newStart = Math.max(0, currentChunkStart - CHUNK_SIZE);
        setCurrentChunkStart(newStart);
        setCurrentSearchIndex(0); // 검색 인덱스 초기화
        // 검색어가 있으면 첫 번째 검색 결과로 이동, 없으면 첫 번째 줄로 스크롤
        setTimeout(() => {
            if (listRef.current) {
                if (!searchTerm) {
                    listRef.current.scrollToItem(0, 'start');
                }
                // 검색어가 있으면 useEffect에서 자동으로 첫 번째 검색 결과로 스크롤됨
            }
        }, 100);
    };

    const goToNextChunk = () => {
        // 다음 청크의 시작점 계산
        const nextChunkStart = currentChunkStart + CHUNK_SIZE;
        // totalChunks 범위 내에서만 이동
        if (currentChunk < totalChunks) {
            setCurrentChunkStart(nextChunkStart);
            setCurrentSearchIndex(0); // 검색 인덱스 초기화
        }
        // 검색어가 있으면 첫 번째 검색 결과로 이동, 없으면 첫 번째 줄로 스크롤
        setTimeout(() => {
            if (listRef.current) {
                listRef.current.scrollToItem(0, 'start');
            }
        }, 100);
    };

    const handleItemsRendered = ({visibleStopIndex}: { visibleStopIndex: number }) => {
        setVisibleStopIndex(visibleStopIndex);
    };

    const handleScroll = ({scrollOffset}: { scrollOffset: number }) => {
        scrollOffsetRef.current = scrollOffset;
    };

    const controlButtonVariants = {
        hover: {
            scale: 1.05,
            borderColor: 'rgba(255, 215, 0, 0.7)',
            transition: {duration: 0.25}
        },
        tap: {
            scale: 0.95,
            borderColor: 'rgba(255, 215, 0, 0.7)',
            backgroundColor: 'rgba(255, 215, 0, 0.3)',
            transition: {duration: 0.25}
        }
    };

    const closeButtonVariants = {
        hover: {
            scale: 1.05,
            transition: {duration: 0.25}
        },
        tap: {
            scale: 0.95,
            backgroundColor: 'rgb(180, 40, 50)',
            transition: {duration: 0.25}
        }
    };

    return (
        <motion.div
            ref={containerRef}
            className="h-full w-full flex flex-col p-4 overflow-y-auto log-container"
            initial={{opacity: 0}}
            animate={{opacity: 1}}
            transition={{duration: 0.5}}
            style={{
                // 뿌옇게 되는 문제 방지를 위한 명시적 스타일 - 강화된 버전
                filter: 'none',
                backfaceVisibility: 'hidden',
                WebkitBackfaceVisibility: 'hidden',
                transform: 'translate3d(0, 0, 0)',
                WebkitTransform: 'translate3d(0, 0, 0)',
                willChange: 'auto',
                WebkitFontSmoothing: 'antialiased',
                MozOsxFontSmoothing: 'grayscale',
                textRendering: 'optimizeLegibility',
                // 강화된 안티엘리어싱 설정
                fontSmooth: 'always',
                fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                textSizeAdjust: '100%',
                WebkitTextSizeAdjust: '100%',
                position: 'relative', // LogSpinner를 위한 상대 위치 설정
            }}
        >
            {/* 초기 로딩 시 전체 화면 스피너 - 최상위에 배치 */}
            {loading && <LogSpinner/>}

            {/* 제목 영역 */}
            <div
                style={{
                    position: 'relative',
                    marginBottom: '25px',
                    zIndex: 100,
                }}
            >
                <motion.h2
                    initial={{opacity: 0, x: -20}}
                    animate={{opacity: loading ? 0 : 1, x: loading ? -20 : 0}}
                    transition={{delay: 0.1, duration: 0.5}}
                    style={{
                        color: 'white',
                        fontSize: '2.5rem',
                        fontWeight: 700,
                        textAlign: 'left',
                        marginLeft: '35px',
                        marginTop: '10px',
                        paddingBottom: '8px',
                        display: 'inline-block',
                        position: 'relative',
                    }}
                >
                    백테스팅 로그
                    {/* 밑줄 */}
                    <motion.span
                        initial={{width: 0}}
                        animate={{width: loading ? 0 : '100%'}}
                        transition={{delay: 0.3, duration: 0.5}}
                        style={{
                            position: 'absolute',
                            bottom: 0,
                            left: 0,
                            right: 0,
                            height: '2px',
                            background: 'rgba(255, 215, 0, 0.4)',
                        }}
                    />
                </motion.h2>
            </div>

            {/* 검색 영역과 청크 네비게이션 */}
            <motion.div 
                initial={{opacity: 0, y: -10}}
                animate={{opacity: loading ? 0 : 1, y: loading ? -10 : 0}}
                transition={{delay: 0.5, duration: 0.5}}
                style={{
                margin: '0 20px 15px 20px',
                display: 'flex',
                gap: '15px',
                alignItems: 'center',
            }}>
                {/* 검색 영역 */}
                <motion.div
                    initial={{opacity: 0, y: -10}}
                    animate={{opacity: loading ? 0 : 1, y: loading ? -10 : 0}}
                    transition={{delay: 0.6, duration: 0.5}}
                    style={{
                        padding: '15px 20px',
                        background: '#111111',
                        borderRadius: '8px',
                        border: '1px solid rgba(255, 215, 0, 0.2)',
                        display: 'flex',
                        gap: '15px',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        width: '800px',
                        minWidth: '800px',
                        maxWidth: '800px',
                    }}
                >
                    {/* 검색 입력 및 네비게이션 */}
                    <div style={{
                        flex: 1,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '10px'
                    }}>
                        <div style={{
                            position: 'relative',
                            width: '500px',
                            minWidth: '500px'
                        }}>
                            <input
                                type="text"
                                placeholder="검색"
                                value={searchTerm}
                                onChange={(e) => handleSearchChange(e.target.value)}
                                style={{
                                    width: '500px',
                                    padding: '8px 12px',
                                    paddingRight: searchTerm ? '70px' : '12px',
                                    background: '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: '#ffffff',
                                    fontSize: '14px',
                                    fontFamily: "'Inter', 'Pretendard', sans-serif",
                                    outline: 'none',
                                }}
                            />

                            {/* 검색 결과 표시 (검색창 내부) */}
                            {searchTerm && (
                                <div style={{
                                    position: 'absolute',
                                    right: '12px',
                                    top: '50%',
                                    transform: 'translateY(-50%)',
                                    color: '#888888',
                                    fontSize: '12px',
                                    pointerEvents: 'none',
                                    minWidth: '50px',
                                    textAlign: 'right',
                                }}>
                                    {searchResults.length > 0 ? `${(currentSearchIndex + 1).toLocaleString()} / ${searchResults.length.toLocaleString()}` : '0 / 0'}
                                </div>
                            )}
                        </div>

                        {/* 검색 네비게이션 버튼 */}
                        <div style={{
                            display: 'flex',
                            gap: '5px',
                            minWidth: '95px',
                            maxWidth: '95px',
                            justifyContent: 'space-between'
                        }}>
                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={handlePrevSearch}
                                disabled={searchResults.length === 0}
                                style={{
                                    padding: '4px 8px',
                                    backgroundColor: searchResults.length > 0 ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: searchResults.length > 0 ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: searchResults.length > 0 ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ↑
                            </motion.button>
                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={handleNextSearch}
                                disabled={searchResults.length === 0}
                                style={{
                                    padding: '4px 8px',
                                    backgroundColor: searchResults.length > 0 ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: searchResults.length > 0 ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: searchResults.length > 0 ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ↓
                            </motion.button>
                            <motion.button
                                variants={closeButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={() => setSearchTerm('')}
                                style={{
                                    padding: '4px 8px',
                                    background: '#f23645',
                                    border: 'none',
                                    borderRadius: '4px',
                                    color: '#ffffff',
                                    fontSize: '12px',
                                    cursor: 'pointer',
                                }}
                            >
                                ✕
                            </motion.button>
                        </div>
                    </div>

                    {/* 라인 정보 표시 */}
                    <div style={{
                        color: '#888888',
                        fontSize: '12px',
                        minWidth: '110px',
                        textAlign: 'right'
                    }}>
                        {allLogLines.length > CHUNK_SIZE
                            ? `${(currentChunkStart + visibleStopIndex + 1).toLocaleString()} / ${allLogLines.length.toLocaleString()}`
                            : `${(visibleStopIndex + 1).toLocaleString()} / ${logLines.length.toLocaleString()}`
                        }
                    </div>
                </motion.div>

                {/* 청크 네비게이션 - 대용량 파일일 때만 표시 */}
                {allLogLines.length > CHUNK_SIZE && (
                    <motion.div
                        initial={{opacity: 0, y: -10}}
                        animate={{opacity: loading ? 0 : 1, y: loading ? -10 : 0}}
                        transition={{delay: 0.7, duration: 0.5}}
                        style={{
                            width: '260px',
                            height: '71px',
                            padding: '15px 20px',
                            background: '#111111',
                            borderRadius: '8px',
                            border: '1px solid rgba(255, 215, 0, 0.2)',
                            display: 'flex',
                            gap: '10px',
                            alignItems: 'center',
                        }}
                    >
                        <div style={{
                            display: 'flex',
                            gap: '6px',
                            alignItems: 'center'
                        }}>
                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={goToFirstChunk}
                                disabled={!canNavigateToStart}
                                style={{
                                    padding: '5px 9px',
                                    backgroundColor: canNavigateToStart ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: canNavigateToStart ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: canNavigateToStart ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ⏮
                            </motion.button>

                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={goToPrevChunk}
                                disabled={!canNavigateToStart}
                                style={{
                                    padding: '5px 9px',
                                    backgroundColor: canNavigateToStart ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: canNavigateToStart ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: canNavigateToStart ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ◀
                            </motion.button>

                            <span style={{
                                color: '#cccccc',
                                fontSize: '12px',
                                margin: '0 6px',
                                minWidth: '65px',
                                textAlign: 'center'
                            }}>
                            {currentChunk} / {totalChunks}
                        </span>

                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={goToNextChunk}
                                disabled={!canNavigateToEnd}
                                style={{
                                    padding: '5px 9px',
                                    backgroundColor: canNavigateToEnd ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: canNavigateToEnd ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: canNavigateToEnd ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ▶
                            </motion.button>

                            <motion.button
                                variants={controlButtonVariants}
                                whileHover="hover"
                                whileTap="tap"
                                onClick={goToLastChunk}
                                disabled={!canNavigateToEnd}
                                style={{
                                    padding: '5px 9px',
                                    backgroundColor: canNavigateToEnd ? '#2a2a2a' : '#1a1a1a',
                                    border: '1px solid rgba(255, 215, 0, 0.3)',
                                    borderRadius: '4px',
                                    color: canNavigateToEnd ? '#ffffff' : '#666666',
                                    fontSize: '12px',
                                    cursor: canNavigateToEnd ? 'pointer' : 'not-allowed',
                                }}
                            >
                                ⏭
                            </motion.button>
                        </div>
                    </motion.div>
                )}
            </motion.div>

            {/* 컨텐츠 영역 */}
            <motion.div
                initial={{opacity: 0}}
                animate={{opacity: loading ? 0 : 1}}
                transition={{delay: 0.8, duration: 0.5}}
                style={{
                    flex: 1,
                    borderRadius: '8px',
                    overflow: 'hidden',
                    background: '#111111',
                    boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
                    border: '1.2px solid rgba(255, 215, 0, 0.3)',
                    position: 'relative',
                    minHeight: '500px',
                    minWidth: '800px',
                    margin: '8px 20px 15px 20px',
                    display: 'flex',
                    alignItems: 'stretch',
                    justifyContent: 'flex-start',
                    // 뿌옇게 되는 문제 방지 - 더 강력한 설정
                    opacity: 1,
                    filter: 'none',
                    backfaceVisibility: 'hidden',
                    WebkitBackfaceVisibility: 'hidden',
                    transform: 'translate3d(0, 0, 0)',
                    WebkitTransform: 'translate3d(0, 0, 0)',
                    willChange: 'auto',
                    WebkitFontSmoothing: 'antialiased',
                    MozOsxFontSmoothing: 'grayscale',
                    textRendering: 'optimizeLegibility',
                    // 강화된 안티엘리어싱 설정
                    fontSmooth: 'always',
                    fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                    WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                    textSizeAdjust: '100%',
                    WebkitTextSizeAdjust: '100%',
                }}
            >
                {loading && (
                    <div style={{
                        width: '100%',
                        height: '100%',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        flexDirection: 'column',
                        gap: '20px'
                    }}>
                        <LogSpinner/>
                        {isChunkLoading && totalLines > 0 && (
                            <div style={{
                                color: 'rgba(255, 255, 255, 0.8)',
                                fontSize: '14px',
                                fontFamily: "'Inter', 'Pretendard', sans-serif",
                                textAlign: 'center'
                            }}>
                                <div>대용량 로그 파일 로딩 중...</div>
                                <div style={{marginTop: '8px', fontSize: '12px', color: 'rgba(255, 255, 255, 0.6)'}}>
                                    {loadedLines.toLocaleString()} / {totalLines > 0 ? totalLines.toLocaleString() : '?'} 라인
                                </div>
                                <div style={{
                                    width: '200px',
                                    height: '4px',
                                    backgroundColor: 'rgba(255, 255, 255, 0.2)',
                                    borderRadius: '2px',
                                    marginTop: '8px',
                                    overflow: 'hidden'
                                }}>
                                    <div style={{
                                        width: totalLines > 0 ? `${Math.min((loadedLines / totalLines) * 100, 100)}%` : '0%',
                                        height: '100%',
                                        backgroundColor: 'rgba(255, 215, 0, 0.8)',
                                        transition: 'width 0.3s ease',
                                        borderRadius: '2px'
                                    }}/>
                                </div>
                            </div>
                        )}
                    </div>
                )}

                {error && !loading && (
                    <NoDataMessage message="로그 파일을 불러올 수 없습니다."/>
                )}

                {!loading && !error && logLines.length > 0 && (
                    <>
                        <AutoSizer>
                            {({height, width}: { height: number; width: number }) => {
                                listHeightRef.current = height;
                                return (
                                    <List
                                        key={`log-list-${forceRenderKey}`}
                                        ref={listRef}
                                        height={height}
                                        width={width}
                                        itemCount={logLines.length}
                                        itemSize={getRowHeight}
                                        itemData={{
                                            lines: logLines,
                                            searchTerm,
                                            currentSearchIndex,
                                            searchResults,
                                            currentChunkStart,
                                            allLogLinesLength: allLogLines.length,
                                            forceRenderKey, // 강제 리렌더링을 위한 키 추가
                                            maxTextWidth // 최대 텍스트 너비 추가
                                        }}
                                        estimatedItemSize={28}
                                        onItemsRendered={handleItemsRendered}
                                        onScroll={handleScroll}
                                        overscanCount={10}
                                        useIsScrolling={false}
                                        layout="vertical"
                                        direction="ltr"
                                        style={{
                                            // 뿌옇게 되는 문제 방지를 위한 명시적 스타일
                                            opacity: 1,
                                            filter: 'none',
                                            backfaceVisibility: 'hidden',
                                            WebkitBackfaceVisibility: 'hidden',
                                            transform: 'translate3d(0, 0, 0)',
                                            willChange: 'scroll-position',
                                            WebkitFontSmoothing: 'antialiased',
                                            MozOsxFontSmoothing: 'grayscale',
                                            textRendering: 'optimizeLegibility',
                                            // 강화된 안티엘리어싱 설정
                                            fontSmooth: 'always',
                                            fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                                            WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                                            textSizeAdjust: '100%',
                                            WebkitTextSizeAdjust: '100%',
                                        }}
                                    >
                                        {LogRow}
                                    </List>
                                )
                            }}
                        </AutoSizer>

                        {/* 텍스트 최적화 중 로딩 오버레이 */}
                        {isTextOptimizing && (
                            <div style={{
                                position: 'absolute',
                                top: 0, left: 0, right: 0, bottom: 0,
                                backgroundColor: 'rgba(17, 17, 17, 0.8)',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                zIndex: 1000,
                                backdropFilter: 'blur(2px)',
                                WebkitBackdropFilter: 'blur(2px)',
                                pointerEvents: 'all'
                            }}>
                                <div style={{
                                    display: 'flex',
                                    flexDirection: 'column',
                                    alignItems: 'center',
                                    gap: '10px'
                                }}>
                                    {/* 단순 CSS 애니메이션 스피너 */}
                                    <div style={{
                                        width: '42px',
                                        height: '42px',
                                        borderRadius: '50%',
                                        border: '3px solid rgba(20, 20, 20, 0.15)',
                                        borderTopColor: '#FFD700',
                                        animation: 'spin 1s cubic-bezier(0.4, 0.1, 0.3, 1) infinite',
                                        margin: '0 auto',
                                        boxShadow: '0 0 20px rgba(255, 215, 0, 0.15)',
                                        transform: 'translateZ(0)',
                                        willChange: 'transform'
                                    }}/>
                                </div>
                            </div>
                        )}
                    </>
                )}

                {!loading && !error && logLines.length === 0 && (
                    <NoDataMessage message="로그 데이터가 없습니다."/>
                )}
            </motion.div>
        </motion.div>
    );
};

export default Log;
