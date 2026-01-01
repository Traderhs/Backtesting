import React, {createContext, ReactNode, useCallback, useContext, useMemo, useRef, useState} from 'react';
import axios from 'axios';

// Context가 제공할 값의 타입 정의
interface LogoContextType {
    getLogoUrl: (symbolName: string) => string; // 로고 URL 가져오는 함수만 제공
    isLoading: (symbolName: string) => boolean; // 특정 심볼 로딩 상태 확인 함수 (선택적)
    preloadLogos: (symbolNames: string[]) => void; // 로고 사전 로드 함수 추가
    isGlobalLoading: boolean; // 전체 로딩 상태 추가
}

// 초기 Fallback 로고 URL (이 값은 백엔드 API 실패 시 최종 fallback으로 사용)
const FINAL_FALLBACK_URL = "/Logos/USDT.png"; // 서버의 폴백 이미지 경로

// Context 생성 (기본값 설정)
const LogoContext = createContext<LogoContextType>({
    getLogoUrl: () => FINAL_FALLBACK_URL,
    isLoading: () => false,
    preloadLogos: () => {
    }, // 초기 빈 함수 설정
    isGlobalLoading: false, // 전체 로딩 상태 기본값
});

// 커스텀 훅
export const useLogo = () => useContext(LogoContext);

// Provider 컴포넌트 정의
interface LogoProviderProps {
    children: ReactNode;
}

// Provider 이름 변경
export const LogoProvider: React.FC<LogoProviderProps> = ({children}) => {
    // 로고 URL/객체 URL 캐시 (심볼 -> 브라우저에서 사용할 URL)
    const [logoCache, setLogoCache] = useState<{ [symbolName: string]: string }>({});

    // 현재 로고를 fetching 중인지 추적 (원본 심볼 이름 기준)
    const fetchingLogosRef = useRef<Set<string>>(new Set());

    // 로딩 상태 (개별 심볼 기준, 선택적)
    const [loadingStatus, setLoadingStatus] = useState<{ [symbolName: string]: boolean }>({});

    // 전체 로딩 상태 계산 (현재 로딩 중인 심볼이 있는지 확인)
    const isGlobalLoading = useMemo(() => {
        const hasLoadingSymbols = Object.values(loadingStatus).some(loading => loading);
        const hasFetchingSymbols = fetchingLogosRef.current.size > 0;

        return hasLoadingSymbols || hasFetchingSymbols;
    }, [loadingStatus]);

    // 로고 URL 가져오는 함수 (백엔드 API 호출)
    const getLogoUrl = useCallback((symbolName: string): string => {
        // 1. 유효하지 않은 심볼 처리
        if (!symbolName) {
            return FINAL_FALLBACK_URL;
        }

        // 2. 캐시 확인
        if (logoCache[symbolName]) {
            return logoCache[symbolName]; // 캐시된 URL 반환
        }

        // 3. 현재 fetching 중인지 확인
        const isFetching = fetchingLogosRef.current.has(symbolName);

        // 4. 캐시에 없고, 현재 fetching 중도 아니면 fetch 시작
        if (!isFetching) {
            fetchingLogosRef.current.add(symbolName); // fetching 시작 표시
            setLoadingStatus(prev => ({...prev, [symbolName]: true})); // 로딩 시작

            (async () => {
                try {
                    // 서버 API에서 로고 URL을 가져옴 (서버는 JSON {logoUrl: "..."}을 반환)
                    const resp = await axios.get(`/api/get-logo?symbol=${encodeURIComponent(symbolName)}`);
                    const logoUrl = resp.data?.logoUrl || FINAL_FALLBACK_URL;

                    setLogoCache(prev => ({...prev, [symbolName]: logoUrl}));
                } catch (err) {
                    setLogoCache(prev => ({...prev, [symbolName]: FINAL_FALLBACK_URL}));
                } finally {
                    fetchingLogosRef.current.delete(symbolName);
                    setLoadingStatus(prev => ({...prev, [symbolName]: false}));
                }
            })();
        }

        // 5. 캐시에 없으면 일단 fallback 반환 (fetch 완료 후 리렌더링되어 갱신됨)
        //    이미 fetching 중일 때도 fallback 반환
        return FINAL_FALLBACK_URL;

    }, [logoCache]);

    // 여러 로고를 한 번에 미리 로드하는 함수
    const preloadLogos = useCallback((symbolNames: string[]) => {
        if (!symbolNames || !symbolNames.length) {
            return;
        }

        // 중복 제거 및 아직 캐시되지 않은 심볼만 필터링
        const uniqueSymbolsToLoad = [...new Set(symbolNames)].filter(
            symbol => !logoCache[symbol] && !fetchingLogosRef.current.has(symbol)
        );

        // 한 번에 로드할 로고 수 제한
        const batchSize = 5;

        // 배치 처리 함수
        const processBatch = async (batch: string[]) => {
            const promises = batch.map(async symbol => {
                fetchingLogosRef.current.add(symbol);
                setLoadingStatus(prev => ({...prev, [symbol]: true}));

                try {
                    // 서버 API에서 로고 URL을 가져옴 (서버는 JSON {logoUrl: "..."}을 반환)
                    const resp = await axios.get(`/api/get-logo?symbol=${encodeURIComponent(symbol)}`);
                    const logoUrl = resp.data?.logoUrl || FINAL_FALLBACK_URL;

                    return {symbol, url: logoUrl};
                } catch (err: any) {
                    console.error(`Error preloading logo for ${symbol}:`, err.response?.data || err.message);

                    return {symbol, url: FINAL_FALLBACK_URL};
                } finally {
                    // 모든 경우에 로딩 상태 정리
                    fetchingLogosRef.current.delete(symbol);
                    setLoadingStatus(prev => ({...prev, [symbol]: false}));
                }
            });

            const results = await Promise.all(promises);
            // 한 번에 캐시 업데이트하여 리렌더링 최소화
            setLogoCache(prevCache => {
                const newCache = {...prevCache};

                results.forEach(({symbol: symbol_1, url: url_2}) => {
                    newCache[symbol_1] = url_2;
                });

                return newCache;
            });
        };

        // 배치 단위로 처리
        for (let i = 0; i < uniqueSymbolsToLoad.length; i += batchSize) {
            const batch = uniqueSymbolsToLoad.slice(i, i + batchSize);
            processBatch(batch).then();
        }
    }, [logoCache]);

    // 특정 심볼이 로딩 중인지 확인하는 함수
    const isLoadingSymbol = useCallback((symbolName: string): boolean => {
        return loadingStatus[symbolName] || false;
    }, [loadingStatus]);

    // Context를 통해 제공할 값들
    const value = {
        getLogoUrl,
        isLoading: isLoadingSymbol,
        preloadLogos,
        isGlobalLoading,
    };

    return (
        <LogoContext.Provider value={value}>
            {children}
        </LogoContext.Provider>
    );
};
