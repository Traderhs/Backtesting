const fsPromises = require("fs").promises;
const path = require("path");

// PKG 환경에서 axios 로드
const processWithPkg = process;
let axios;
try {
    if (processWithPkg.pkg) {
        axios = require('axios/dist/node/axios.cjs');
    } else {
        axios = require('axios');
    }
} catch (e) {
    try {
        axios = require('axios');
    } catch (e2) {
        axios = null;
    }
}

const parquet = require("@dsnp/parquetjs");

// =====================================================================================================================
// 로고 관련
// =====================================================================================================================

const manualSymbolIdMap = {
    'btc': 'bitcoin', 'eth': 'ethereum', 'usdt': 'tether', 'bnb': 'binancecoin',
    'sol': 'solana', 'usdc': 'usd-coin', 'xrp': 'ripple', 'doge': 'dogecoin',
    'ton': 'the-open-network', 'ada': 'cardano', 'shib': 'shiba-inu',
    'avax': 'avalanche-2', 'trx': 'tron', 'dot': 'polkadot', 'link': 'chainlink',
    'wbtc': 'wrapped-bitcoin', 'bch': 'bitcoin-cash', 'near': 'near',
    'matic': 'matic-network', 'ltc': 'litecoin', 'icp': 'internet-computer',
    'uni': 'uniswap', 'dai': 'dai', 'leo': 'leo-token', 'steth': 'staked-ether',
    'kas': 'kaspa', 'etc': 'ethereum-classic', 'op': 'optimism', 'xlm': 'stellar',
    'okb': 'okb', 'inj': 'injective-protocol', 'fil': 'filecoin', 'imx': 'immutable-x',
    'hbar': 'hedera-hashgraph', 'cro': 'crypto-com-chain', 'tao': 'bittensor',
    'apt': 'aptos', 'vet': 'vechain', 'ldo': 'lido-dao', 'mkr': 'maker',
    'rndr': 'render-token', 'grt': 'the-graph', 'arb': 'arbitrum', 'atom': 'cosmos',
    'mnt': 'mantle', 'tia': 'celestia', 'ethfi': 'ether-fi', 'fet': 'fetch-ai',
    'sei': 'sei-network', 'weth': 'weth', 'stx': 'blockstack', 'aave': 'aave',
    'theta': 'theta-token', 'reth': 'rocket-pool-eth', 'algo': 'algorand',
    'floki': 'floki', 'sui': 'sui', 'fdusd': 'first-digital-usd', 'ftm': 'fantom',
    'ena': 'ethena', 'axs': 'axie-infinity', 'core': 'core-dao', 'bonk': 'bonk',
    'snx': 'havven', 'zeta': 'zetachain', 'bgb': 'bitget-token',
    'agix': 'singularitynet', 'xtz': 'tezos', 'pyth': 'pyth-network',
    'beam': 'beam-2', 'bsv': 'bitcoin-sv', 'jup': 'jupiter-exchange-solana',
    'kava': 'kava', 'chz': 'chiliz', 'ordi': 'ordinals', 'wld': 'worldcoin-wld',
    'wemix': 'wemix-token', 'ocean': 'ocean-protocol', 'hnt': 'helium',
    'gala': 'gala', 'eos': 'eos', 'mina': 'mina-protocol', 'astr': 'astar',
    'rose': 'oasis-network', 'pepe': 'pepe', 'cfx': 'conflux-token',
    'xdc': 'xdce-network', 'neo': 'neo', 'flow': 'flow', 'rune': 'thorchain',
    'kcs': 'kucoin-shares', 'akt': 'akash-network', 'sand': 'the-sandbox',
    'xec': 'ecash', 'iota': 'iota', 'blur': 'blur-token', 'usdd': 'usdd',
    'gt': 'gatetoken', 'ar': 'arweave', 'osmo': 'osmosis', 'mana': 'decentraland',
    'zec': 'zcash', 'klay': 'klay-token', 'egld': 'elrond-erd-2',
};

let binanceSymbolCache = null;
let coinGeckoListCache = null;
let lastCoinGeckoCall = 0;
const COINGECKO_RATE_LIMIT_MS = 2000;

const BINANCE_24HR_TICKER_URL = "https://api.binance.com/api/v3/ticker/24hr";
const COINGECKO_LIST_URL = "https://api.coingecko.com/api/v3/coins/list";
const USDT_FALLBACK_ICON_PATH = "/BackBoard/icon/USDT_fallback.png";
const USDT_LOGO_URLS = [
    "https://coin-images.coingecko.com/coins/images/325/small/Tether.png",
    "https://s2.coinmarketcap.com/static/img/coins/64x64/825.png",
    "https://assets.coingecko.com/coins/images/325/thumb/Tether-logo.png"
];

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

    return axios.get(url, {timeout: 15000, validateStatus: (status) => status >= 200 && status < 400});
}

async function getBinanceSymbols() {
    if (binanceSymbolCache) {
        return binanceSymbolCache;
    }

    try {
        const response = await axios.get(BINANCE_24HR_TICKER_URL, {timeout: 10000});
        binanceSymbolCache = response.data;

        return binanceSymbolCache;
    } catch (error) {
        return null;
    }
}

async function isBinanceSymbol(symbol) {
    const binanceData = await getBinanceSymbols();
    if (!binanceData) {
        return false;
    }

    return binanceData.some(ticker => ticker.symbol === symbol.toUpperCase());
}

function extractBaseAsset(symbol) {
    const upperSymbol = symbol.toUpperCase();
    const quotePatterns = [
        /USDT$/, /USDC$/, /BUSD$/, /FDUSD$/, /TUSD$/, /BNB$/, /ETH$/, /BTC$/,
        /EUR$/, /GBP$/, /TRY$/, /RUB$/, /UAH$/, /BIDR$/, /BKRW$/, /DAI$/, /PAXG$/, /VAI$/
    ];

    for (const pattern of quotePatterns) {
        if (pattern.test(upperSymbol)) {
            return upperSymbol.replace(pattern, '');
        }
    }
    return upperSymbol;
}

async function getCoinGeckoList() {
    if (coinGeckoListCache) {
        return coinGeckoListCache;
    }

    try {
        const response = await rateLimitedCoinGeckoCall(COINGECKO_LIST_URL);
        coinGeckoListCache = response.data;

        return coinGeckoListCache;
    } catch (error) {
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
            if (!coinEntry) {
                coinEntry = potentialMatches[0];
            }

            return coinEntry.id;
        }
    }
    return null;
}

async function findLogoUrl(symbol) {
    const isBinance = await isBinanceSymbol(symbol);

    if (isBinance) {
        const baseAsset = extractBaseAsset(symbol);
        const reliableUrls = {
            'BTC': ['https://coin-images.coingecko.com/coins/images/1/small/bitcoin.png'],
            'ETH': ['https://coin-images.coingecko.com/coins/images/279/small/ethereum.png'],
            'BNB': ['https://coin-images.coingecko.com/coins/images/825/small/bnb-icon2_2x.png'],
            'SOL': ['https://coin-images.coingecko.com/coins/images/4128/small/solana.png'],
            'ADA': ['https://coin-images.coingecko.com/coins/images/975/small/cardano.png'],
            'DOGE': ['https://coin-images.coingecko.com/coins/images/5/small/dogecoin.png'],
            'XRP': ['https://coin-images.coingecko.com/coins/images/44/small/xrp-symbol-white-128.png'],
            'DOT': ['https://coin-images.coingecko.com/coins/images/12171/small/polkadot.png'],
            'AVAX': ['https://coin-images.coingecko.com/coins/images/12559/small/Avalanche_Circle_RedWhite_Trans.png'],
            'LINK': ['https://coin-images.coingecko.com/coins/images/877/small/chainlink-new-logo.png'],
            'LTC': ['https://coin-images.coingecko.com/coins/images/2/small/litecoin.png'],
            'TRX': ['https://coin-images.coingecko.com/coins/images/1094/small/tron-logo.png'],
            'ATOM': ['https://coin-images.coingecko.com/coins/images/1481/small/cosmos_hub.png'],
            'NEAR': ['https://coin-images.coingecko.com/coins/images/10365/small/near.jpg'],
            'ETC': ['https://coin-images.coingecko.com/coins/images/453/small/ethereum-classic-logo.png'],
            'FIL': ['https://coin-images.coingecko.com/coins/images/12817/small/filecoin.png'],
            'INJ': ['https://coin-images.coingecko.com/coins/images/12882/small/Secondary_Symbol.png'],
            'SUI': ['https://coin-images.coingecko.com/coins/images/26375/small/sui-ocean-square.png'],
            'APT': ['https://coin-images.coingecko.com/coins/images/26455/small/aptos_round.png'],
            'ARB': ['https://coin-images.coingecko.com/coins/images/16547/small/photo_2023-03-29_21.47.00.jpeg']
        };

        const logoPatterns = [];
        if (reliableUrls[baseAsset]) {
            logoPatterns.push(...reliableUrls[baseAsset]);
        }

        logoPatterns.push(
            `https://coin-images.coingecko.com/coins/images/1/small/${baseAsset.toLowerCase()}.png`,
            `https://bin.bnbstatic.com/image/coin/${baseAsset}.png`
        );

        try {
            for (const pattern of logoPatterns) {
                const headResponse = await axios.head(pattern, {
                    timeout: 5000,
                    validateStatus: (status) => status >= 200 && status < 400
                });

                const contentType = headResponse.headers['content-type'];
                if (contentType && contentType.startsWith('image/')) {
                    return pattern;
                }
            }
        } catch (error) {
        }
    }

    const coinId = await findCoinGeckoId(symbol);
    if (!coinId) {
        return null;
    }

    try {
        const apiUrl = `https://api.coingecko.com/api/v3/coins/${coinId}`;
        const geckoResponse = await rateLimitedCoinGeckoCall(apiUrl);
        const imageUrl = geckoResponse.data?.image?.small || geckoResponse.data?.image?.thumb || geckoResponse.data?.image?.large;

        if (imageUrl) {
            return imageUrl;
        }
    } catch (error) {
    }

    return null;
}

async function downloadAndSaveImage(imageUrl, savePath) {
    try {
        const response = await axios({url: imageUrl, method: 'GET', responseType: 'stream', timeout: 15000});
        await fsPromises.mkdir(path.dirname(savePath), {recursive: true});
        const writer = require("fs").createWriteStream(savePath);
        response.data.pipe(writer);

        return new Promise((resolve, reject) => {
            writer.on('finish', () => resolve());
            writer.on('error', reject);
        });
    } catch (error) {
        throw error;
    }
}

async function initializeFallbackImage(iconDir) {
    const fallbackPath = path.join(iconDir, "USDT_fallback.png");
    try {
        await fsPromises.access(fallbackPath, require("fs").constants.F_OK);
        return;
    } catch (error) {
        // 무시
    }

    for (const url of USDT_LOGO_URLS) {
        try {
            await fsPromises.mkdir(path.dirname(fallbackPath), {recursive: true});
            await downloadAndSaveImage(url, fallbackPath);

            return;
        } catch (downloadError) {
        }
    }
}

// =====================================================================================================================
// 캔들 및 지표 데이터 로드
// =====================================================================================================================

const parquetReaderCache = {};
const indicatorReaderCache = {};
const fileInfoCache = {};

async function getParquetReader(filePath, cache) {
    if (cache[filePath]) {
        return cache[filePath];
    }

    const reader = await parquet.ParquetReader.openFile(filePath);
    cache[filePath] = reader;

    return reader;
}

async function getCandleDataByFiles(directory, fileRequest) {
    const {type, count, referenceTime} = fileRequest;
    const pathParts = directory.split(/[/\\]/);
    const symbolName = pathParts[pathParts.length - 2] || "unknown";

    let fileInfos;
    if (fileInfoCache[symbolName]) {
        fileInfos = fileInfoCache[symbolName];
    } else {
        try {
            let files = await fsPromises.readdir(directory);
            const dirName = path.basename(directory);

            files = files.filter((file) => file.endsWith(".parquet") && file !== `${dirName}.parquet`);
            fileInfos = files.map(file => {
                const parts = file.slice(0, -8).split("_");
                return {file, start: Number(parts[0]), end: Number(parts[1])};
            }).filter(info => !isNaN(info.start) && !isNaN(info.end));
            fileInfoCache[symbolName] = fileInfos;
        } catch (err) {
            return [];
        }
    }

    let filesToReadInfo = [];
    if (type === "initial") {
        filesToReadInfo = fileInfos.slice(0, count);
    } else if (type === "newer" && referenceTime) {
        const relevantFiles = fileInfos.filter(info => info.start > referenceTime);

        filesToReadInfo = relevantFiles.slice(0, count);
    } else if (type === "date" && referenceTime) {
        const targetFile = fileInfos.find(info => referenceTime >= info.start && referenceTime <= info.end);
        if (targetFile) {
            const fileIndex = fileInfos.indexOf(targetFile);
            const startIndex = Math.max(0, fileIndex - (count - 1));

            filesToReadInfo = fileInfos.slice(startIndex, fileIndex + 1);
        } else {
            let closestFileIndex = -1;
            for (let i = fileInfos.length - 1; i >= 0; i--) {
                if (fileInfos[i].end <= referenceTime) {
                    closestFileIndex = i;
                    break;
                }
            }

            if (closestFileIndex >= 0) {
                const startIndex = Math.max(0, closestFileIndex - (count - 1));

                filesToReadInfo = fileInfos.slice(startIndex, closestFileIndex + 1);
            } else {
                filesToReadInfo = fileInfos.slice(0, Math.min(count, fileInfos.length));
            }
        }
    }

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

                if (typeof openTimeVal === "bigint") {
                    openTimeVal = Number(openTimeVal);
                }

                if (openTimeVal > 1e10) {
                    openTimeVal = Math.floor(openTimeVal / 1000);
                }

                if (type === "newer" && referenceTime && openTimeVal <= referenceTime) {
                    continue;
                }

                allResults.push({
                    time: openTimeVal,
                    open: record["Open"],
                    high: record["High"],
                    low: record["Low"],
                    close: record["Close"],
                    volume: record["Volume"]
                });
            }
        } catch (err) {
        }
    }
    return allResults;
}

async function getIndicatorDataByTimeRange(directory, timeRange, symbol) {
    if (!timeRange || !timeRange.from || !timeRange.to) {
        return [];
    }

    const cacheKey = path.basename(directory);
    let fileInfos;
    if (fileInfoCache[cacheKey]) {
        fileInfos = fileInfoCache[cacheKey];
    } else {
        try {
            let files = await fsPromises.readdir(directory);
            const dirName = path.basename(directory);
            files = files.filter((file) => file.endsWith(".parquet") && file !== `${dirName}.parquet`);
            fileInfos = files.map(file => {
                const parts = file.slice(0, -8).split("_");
                return {file, start: Number(parts[0]), end: Number(parts[1])};
            }).filter(info => !isNaN(info.start) && !isNaN(info.end));
            fileInfoCache[cacheKey] = fileInfos;
        } catch (err) {
            return [];
        }
    }

    const overlappingFiles = fileInfos.filter(info => info.start <= timeRange.to && info.end >= timeRange.from);
    if (overlappingFiles.length === 0) {
        return [];
    }

    const allResults = [];
    for (const info of overlappingFiles) {
        const filePath = path.join(directory, info.file);
        try {
            const reader = await getParquetReader(filePath, indicatorReaderCache);
            const cursor = reader.getCursor();

            let record;
            while ((record = await cursor.next())) {
                if (record[symbol] === undefined) {
                    continue;
                }

                let timeVal = record["time"];

                if (typeof timeVal === "bigint") {
                    timeVal = Number(timeVal);
                }

                if (timeVal > 1e10) {
                    timeVal = Math.floor(timeVal / 1000);
                }

                if (timeVal >= timeRange.from && timeVal <= timeRange.to) {
                    let value;
                    if (record[symbol] === null || record[symbol] === undefined) {
                        value = null;
                    } else if (typeof record[symbol] === 'string') {
                        if (record[symbol] === 'NaN') {
                            value = NaN;
                        } else {
                            value = parseFloat(record[symbol]);
                        }
                    } else {
                        value = Number(record[symbol]);
                    }

                    allResults.push({time: timeVal, value: value});
                }
            }
        } catch (err) {
        }
    }
    allResults.sort((a, b) => a.time - b.time);
    return allResults;
}

async function handleLoadChartData(ws, msg, dataPaths, indicatorPaths) {
    const {symbol, indicators, fileRequest} = msg;
    const candleDirectory = dataPaths[symbol];

    let candleDataPromise;
    if (!candleDirectory) {
        candleDataPromise = Promise.resolve({error: "캔들스틱 데이터 경로 없음", data: []});
    } else if (!fileRequest || !fileRequest.type) {
        candleDataPromise = Promise.resolve({error: "캔들스틱 요청 파라미터 오류 (fileRequest)", data: []});
    } else {
        candleDataPromise = getCandleDataByFiles(candleDirectory, fileRequest)
            .then(data => ({data}))
            .catch(err => ({error: `캔들스틱 데이터(file, ${fileRequest.type}) 처리 오류: ${err.message}`, data: []}));
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
            let indicatorTimeRange = null;
            const candleResult = await candleDataPromise;
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
        const filteredIndicatorResults = indicatorResults.map(indResult => ({
            ...indResult,
            data: indResult.data || []
        }));

        const response = {
            action: "loadChartDataResponse",
            candleData: candleResult.data || [],
            indicatorResults: filteredIndicatorResults,
        };

        const bigintReplacer = (key, value) => typeof value === "bigint" ? value.toString() : value;

        ws.send(JSON.stringify(response, bigintReplacer));
    } catch (err) {
        ws.send(JSON.stringify({
            action: "loadChartDataResponse",
            error: "통합 데이터 처리 중 전체 오류",
            candleData: [],
            indicatorResults: indicators.map(name => ({indicatorName: name, error: "전체 오류로 데이터 못 가져옴", data: []})),
        }));
    }
}

module.exports = {
    initializeFallbackImage,
    findLogoUrl,
    downloadAndSaveImage,
    handleLoadChartData,
    USDT_FALLBACK_ICON_PATH
};
