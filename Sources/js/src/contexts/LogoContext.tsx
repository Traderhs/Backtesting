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
const FINAL_FALLBACK_URL = "/Backboard/icon/fallback.png"; // 백엔드와 동일한 경로 사용

// Context 생성 (기본값 설정)
const LogoContext = createContext<LogoContextType>({
    getLogoUrl: () => FINAL_FALLBACK_URL,
    isLoading: () => false,
    preloadLogos: () => {}, // 초기 빈 함수 설정
    isGlobalLoading: false, // 전체 로딩 상태 기본값
});

// 커스텀 훅
export const useLogo = () => useContext(LogoContext);

// Provider 컴포넌트 정의
interface LogoProviderProps {
    children: ReactNode;
}

// Provider 이름 변경
export const LogoProvider: React.FC<LogoProviderProps> = ({ children }) => {
    // 로고 URL 캐시 (원본 심볼 이름 기준: "BTCUSDT" -> "/Backboard/icon/BTCUSDT.png")
    const [logoCache, setLogoCache] = useState<{ [symbolName: string]: string }>({});
    // 현재 로고를 fetching 중인지 추적 (원본 심볼 이름 기준)
    const fetchingLogosRef = useRef<Set<string>>(new Set());
    // 로딩 상태 (개별 심볼 기준, 선택적)
    const [loadingStatus, setLoadingStatus] = useState<{ [symbolName: string]: boolean }>({});
    // 이미 확인한 로컬 파일 존재 여부 캐싱
    const localFileExistsCache = useRef<{ [key: string]: boolean }>({});
    
    // 전체 로딩 상태 계산 (현재 로딩 중인 심볼이 있는지 확인)
    const isGlobalLoading = useMemo(() => {
        const hasLoadingSymbols = Object.values(loadingStatus).some(loading => loading);
        const hasFetchingSymbols = fetchingLogosRef.current.size > 0;

        return hasLoadingSymbols || hasFetchingSymbols;
    }, [loadingStatus]);

    // 로컬 파일 존재 여부 확인 함수
    const checkLocalFileExists = useCallback(async (symbolName: string): Promise<boolean> => {
        const localPath = `/Backboard/icon/${symbolName}.png`;
        
        // 캐시에서 확인 결과 존재 시 재사용
        if (localFileExistsCache.current[localPath] !== undefined) {
            return localFileExistsCache.current[localPath];
        }

        // Image 객체를 사용하여 이미지 존재 여부 확인
        return new Promise((resolve) => {
            const img = new Image();
            
            img.onload = () => {
                localFileExistsCache.current[localPath] = true;
                resolve(true);
            };
            
            img.onerror = () => {
                localFileExistsCache.current[localPath] = false;
                resolve(false);
            };

            // 스타일 설정 - 테두리 제거
            img.style.filter = 'none';
            img.style.border = 'none';
            img.style.boxShadow = 'none';

            // 캐시 방지를 위한 쿼리 파라미터 추가
            img.src = `${localPath}?t=${new Date().getTime()}`;
        });
    }, []);

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
            setLoadingStatus(prev => ({ ...prev, [symbolName]: true })); // 로딩 시작

            // 로컬 파일 경로
            const localPath = `/Backboard/icon/${symbolName}.png`;
            
            // 로컬 파일 존재 여부 확인
            checkLocalFileExists(symbolName)
                .then(async exists => {
                    if (exists) {
                        // 로컬 파일 존재하면 해당 경로 사용
                        setLogoCache(prevCache => ({
                            ...prevCache,
                            [symbolName]: localPath
                        }));
                        // 로컬 파일이 존재하는 경우도 Promise.resolve()로 반환하여 체인 유지
                        return Promise.resolve();
                    } else {
                        // 로컬 파일이 없으면 API 호출
                        try {
                            const response = await axios.get(`/api/get-logo?symbol=${encodeURIComponent(symbolName)}&save=true`);
                            const fetchedUrl = response.data?.logoUrl || FINAL_FALLBACK_URL;
                            // 캐시 업데이트
                            setLogoCache(prevCache_1 => ({
                                ...prevCache_1,
                                [symbolName]: fetchedUrl
                            }));
                        } catch (err) {
                            // 에러 발생 시 fallback URL을 캐시에 저장
                            setLogoCache(prevCache_2 => ({
                                ...prevCache_2,
                                [symbolName]: FINAL_FALLBACK_URL
                            }));
                        }
                    }
                })
                .finally(() => {
                    // fetching 완료 표시 제거 및 로딩 종료
                    fetchingLogosRef.current.delete(symbolName);
                    setLoadingStatus(prev => ({ ...prev, [symbolName]: false }));
                });
        }

        // 5. 캐시에 없으면 일단 fallback 반환 (fetch 완료 후 리렌더링되어 갱신됨)
        //    이미 fetching 중일 때도 fallback 반환
        return FINAL_FALLBACK_URL;

    }, [logoCache, checkLocalFileExists]); // 의존성 배열 업데이트

    // 여러 로고를 한 번에 미리 로드하는 함수
    const preloadLogos = useCallback((symbolNames: string[]) => {
        if (!symbolNames || !symbolNames.length) return;
        
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
                    // 로컬 파일 확인
                    const localPath = `/Backboard/icon/${symbol}.png`;
                    const localFileExists = await checkLocalFileExists(symbol);

                    if (localFileExists) {
                        // 로컬 파일 존재하면 해당 경로 사용
                        return {symbol, url: localPath};
                    }

                    // 로컬 파일이 없으면 API 호출
                    const response = await axios.get(`/api/get-logo?symbol=${encodeURIComponent(symbol)}&save=true`);
                    const fetchedUrl = response.data?.logoUrl || FINAL_FALLBACK_URL;
                    return {symbol, url: fetchedUrl};
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
            processBatch(batch).then(() => {});
        }
    }, [logoCache, checkLocalFileExists]);

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
