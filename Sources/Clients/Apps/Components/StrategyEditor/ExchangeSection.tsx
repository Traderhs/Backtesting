import {Input} from '@/Components/UI/Input';
import {useStrategy} from './StrategyContext';

export default function ExchangeSection() {
    const {exchangeConfig, setExchangeConfig, lastDataUpdates} = useStrategy();

    const updateConfig = (key: keyof typeof exchangeConfig, value: string) => {
        setExchangeConfig(prev => ({...prev, [key]: value}));
    };

    return (
        <div className="mb-6 p-4 bg-[#071029] border border-gray-700 rounded-lg">
            <h2 className="text-lg font-semibold text-white mb-4">거래소 설정</h2>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                {/* API 환경 변수 */}
                <div className="space-y-4">
                    <h3 className="text-sm font-medium text-gray-200">API 환경 변수</h3>
                    <p className="text-xs text-gray-400">환경 변수 이름만 저장합니다. 비밀값은 시스템 환경 변수로 관리하세요.</p>

                    <div>
                        <label className="text-xs text-gray-300 block mb-1">API 키 환경 변수</label>
                        <Input type="text" value={exchangeConfig.apiKeyEnvVar}
                               onChange={(e) => updateConfig('apiKeyEnvVar', e.currentTarget.value)}
                               placeholder="예: BINANCE_API_KEY"
                               className="bg-[#050a12] border-gray-600 w-full"/>
                    </div>
                    <div>
                        <label className="text-xs text-gray-300 block mb-1">API 시크릿 환경 변수</label>
                        <Input type="text" value={exchangeConfig.apiSecretEnvVar}
                               onChange={(e) => updateConfig('apiSecretEnvVar', e.currentTarget.value)}
                               placeholder="예: BINANCE_API_SECRET"
                               className="bg-[#050a12] border-gray-600 w-full"/>
                    </div>
                </div>

                {/* 파일 경로 및 업데이트 정보 */}
                <div className="space-y-4">
                    <h3 className="text-sm font-medium text-gray-200">파일 경로 및 정보</h3>
                    <p className="text-xs text-gray-400">거래소 정보 및 레버리지 구간 파일 경로</p>

                    <div>
                        <label className="text-xs text-gray-300 block mb-1">거래소 정보 파일 경로</label>
                        <Input type="text" value={exchangeConfig.exchangeInfoPath}
                               onChange={(e) => updateConfig('exchangeInfoPath', e.currentTarget.value)}
                               placeholder="예: Data/exchange_info.json"
                               className="bg-[#050a12] border-gray-600 w-full"/>
                    </div>
                    <div>
                        <label className="text-xs text-gray-300 block mb-1">레버리지 구간 파일 경로</label>
                        <Input type="text" value={exchangeConfig.leverageBracketPath}
                               onChange={(e) => updateConfig('leverageBracketPath', e.currentTarget.value)}
                               placeholder="예: Data/leverage_bracket.json"
                               className="bg-[#050a12] border-gray-600 w-full"/>
                    </div>
                    <div>
                        <label className="text-xs text-gray-300 block mb-1">마지막 데이터 업데이트</label>
                        <div
                            className="px-3 py-2 bg-[#050a12] border border-gray-600 rounded-md text-sm text-gray-300 min-h-[40px] flex items-center">
                            {lastDataUpdates || "업데이트 정보 없음"}
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}
