const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {exec} = require("child_process");
const express = require("express");
const {readFileSync} = require("fs");
const {Server: WebSocketServer} = require("ws");
const parquet = require("@dsnp/parquetjs");
const net = require("net");

// PKG 환경에서 axios 로드
let axios;
try {
    if (process.pkg) {
        // PKG 환경에서는 exports 필드에 정의된 경로 사용
        axios = require('axios/dist/node/axios.cjs');
    } else {
        axios = require('axios');
    }
} catch (e) {
    try {
        // 메인 axios 경로 시도
        axios = require('axios');
    } catch (e2) {
        console.warn('[WARN] axios 로드 실패, fallback 사용:', e2.message);
        axios = null;
    }
}

const app = express();
let port = 7777;

// 포트 사용 가능 여부 확인 함수
function isPortAvailable(port) {
    return new Promise((resolve) => {
        const server = net.createServer();
        server.listen(port, () => {
            server.once('close', () => {
                resolve(true);
            });
            server.close();
        });
        server.on('error', () => {
            resolve(false);
        });
    });
}

// 사용 가능한 포트 찾기 함수
async function findAvailablePort(startPort) {
    let currentPort = startPort;
    while (!(await isPortAvailable(currentPort))) {
        console.log(`[INFO] Port ${currentPort} is already in use, trying ${currentPort + 1}`);
        currentPort++;
    }
    return currentPort;
}

// exe 파일 실행 시 올바른 경로 찾기
const isDevelopment = !process.pkg;
let baseDir;

if (isDevelopment) {
    baseDir = process.cwd();
} else {
    // PKG 환경에서 안전한 경로 처리
    try {
        baseDir = path.dirname(process.execPath) || process.cwd();
    } catch (e) {
        baseDir = process.cwd();
    }
}

console.log(`[INFO] Running in ${isDevelopment ? 'development' : 'production'} mode`);
console.log(`[INFO] Base directory: ${baseDir}`);

// -------------------------------
// 1. 정적 파일 제공 및 기본 라우팅
// -------------------------------
const iconDir = path.join(baseDir, "Backboard", "icon");
app.use("/Backboard", express.static(path.join(baseDir, "Backboard")));
const distPath = path.join(baseDir, "Backboard");
app.use(express.static(distPath));

// -------------------------------
//  새로운 API: /api/get-logo
// -------------------------------

// --- 통합 심볼 매핑 로직 (Binance + CoinGecko) ---
const manualSymbolIdMap = {
    'btc': 'bitcoin',
    'eth': 'ethereum',
    'usdt': 'tether',
    'bnb': 'binancecoin',
    'sol': 'solana',
    'usdc': 'usd-coin',
    'xrp': 'ripple',
    'doge': 'dogecoin',
    'ton': 'the-open-network',
    'ada': 'cardano',
    'shib': 'shiba-inu',
    'avax': 'avalanche-2',
    'trx': 'tron',
    'dot': 'polkadot',
    'link': 'chainlink',
    'wbtc': 'wrapped-bitcoin',
    'bch': 'bitcoin-cash',
    'near': 'near',
    'matic': 'matic-network',
    'ltc': 'litecoin',
    'icp': 'internet-computer',
    'uni': 'uniswap',
    'dai': 'dai',
    'leo': 'leo-token',
    'steth': 'staked-ether',
    'kas': 'kaspa',
    'etc': 'ethereum-classic',
    'op': 'optimism',
    'xlm': 'stellar',
    'okb': 'okb',
    'inj': 'injective-protocol',
    'fil': 'filecoin',
    'imx': 'immutable-x',
    'hbar': 'hedera-hashgraph',
    'cro': 'crypto-com-chain',
    'tao': 'bittensor',
    'apt': 'aptos',
    'vet': 'vechain',
    'ldo': 'lido-dao',
    'mkr': 'maker',
    'rndr': 'render-token',
    'grt': 'the-graph',
    'arb': 'arbitrum',
    'atom': 'cosmos',
    'mnt': 'mantle',
    'tia': 'celestia',
    'ethfi': 'ether-fi',
    'fet': 'fetch-ai',
    'sei': 'sei-network',
    'weth': 'weth',
    'stx': 'blockstack',
    'aave': 'aave',
    'theta': 'theta-token',
    'reth': 'rocket-pool-eth',
    'algo': 'algorand',
    'floki': 'floki',
    'sui': 'sui',
    'fdusd': 'first-digital-usd',
    'ftm': 'fantom',
    'ena': 'ethena',
    'axs': 'axie-infinity',
    'core': 'core-dao',
    'bonk': 'bonk',
    'snx': 'havven',
    'zeta': 'zetachain',
    'bgb': 'bitget-token',
    'agix': 'singularitynet',
    'xtz': 'tezos',
    'pyth': 'pyth-network',
    'beam': 'beam-2',
    'bsv': 'bitcoin-sv',
    'jup': 'jupiter-exchange-solana',
    'kava': 'kava',
    'chz': 'chiliz',
    'ordi': 'ordinals',
    'wld': 'worldcoin-wld',
    'wemix': 'wemix-token',
    'ocean': 'ocean-protocol',
    'hnt': 'helium',
    'gala': 'gala',
    'eos': 'eos',
    'mina': 'mina-protocol',
    'astr': 'astar',
    'rose': 'oasis-network',
    'pepe': 'pepe',
    'cfx': 'conflux-token',
    'xdc': 'xdce-network',
    'neo': 'neo',
    'flow': 'flow',
    'rune': 'thorchain',
    'kcs': 'kucoin-shares',
    'akt': 'akash-network',
    'sand': 'the-sandbox',
    'xec': 'ecash',
    'iota': 'iota',
    'blur': 'blur-token',
    'usdd': 'usdd',
    'gt': 'gatetoken',
    'ar': 'arweave',
    'osmo': 'osmosis',
    'mana': 'decentraland',
    'zec': 'zcash',
    'klay': 'klay-token',
    'egld': 'elrond-erd-2',
};

// Binance 심볼 정보 캐시
let binanceSymbolCache = null;
const BINANCE_24HR_TICKER_URL = "https://api.binance.com/api/v3/ticker/24hr";

// CoinGecko 캐시
let coinGeckoListCache = null;
const COINGECKO_LIST_URL = "https://api.coingecko.com/api/v3/coins/list";

// 더 안정적인 USDT 폴백 이미지 URL
const USDT_FALLBACK_ICON_PATH = "/Backboard/icon/USDT_fallback.png";
const USDT_LOGO_URLS = [
    "https://coin-images.coingecko.com/coins/images/325/small/Tether.png",
    "https://s2.coinmarketcap.com/static/img/coins/64x64/825.png",
    "https://assets.coingecko.com/coins/images/325/thumb/Tether-logo.png"
];

// Rate limiting을 위한 변수들
let lastCoinGeckoCall = 0;
const COINGECKO_RATE_LIMIT_MS = 2000; // 2초 간격

// 서버 시작 시 USDT 폴백 이미지 미리 다운로드 (여러 URL 시도)
async function initializeFallbackImage() {
    const fallbackPath = path.join(iconDir, "USDT_fallback.png");

    try {
        // 이미 존재하는지 확인
        await fsPromises.access(fallbackPath, fs.constants.F_OK);
        return;
    } catch (error) {
        // 존재하지 않으면 다운로드
    }

    // 여러 USDT URL을 순차적으로 시도
    for (const url of USDT_LOGO_URLS) {
        try {
            await fsPromises.mkdir(path.dirname(fallbackPath), {recursive: true});
            await downloadAndSaveImage(url, fallbackPath);
            return;
        } catch (downloadError) {
        }
    }
}

// Rate limited CoinGecko 호출
async function rateLimitedCoinGeckoCall(url) {
    const now = Date.now();
    const timeSinceLastCall = now - lastCoinGeckoCall;

    if (timeSinceLastCall < COINGECKO_RATE_LIMIT_MS) {
        const waitTime = COINGECKO_RATE_LIMIT_MS - timeSinceLastCall;
        await new Promise(resolve => setTimeout(resolve, waitTime));
    }

    lastCoinGeckoCall = Date.now();

    if (!axios) {
        throw new Error('axios not available');
    }

    return axios.get(url, {
        timeout: 15000,
        validateStatus: (status) => status >= 200 && status < 400
    });
}

// 서버 시작 시 폴백 이미지 초기화
initializeFallbackImage().then();

// Binance 심볼 정보 가져오기 (캐시 사용)
async function getBinanceSymbols() {
    if (binanceSymbolCache) return binanceSymbolCache;
    try {
        if (!axios) {
            throw new Error('axios not available');
        }

        const response = await axios.get(BINANCE_24HR_TICKER_URL, {
            timeout: 10000 // 10초 타임아웃
        });
        binanceSymbolCache = response.data;

        return binanceSymbolCache;
    } catch (error) {
        console.error("Binance API 호출 실패:", error.message);
        return null;
    }
}

// Binance 심볼 유효성 검사
async function isBinanceSymbol(symbol) {
    const binanceData = await getBinanceSymbols();
    if (!binanceData) return false;

    return binanceData.some(ticker => ticker.symbol === symbol.toUpperCase());
}

// 베이스 자산 추출 함수 (더 정확한 패턴 매칭)
function extractBaseAsset(symbol) {
    const upperSymbol = symbol.toUpperCase();

    // 일반적인 quote currency 패턴들을 더 정확하게 매칭
    const quotePatterns = [
        /USDT$/,   // 가장 일반적
        /USDC$/,   // USD Coin
        /BUSD$/,   // Binance USD
        /FDUSD$/,  // First Digital USD
        /TUSD$/,   // TrueUSD
        /BNB$/,    // Binance Coin
        /ETH$/,    // Ethereum
        /BTC$/,    // Bitcoin
        /EUR$/,    // Euro
        /GBP$/,    // British Pound
        /TRY$/,    // Turkish Lira
        /RUB$/,    // Russian Ruble
        /UAH$/,    // Ukrainian Hryvnia
        /BIDR$/,   // Binance IDR
        /BKRW$/,   // Binance KRW
        /DAI$/,    // DAI Stablecoin
        /PAXG$/,   // PAX Gold
        /VAI$/,    // VAI
    ];

    for (const pattern of quotePatterns) {
        if (pattern.test(upperSymbol)) {
            return upperSymbol.replace(pattern, '');
        }
    }

    // 매칭되지 않으면 원본 반환
    return upperSymbol;
}

async function getCoinGeckoList() {
    if (coinGeckoListCache) return coinGeckoListCache;
    try {
        const response = await rateLimitedCoinGeckoCall(COINGECKO_LIST_URL);
        coinGeckoListCache = response.data;

        return coinGeckoListCache;
    } catch (error) {
        console.error("CoinGecko API 호출 실패:", error.message);
        return null;
    }
}

async function findCoinGeckoId(symbolName) {
    const targetSymbol = symbolName.toLowerCase() === 'usdt' ? 'usdt' : symbolName.replace(/usdt$/i, "").toLowerCase();

    if (manualSymbolIdMap[targetSymbol]) {
        return manualSymbolIdMap[targetSymbol];
    }

    const coinList = await getCoinGeckoList();
    if (coinList) {
        const potentialMatches = coinList.filter(coin => coin.symbol === targetSymbol);
        if (potentialMatches.length > 0) {
            let coinEntry = potentialMatches.find(coin => coin.id === targetSymbol);
            if (!coinEntry) coinEntry = potentialMatches[0];
            return coinEntry.id;
        }
    }
    return null;
}

// 통합 로고 검색 함수
async function findLogoUrl(symbol) {

    // 1단계: Binance 심볼인지 확인
    const isBinance = await isBinanceSymbol(symbol);

    if (isBinance) {

        // 베이스 자산 추출 (개선된 함수 사용)
        const baseAsset = extractBaseAsset(symbol);

        // 더 안정적인 로고 소스들 (실제 작동하는 것들 위주)
        const logoPatterns = [
            // CoinGecko assets - 가장 안정적
            `https://coin-images.coingecko.com/coins/images/1/small/${baseAsset.toLowerCase()}.png`,
            `https://assets.coingecko.com/coins/images/1/small/${baseAsset.toLowerCase()}.png`,

            // CoinMarketCap - 상당히 안정적
            `https://s2.coinmarketcap.com/static/img/coins/64x64/1.png`, // Bitcoin ID as fallback

            // Binance 공식 (일부만 작동)
            `https://bin.bnbstatic.com/image/coin/${baseAsset}.png`,
        ];

        // 특별한 심볼들을 위한 하드코딩된 안정적인 URL들
        const reliableUrls = {
            'BTC': [
                'https://coin-images.coingecko.com/coins/images/1/small/bitcoin.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1.png'
            ],
            'ETH': [
                'https://coin-images.coingecko.com/coins/images/279/small/ethereum.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1027.png'
            ],
            'BNB': [
                'https://coin-images.coingecko.com/coins/images/825/small/bnb-icon2_2x.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1839.png'
            ],
            'SOL': [
                'https://coin-images.coingecko.com/coins/images/4128/small/solana.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/5426.png'
            ],
            'ADA': [
                'https://coin-images.coingecko.com/coins/images/975/small/cardano.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/2010.png'
            ],
            'DOGE': [
                'https://coin-images.coingecko.com/coins/images/5/small/dogecoin.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/74.png'
            ],
            'XRP': [
                'https://coin-images.coingecko.com/coins/images/44/small/xrp-symbol-white-128.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/52.png'
            ],
            'DOT': [
                'https://coin-images.coingecko.com/coins/images/12171/small/polkadot.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/6636.png'
            ],
            'AVAX': [
                'https://coin-images.coingecko.com/coins/images/12559/small/Avalanche_Circle_RedWhite_Trans.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/5805.png'
            ],
            'LINK': [
                'https://coin-images.coingecko.com/coins/images/877/small/chainlink-new-logo.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1975.png'
            ],
            'LTC': [
                'https://coin-images.coingecko.com/coins/images/2/small/litecoin.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/2.png'
            ],
            'TRX': [
                'https://coin-images.coingecko.com/coins/images/1094/small/tron-logo.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1958.png'
            ],
            'ATOM': [
                'https://coin-images.coingecko.com/coins/images/1481/small/cosmos_hub.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/3794.png'
            ],
            'NEAR': [
                'https://coin-images.coingecko.com/coins/images/10365/small/near.jpg',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/6535.png'
            ],
            'ETC': [
                'https://coin-images.coingecko.com/coins/images/453/small/ethereum-classic-logo.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/1321.png'
            ],
            'FIL': [
                'https://coin-images.coingecko.com/coins/images/12817/small/filecoin.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/2280.png'
            ],
            'INJ': [
                'https://coin-images.coingecko.com/coins/images/12882/small/Secondary_Symbol.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/7226.png'
            ],
            'SUI': [
                'https://coin-images.coingecko.com/coins/images/26375/small/sui-ocean-square.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/20947.png'
            ],
            'APT': [
                'https://coin-images.coingecko.com/coins/images/26455/small/aptos_round.png',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/21794.png'
            ],
            'ARB': [
                'https://coin-images.coingecko.com/coins/images/16547/small/photo_2023-03-29_21.47.00.jpeg',
                'https://s2.coinmarketcap.com/static/img/coins/64x64/11841.png'
            ]
        };

        // 특별 URL이 있으면 우선 시도
        if (reliableUrls[baseAsset]) {
            logoPatterns.unshift(...reliableUrls[baseAsset]);
        }

        // 각 패턴을 순차적으로 시도 (빠르게 실패하도록 타임아웃 단축)
        for (const pattern of logoPatterns) {
            try {
                if (!axios) {
                    throw new Error('axios not available');
                }

                // HEAD 요청으로 이미지 존재 여부 확인 (타임아웃 단축)
                const headResponse = await axios.head(pattern, {
                    timeout: 5000, // 5초로 단축
                    validateStatus: (status) => status >= 200 && status < 400
                });

                // Content-Type 확인 (이미지인지)
                const contentType = headResponse.headers['content-type'];
                if (contentType && contentType.startsWith('image/')) {
                    return pattern;
                }
            } catch (error) {
            }
        }
    }

    // 2단계: CoinGecko 시도 (Rate limited)
    const coinId = await findCoinGeckoId(symbol);
    if (!coinId) {
        return null;
    }

    try {
        const apiUrl = `https://api.coingecko.com/api/v3/coins/${coinId}`;

        const geckoResponse = await rateLimitedCoinGeckoCall(apiUrl);

        const imageUrl = geckoResponse.data?.image?.small ||
            geckoResponse.data?.image?.thumb ||
            geckoResponse.data?.image?.large;

        if (imageUrl) {
            return imageUrl;
        }
    } catch (error) {
        console.error(`CoinGecko API 호출 실패 (${symbol}):`, error.message);
    }

    return null;
}

async function downloadAndSaveImage(imageUrl, savePath) {
    try {
        const response = await axios({
            url: imageUrl,
            method: 'GET',
            responseType: 'stream',
            timeout: 15000 // 15초 타임아웃
        });

        await fsPromises.mkdir(path.dirname(savePath), {recursive: true});

        const writer = fs.createWriteStream(savePath);
        response.data.pipe(writer);

        return new Promise((resolve, reject) => {
            writer.on('finish', () => {
                resolve();
            });
            writer.on('error', reject);
        });
    } catch (error) {
        console.error(`이미지 다운로드 실패 (${imageUrl}):`, error.message);
        throw error;
    }
}

app.get("/api/get-logo", async (req, res) => {
    const {symbol} = req.query;

    if (!symbol) {
        return res.status(400).send("Symbol query parameter is required.");
    }

    // 파일명에 사용할 안전한 심볼 이름 생성
    const safeSymbolName = symbol.replace(/[^a-zA-Z0-9]/g, '_');
    const localFilePath = path.join(iconDir, `${safeSymbolName}.png`);
    const localFileUrl = `/Backboard/icon/${safeSymbolName}.png`;

    // 이미 로컬에 파일이 존재하는지 확인
    try {
        await fsPromises.access(localFilePath, fs.constants.F_OK);

        return res.json({logoUrl: localFileUrl});
    } catch (error) {
        // 파일이 존재하지 않는 경우 진행
    }

    try {
        // 통합 로고 검색 함수 사용
        const imageUrl = await findLogoUrl(symbol);

        if (!imageUrl) {
            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        // 로컬 저장 디렉토리가 존재하는지 확인하고 없으면 생성
        try {
            await fsPromises.mkdir(path.dirname(localFilePath), {recursive: true});
        } catch (dirError) {
            console.error("디렉토리 생성 실패:", dirError.message);
        }

        // 이미지 다운로드 및 저장
        try {
            await downloadAndSaveImage(imageUrl, localFilePath);
        } catch (downloadError) {
            console.error(`이미지 다운로드 실패 (${symbol}):`, downloadError.message);
            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        // 파일이 정상적으로 저장되었는지 최종 확인
        try {
            const stats = await fsPromises.stat(localFilePath);
            if (stats.size === 0) {
                console.error(`다운로드된 파일 크기가 0입니다: ${localFilePath}`);
                return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
            }
        } catch (statError) {
            console.error("파일 상태 확인 실패:", statError.message);
            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        return res.json({logoUrl: localFileUrl});
    } catch (fetchError) {
        console.error(`전체 로고 처리 실패 (${symbol}):`, fetchError.message);
        return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
    }
});

// -------------------------------
//  새로운 API: /api/get-source-code
// -------------------------------
app.get("/api/get-source-code", async (req, res) => {
    try {
        const {filePath} = req.query;

        if (!filePath) {
            return res.status(400).json({
                error: "파일 경로가 제공되지 않았습니다."
            });
        }

        try {
            // 파일 존재 여부 확인
            await fsPromises.access(filePath, fs.constants.F_OK);

            // 파일 내용 읽기
            const fileContent = await fsPromises.readFile(filePath, 'utf8');

            // 캐시 완전 비활성화 헤더 설정
            res.set({
                'Cache-Control': 'no-cache, no-store, must-revalidate',
                'Pragma': 'no-cache',
                'Expires': '0',
                'ETag': `"${Date.now()}-${Math.random()}"`
            });

            // 파일 내용 반환
            res.json({
                content: fileContent
            });
        } catch (error) {
            // 파일을 찾을 수 없거나 읽기 오류
            res.status(404).json({
                error: `파일을 찾을 수 없거나 읽을 수 없습니다: ${filePath}`,
                details: error.message
            });
        }
    } catch (error) {
        res.status(500).json({
            error: "서버 오류가 발생했습니다.",
            details: error.message
        });
    }
});

// -------------------------------
// 2. 서버 실행 및 브라우저 자동 실행
// -------------------------------
async function startServer() {
    // 사용 가능한 포트 찾기
    port = await findAvailablePort(port);
    console.log(`[INFO] Starting server on port ${port}`);

    return app.listen(port, () => {
        console.log(`[INFO] Server is running on http://localhost:${port}`);

        // 브라우저 실행
        exec(`start http://localhost:${port}`, (err) => {
            if (err) {
                console.error('[ERROR] Failed to open browser:', err.message);
            }
        });
    });
}

// 서버 시작
startServer().then(server => {
    // -------------------------------
    // 3. WebSocket 서버 설정 및 종료 타이머
    // -------------------------------
    const wss = new WebSocketServer({server});

// 서버 종료를 위한 타이머 (모든 클라이언트 연결 종료 후 5초 후 종료)
    let shutdownTimer = null;
    const SHUTDOWN_DELAY = 5000;

    function scheduleShutdown() {
        if (!shutdownTimer) {
            shutdownTimer = setTimeout(() => {
                process.exit();
            }, SHUTDOWN_DELAY);
        }
    }

    function cancelShutdown() {
        if (shutdownTimer) {
            clearTimeout(shutdownTimer);
            shutdownTimer = null;
        }
    }

    wss.on("connection", (ws) => {
        cancelShutdown();

        ws.on("close", () => {
            if (wss.clients.size === 0) {
                scheduleShutdown();
            }
        });
    });

// -------------------------------
// 4. config.json 파싱: 캔들스틱 데이터와 지표 데이터 경로 추출
// -------------------------------
    const configPath = path.join(baseDir, "Backboard", "config.json");
    let config;
    try {
        config = JSON.parse(readFileSync(configPath, "utf8"));
    } catch (e) {
    }

// 캔들스틱 데이터 경로 구성 (심볼 기준)
    const dataPaths = {};
    if (config && config["심볼"]) {
        config["심볼"].forEach((symbol) => {
            if (symbol["트레이딩 바 데이터"] && symbol["트레이딩 바 데이터"]["데이터 경로"]) {
                const symbolName = symbol["심볼 이름"] || "default";

                // 데이터 경로의 마지막 폴더/파일명을 제거
                dataPaths[symbolName] = symbol["트레이딩 바 데이터"]["데이터 경로"].replace(/\/[^\/]*$/, "");
            }
        });
    }

// 지표 데이터 경로 구성
// config의 '전략 및 지표' 배열을 순회하여, 각 전략 내 '지표' 배열의 각 항목에서 데이터 경로 추출
    const indicatorPaths = {};
    if (config && config["지표"]) {
        config["지표"].forEach((indicatorObj) => {
            const indicatorName = indicatorObj["지표 이름"] || "unknown_indicator";
            const indicatorPath = indicatorObj["데이터 경로"] || "";

            // 데이터 경로에서 마지막 폴더/파일명 제거
            indicatorPaths[indicatorName] = indicatorPath.replace(/\/[^\/]*$/, "");
        });
    }

// -------------------------------
// 5. Parquet Reader 캐시 및 파일 정보 캐시
// -------------------------------
// 캔들스틱용과 지표용을 분리하여 캐시 저장
    const parquetReaderCache = {};       // 캔들스틱용
    const indicatorReaderCache = {};     // 지표용
// 심볼별 파일 정보 캐시 추가
    const fileInfoCache = {};           // 심볼별 fileInfos 캐시

// 파일 경로와 캐시 객체를 받아 ParquetReader를 반환하는 함수
    async function getParquetReader(filePath, cache) {
        if (cache[filePath]) {
            return cache[filePath];
        }
        const reader = await parquet.ParquetReader.openFile(filePath);
        cache[filePath] = reader;
        return reader;
    }

// bigint 값을 string으로 변환하기 위한 replacer
    function bigintReplacer(key, value) {
        return typeof value === "bigint" ? value.toString() : value;
    }

// -------------------------------
// 6. 통합 데이터 로드 함수
// -------------------------------

// --- Helper: 파일 기반 캔들 데이터 로드 ---
    async function getCandleDataByFiles(directory, fileRequest) {
        const {type, count, referenceTime} = fileRequest;

        // 디렉토리 경로에서 심볼 이름 추출 (마지막 폴더의 부모 폴더명 사용)
        const pathParts = directory.split(/[/\\]/);
        const symbolName = pathParts[pathParts.length - 2] || "unknown"; // 마지막에서 두 번째 경로 부분 사용

        let fileInfos;

        // 캐시된 fileInfos가 있는지 확인
        if (fileInfoCache[symbolName]) {
            fileInfos = fileInfoCache[symbolName];
        } else {
            // 캐시가 없으면 파일 목록을 가져와서 처리
            try {
                let files = await fsPromises.readdir(directory);

                // 폴더 이름과 동일한 파일명 필터링 (예: "1m" 폴더에 있는 "1m.parquet" 파일 제외)
                const dirName = path.basename(directory);
                files = files.filter((file) => {
                    return file.endsWith(".parquet") && file !== `${dirName}.parquet`;
                });

                fileInfos = files.map(file => {
                    const parts = file.slice(0, -8).split("_"); // 파일명 형식: 시작시간_종료시간.parquet
                    return {file, start: Number(parts[0]), end: Number(parts[1])};
                }).filter(info => !isNaN(info.start) && !isNaN(info.end));

                // 파일 정보 캐시에 저장
                fileInfoCache[symbolName] = fileInfos;
            } catch (err) {
                return [];
            }
        }

        let filesToReadInfo = [];
        if (type === "initial") {
            filesToReadInfo = fileInfos.slice(0, count); // 가장 오래된 파일 'count'개 가져오기
        } else if (type === "newer" && referenceTime) {
            // 최신 데이터: referenceTime 이후 시작하는 파일들 중 가장 오래된 파일 선택
            const relevantFiles = fileInfos.filter(info => info.start > referenceTime);
            filesToReadInfo = relevantFiles.slice(0, count); // referenceTime 이후 파일 중 처음 'count'개 가져오기
        } else if (type === "date" && referenceTime) {
            // 새로운 date 타입 로직: 요청한 시간이 포함된 파일로부터 과거로 count개 파일 요청
            // (그 시간보다 최신 파일은 필요없음)

            // 해당 타임스탬프가 포함된 파일 찾기
            const targetFile = fileInfos.find(info => referenceTime >= info.start && referenceTime <= info.end);

            if (targetFile) {
                const fileIndex = fileInfos.indexOf(targetFile);
                // 타겟 파일부터 과거로 count개 파일 선택 (타겟 파일 포함)
                const startIndex = Math.max(0, fileIndex - (count - 1));
                filesToReadInfo = fileInfos.slice(startIndex, fileIndex + 1);
            } else {
                // 정확히 포함하는 파일이 없으면 가장 가까운 과거 파일부터 count개 선택
                let closestFileIndex = -1;

                // referenceTime보다 이전 시간의 파일들 중 가장 가까운 파일 찾기
                for (let i = fileInfos.length - 1; i >= 0; i--) {
                    if (fileInfos[i].end <= referenceTime) {
                        closestFileIndex = i;
                        break;
                    }
                }

                if (closestFileIndex >= 0) {
                    // 가장 가까운 파일부터 과거로 count개 파일 선택
                    const startIndex = Math.max(0, closestFileIndex - (count - 1));
                    filesToReadInfo = fileInfos.slice(startIndex, closestFileIndex + 1);
                } else {
                    // 해당 시간보다 이전 파일이 없으면 가장 오래된 count개 파일 선택
                    filesToReadInfo = fileInfos.slice(0, Math.min(count, fileInfos.length));
                }
            }
        }

        // 선택된 파일이 없으면 빈 결과 반환
        if (filesToReadInfo.length === 0) {
            return [];
        }

        const allResults = [];
        for (const info of filesToReadInfo) {
            const filePath = path.join(directory, info.file);
            try {
                const reader = await getParquetReader(filePath, parquetReaderCache);
                const cursor = reader.getCursor();
                let record;

                while ((record = await cursor.next())) {
                    let openTimeVal = record["Open Time"];
                    if (typeof openTimeVal === "bigint") openTimeVal = Number(openTimeVal);
                    if (openTimeVal > 1e10) openTimeVal = Math.floor(openTimeVal / 1000);

                    // "newer"의 경우, 데이터가 referenceTime보다 이르지 않도록 확인
                    if (type === "newer" && referenceTime && openTimeVal <= referenceTime) {
                        continue;
                    }

                    allResults.push({
                        time: openTimeVal, open: record["Open"], high: record["High"],
                        low: record["Low"], close: record["Close"], volume: record["Volume"]
                    });
                }
            } catch (err) {
            }
        }

        return allResults;
    }

    // --- Helper: 시간 범위 기반 지표 데이터 로드 ---
    async function getIndicatorDataByTimeRange(directory, timeRange, symbol) {
        if (!timeRange || !timeRange.from || !timeRange.to) {
            return [];
        }

        // 캐시된 fileInfos 가져오기
        const cacheKey = path.basename(directory);
        let fileInfos;

        if (fileInfoCache[cacheKey]) {
            fileInfos = fileInfoCache[cacheKey];
        } else {
            // 캐시가 없으면 파일 목록을 가져와서 처리
            try {
                let files = await fsPromises.readdir(directory);

                // 폴더 이름과 동일한 파일명 필터링
                const dirName = path.basename(directory);
                files = files.filter((file) => {
                    return file.endsWith(".parquet") && file !== `${dirName}.parquet`;
                });

                fileInfos = files.map(file => {
                    const parts = file.slice(0, -8).split("_");
                    return {file, start: Number(parts[0]), end: Number(parts[1])};
                }).filter(info => !isNaN(info.start) && !isNaN(info.end));

                // 파일 정보 캐시에 저장
                fileInfoCache[cacheKey] = fileInfos;
            } catch (err) {
                return [];
            }
        }

        // 요청된 시간 범위와 겹치는 파일들 찾기
        const overlappingFiles = fileInfos.filter(info =>
            // 파일이 요청 범위와 겹치는지 확인 (파일 시작 <= 요청 끝 && 파일 끝 >= 요청 시작)
            info.start <= timeRange.to && info.end >= timeRange.from
        );

        if (overlappingFiles.length === 0) {
            return [];
        }

        // 겹치는 파일들에서 데이터 로드
        const allResults = [];

        for (const info of overlappingFiles) {
            const filePath = path.join(directory, info.file);

            try {
                const reader = await getParquetReader(filePath, indicatorReaderCache);
                const cursor = reader.getCursor();
                let record;
                let recordCount = 0;
                let symbolDataCount = 0;
                let filteredCount = 0;

                while ((record = await cursor.next())) {
                    recordCount++;

                    if (record[symbol] === undefined) {
                        continue; // 해당 심볼 데이터가 없는 경우 스킵
                    }
                    symbolDataCount++;

                    let timeVal = record["time"];
                    if (typeof timeVal === "bigint") timeVal = Number(timeVal);
                    if (timeVal > 1e10) timeVal = Math.floor(timeVal / 1000);

                    // 요청된 시간 범위 내의 데이터만 필터링
                    if (timeVal >= timeRange.from && timeVal <= timeRange.to) {
                        // 값 변환
                        let value;

                        if (record[symbol] === null || record[symbol] === undefined) {
                            value = null;  // null은 그대로 전송
                        } else if (typeof record[symbol] === 'string') {
                            // 문자열인 경우
                            if (record[symbol] === 'NaN') {
                                value = NaN;  // "NaN" 문자열을 NaN으로 변환
                            } else {
                                value = parseFloat(record[symbol]);  // 숫자 문자열을 숫자로 파싱
                            }
                        } else {
                            // 기타 타입은 숫자로 변환
                            value = Number(record[symbol]);
                        }

                        allResults.push({time: timeVal, value: value});
                        filteredCount++;
                    }
                }

            } catch (err) {
            }
        }

        // 시간순으로 정렬
        allResults.sort((a, b) => a.time - b.time);
        return allResults;
    }

// 통합 데이터 로딩 함수
    async function handleLoadChartData(ws, msg) {
        const {symbol, indicators, fileRequest} = msg;

        const candleDirectory = dataPaths[symbol];
        let candleDataPromise;

        if (!candleDirectory) {
            candleDataPromise = Promise.resolve({error: "캔들스틱 데이터 경로 없음", data: []});
        } else if (!fileRequest || !fileRequest.type) {
            candleDataPromise = Promise.resolve({error: "캔들스틱 요청 파라미터 오류 (fileRequest)", data: []});
        } else {
            candleDataPromise = getCandleDataByFiles(candleDirectory, fileRequest)
                .then(data => {
                    return {data};
                })
                .catch(err => {
                    return {error: `캔들스틱 데이터(file, ${fileRequest.type}) 처리 오류: ${err.message}`, data: []};
                });
        }

        const indicatorPromises = indicators.map(async (indicatorName) => {
            const indicatorDir = indicatorPaths[indicatorName];

            if (!indicatorDir) {
                return {indicatorName, error: "해당 지표 데이터 경로 없음", data: []};
            }
            if (!fileRequest || !fileRequest.type) {
                return {indicatorName, error: "지표 요청 파라미터 오류 (fileRequest)", data: []};
            }
            try {
                // 캔들 데이터 먼저 로드하여 시간 범위 계산
                const candleResult = await candleDataPromise;
                let indicatorTimeRange = null;

                if (candleResult.data && candleResult.data.length > 0) {
                    indicatorTimeRange = {
                        from: candleResult.data[0].time,
                        to: candleResult.data[candleResult.data.length - 1].time
                    };
                }

                const data = await getIndicatorDataByTimeRange(indicatorDir, indicatorTimeRange, symbol);
                return {indicatorName, data};
            } catch (err) {
                return {indicatorName, error: `지표 데이터 처리 오류: ${err.message}`, data: []};
            }
        });

        try {
            const [candleResult, ...indicatorResults] = await Promise.all([candleDataPromise, ...indicatorPromises]);

            // 지표 데이터 필터링 및 추가 로드 처리 함수
            const processIndicatorResults = async () => {
                const filteredIndicatorResults = [];

                for (const indResult of indicatorResults) {
                    if (!indResult.data || indResult.data.length === 0) {
                        filteredIndicatorResults.push(indResult);
                        continue;
                    }

                    // 이미 정확한 시간 범위의 데이터만 로드되었으므로 추가 필터링 불필요
                    filteredIndicatorResults.push({
                        ...indResult,
                        data: indResult.data // 그대로 사용
                    });
                }

                return filteredIndicatorResults;
            };

            // 지표 데이터 처리 실행
            const filteredIndicatorResults = await processIndicatorResults();

            const response = {
                action: "loadChartDataResponse",
                candleData: candleResult.data || [],
                indicatorResults: filteredIndicatorResults,
            };

            const serializedResponse = JSON.stringify(response, bigintReplacer);
            ws.send(serializedResponse);

        } catch (err) {
            ws.send(JSON.stringify({
                action: "loadChartDataResponse",
                error: "통합 데이터 처리 중 전체 오류",
                candleData: [],
                indicatorResults: indicators.map(name => ({indicatorName: name, error: "전체 오류로 데이터 못 가져옴", data: []})),
            }));
        }
    }

// -------------------------------
// 8. WebSocket 메시지 핸들러 등록
// -------------------------------
    wss.on("connection", (ws) => {
        ws.on("message", async (message) => {
            try {
                const msg = JSON.parse(message);

                switch (msg.action) {
                    case "loadChartData": // 새로운 통합 액션 핸들러
                        await handleLoadChartData(ws, msg);
                        break;
                    // 기존 loadMore, loadIndicator, loadIndicators 케이스들은 삭제
                    default:
                        ws.send(JSON.stringify({error: "알 수 없는 action"}));
                        break;
                }
            } catch (err) {
                ws.send(JSON.stringify({error: "메시지 처리 오류"}));
            }
        });
    });

// -------------------------------
// 9. 강제 종료 API
// -------------------------------
    function forceShutdown() {
        process.exit(0);
    }

    app.get("/force-shutdown", (req, res) => {
        res.send("서버가 강제 종료됩니다.");
        forceShutdown();
    });

// -------------------------------
// 10. index.html 라우팅 (다른 모든 라우트 뒤)
// -------------------------------
    app.get("*", (req, res) => {
        res.sendFile(path.join(distPath, "index.html"));
    });

}).catch(err => {
    console.error('[ERROR] Failed to start server:', err);
    process.exit(1);
});
