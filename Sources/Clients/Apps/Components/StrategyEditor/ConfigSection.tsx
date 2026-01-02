import {useState} from 'react';
import {Button} from '@/Components/UI/Button';
import {Input} from '@/Components/UI/Input';
import {useStrategy} from './StrategyContext';

export default function ConfigSection() {
    const {engineConfig, setEngineConfig, addLog} = useStrategy();

    const [showProjectDialog, setShowProjectDialog] = useState(false);
    const [projectDirectoryInput, setProjectDirectoryInput] = useState('');

    // 프로젝트 디렉토리 설정 핸들러
    const handleSetProjectDirectory = () => {
        if (!projectDirectoryInput.trim()) {
            addLog('ERROR', '프로젝트 디렉토리 경로를 입력해 주세요.');
            return;
        }

        setEngineConfig(prev => ({...prev, projectDirectory: projectDirectoryInput.trim()}));
        setShowProjectDialog(false);
    };

    // 돋보기 바 사용 여부 토글
    const toggleBarMagnifier = (checked: boolean) => {
        setEngineConfig(prev => ({...prev, useBarMagnifier: checked}));
    };

    // 입력 핸들러들
    const handleConfigChange = (field: keyof typeof engineConfig, value: any) => {
        setEngineConfig(prev => ({...prev, [field]: value}));
    };

    return (
        <div className="mb-6 p-4 bg-[#071029] border border-gray-700 rounded-lg">
            <h2 className="text-lg font-semibold text-white mb-4">엔진 설정</h2>

            <div className="space-y-4">
                {/* 프로젝트 디렉토리 설정 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-1">프로젝트 루트 디렉토리</label>
                    <div className="flex gap-2">
                        <Input
                            type="text"
                            value={engineConfig.projectDirectory}
                            readOnly
                            placeholder="프로젝트 루트 디렉토리를 설정하세요"
                            className="bg-[#050a12] border-gray-600 flex-1"
                        />
                        <Button
                            onClick={() => {
                                setProjectDirectoryInput(engineConfig.projectDirectory);
                                setShowProjectDialog(true);
                            }}
                            className="bg-blue-600 hover:bg-blue-700 text-white"
                        >
                            설정
                        </Button>
                    </div>
                    <p className="text-xs text-gray-400 mt-1">
                        데이터 파일 경로의 기준이 되는 디렉토리입니다. (예: D:/Programming/Backtesting)
                    </p>
                </div>

                {/* 돋보기 바 설정 */}
                <div className="flex items-center space-x-2">
                    <input
                        type="checkbox"
                        id="useBarMagnifier"
                        checked={engineConfig.useBarMagnifier}
                        onChange={(e) => toggleBarMagnifier(e.target.checked)}
                        className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                    />
                    <label
                        htmlFor="useBarMagnifier"
                        className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70 text-gray-200"
                    >
                        바 돋보기(Bar Magnifier) 사용
                    </label>
                </div>
                <p className="text-xs text-gray-400 ml-6">
                    트레이딩 바 내부를 더 작은 타임프레임으로 시뮬레이션하여 체결 정확도를 높입니다.
                </p>

                {/* 백테스팅 기간 설정 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">백테스팅 기간</label>
                    <div className="grid grid-cols-2 gap-4">
                        <div>
                            <div className="flex items-center space-x-2 mb-1">
                                <input
                                    type="checkbox"
                                    id="useBacktestPeriodStart"
                                    checked={engineConfig.useBacktestPeriodStart}
                                    onChange={(e) => {
                                        handleConfigChange('useBacktestPeriodStart', e.target.checked);
                                        if (e.target.checked) {
                                            handleConfigChange('backtestPeriodStart', '');
                                        }
                                    }}
                                    className="w-3.5 h-3.5 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                                />

                                <label htmlFor="useBacktestPeriodStart" className="text-xs text-gray-400">
                                    처음부터
                                </label>
                            </div>
                            <Input
                                type="text"
                                value={engineConfig.backtestPeriodStart}
                                onChange={(e) => {
                                    const value = e.target.value;
                                    handleConfigChange('backtestPeriodStart', value);
                                    if (value.trim()) {
                                        handleConfigChange('useBacktestPeriodStart', false);
                                    } else {
                                        handleConfigChange('useBacktestPeriodStart', true);
                                    }
                                }}
                                placeholder="예: 2023-01-01 00:00:00"
                                className="bg-[#050a12] border-gray-600 text-sm"
                            />
                        </div>
                        <div>
                            <div className="flex items-center space-x-2 mb-1">
                                <input
                                    type="checkbox"
                                    id="useBacktestPeriodEnd"
                                    checked={engineConfig.useBacktestPeriodEnd}
                                    onChange={(e) => {
                                        handleConfigChange('useBacktestPeriodEnd', e.target.checked);
                                        if (e.target.checked) {
                                            handleConfigChange('backtestPeriodEnd', '');
                                        }
                                    }}
                                    className="w-3.5 h-3.5 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                                />

                                <label htmlFor="useBacktestPeriodEnd" className="text-xs text-gray-400">
                                    끝까지
                                </label>
                            </div>
                            <Input
                                type="text"
                                value={engineConfig.backtestPeriodEnd}
                                onChange={(e) => {
                                    const value = e.target.value;
                                    handleConfigChange('backtestPeriodEnd', value);
                                    if (value.trim()) {
                                        handleConfigChange('useBacktestPeriodEnd', false);
                                    } else {
                                        handleConfigChange('useBacktestPeriodEnd', true);
                                    }
                                }}
                                placeholder="예: 2024-12-31 23:59:59"
                                className="bg-[#050a12] border-gray-600 text-sm"
                            />
                        </div>
                    </div>
                    <p className="text-xs text-gray-400 mt-1">
                        체크 시 데이터 처음/끝부터 백테스팅합니다.
                    </p>
                </div>

                {/* 초기 자금 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-1">초기 자금 (USDT)</label>
                    <Input
                        type="number"
                        value={engineConfig.initialBalance}
                        onChange={(e) => handleConfigChange('initialBalance', parseFloat(e.target.value) || undefined)}
                        className="bg-[#050a12] border-gray-600"
                    />
                </div>

                {/* 수수료 설정 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">수수료 설정 (%)</label>
                    <div className="grid grid-cols-2 gap-2">
                        <div>
                            <label className="text-xs text-gray-400 block mb-1">테이커</label>
                            <Input
                                type="number"
                                value={engineConfig.takerFeePercentage}
                                onChange={(e) => handleConfigChange('takerFeePercentage', parseFloat(e.target.value) || undefined)}
                                className="bg-[#050a12] border-gray-600 text-sm"
                            />
                        </div>

                        <div>
                            <label className="text-xs text-gray-400 block mb-1">메이커</label>
                            <Input
                                type="number"
                                value={engineConfig.makerFeePercentage}
                                onChange={(e) => handleConfigChange('makerFeePercentage', parseFloat(e.target.value) || undefined)}
                                className="bg-[#050a12] border-gray-600 text-sm"
                            />
                        </div>
                    </div>
                    <p className="text-xs text-gray-400 mt-1">
                        퍼센트로 입력 (0.045% → 0.045 입력)
                    </p>
                </div>

                {/* 슬리피지 설정 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">슬리피지 설정</label>
                    <div className="mb-2">
                        <select
                            value={engineConfig.slippageModel}
                            onChange={(e) => handleConfigChange('slippageModel', e.target.value as 'PercentageSlippage' | 'MarketImpactSlippage')}
                            className="w-full bg-[#050a12] border-gray-600 text-white rounded px-3 py-2 text-sm"
                        >
                            <option value="PercentageSlippage">퍼센트 슬리피지 (Percentage)</option>
                            <option value="MarketImpactSlippage">시장 충격 슬리피지 (MarketImpact)</option>
                        </select>
                    </div>

                    {engineConfig.slippageModel === 'PercentageSlippage' ? (
                        <div className="grid grid-cols-2 gap-2">
                            <div>
                                <label className="text-xs text-gray-400 block mb-1">테이커 (%)</label>
                                <Input
                                    type="number"
                                    value={engineConfig.slippageTakerPercentage}
                                    onChange={(e) => handleConfigChange('slippageTakerPercentage', parseFloat(e.target.value) || undefined)}
                                    className="bg-[#050a12] border-gray-600 text-sm"
                                />
                            </div>

                            <div>
                                <label className="text-xs text-gray-400 block mb-1">메이커 (%)</label>
                                <Input
                                    type="number"
                                    value={engineConfig.slippageMakerPercentage}
                                    onChange={(e) => handleConfigChange('slippageMakerPercentage', parseFloat(e.target.value) || undefined)}
                                    className="bg-[#050a12] border-gray-600 text-sm"
                                />
                            </div>
                        </div>
                    ) : (
                        <div>
                            <label className="text-xs text-gray-400 block mb-1">스트레스 계수</label>
                            <Input
                                type="number"
                                value={engineConfig.slippageStressMultiplier}
                                onChange={(e) => handleConfigChange('slippageStressMultiplier', parseFloat(e.target.value) || undefined)}
                                className="bg-[#050a12] border-gray-600 text-sm"
                            />
                            <p className="text-xs text-gray-400 mt-1">
                                시장 충격 기반 동적 슬리피지 계산 (권장: 1.5~3)
                            </p>
                        </div>
                    )}
                </div>

                {/* 수량 검사 옵션 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">수량 검사 옵션</label>
                    <div className="space-y-2">
                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkMarketMaxQty"
                                checked={engineConfig.checkMarketMaxQty}
                                onChange={(e) => handleConfigChange('checkMarketMaxQty', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkMarketMaxQty" className="text-sm text-gray-200">
                                시장가 최대 수량 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkMarketMinQty"
                                checked={engineConfig.checkMarketMinQty}
                                onChange={(e) => handleConfigChange('checkMarketMinQty', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkMarketMinQty" className="text-sm text-gray-200">
                                시장가 최소 수량 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkLimitMaxQty"
                                checked={engineConfig.checkLimitMaxQty}
                                onChange={(e) => handleConfigChange('checkLimitMaxQty', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkLimitMaxQty" className="text-sm text-gray-200">
                                지정가 최대 수량 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkLimitMinQty"
                                checked={engineConfig.checkLimitMinQty}
                                onChange={(e) => handleConfigChange('checkLimitMinQty', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkLimitMinQty" className="text-sm text-gray-200">
                                지정가 최소 수량 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkMinNotionalValue"
                                checked={engineConfig.checkMinNotionalValue}
                                onChange={(e) => handleConfigChange('checkMinNotionalValue', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkMinNotionalValue" className="text-sm text-gray-200">
                                최소 명목 가치 검사
                            </label>
                        </div>
                    </div>
                </div>

                {/* 바 데이터 중복 검사 옵션 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">바 데이터 중복 검사 옵션</label>
                    <div className="space-y-2">
                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkSameBarDataWithTarget"
                                checked={engineConfig.checkSameBarDataWithTarget}
                                onChange={(e) => handleConfigChange('checkSameBarDataWithTarget', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkSameBarDataWithTarget" className="text-sm text-gray-200">
                                마크 가격 바 데이터와 목표 바 데이터 중복 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkSameBarDataTrading"
                                checked={engineConfig.checkSameBarDataTrading}
                                onChange={(e) => handleConfigChange('checkSameBarDataTrading', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkSameBarDataTrading" className="text-sm text-gray-200">
                                심볼 간 트레이딩 바 데이터 중복 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkSameBarDataMagnifier"
                                checked={engineConfig.checkSameBarDataMagnifier}
                                onChange={(e) => handleConfigChange('checkSameBarDataMagnifier', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkSameBarDataMagnifier" className="text-sm text-gray-200">
                                심볼 간 돋보기 바 데이터 중복 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkSameBarDataReference"
                                checked={engineConfig.checkSameBarDataReference}
                                onChange={(e) => handleConfigChange('checkSameBarDataReference', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkSameBarDataReference" className="text-sm text-gray-200">
                                심볼 간 참조 바 데이터 중복 검사
                            </label>
                        </div>

                        <div className="flex items-center space-x-2">
                            <input
                                type="checkbox"
                                id="checkSameBarDataMarkPrice"
                                checked={engineConfig.checkSameBarDataMarkPrice}
                                onChange={(e) => handleConfigChange('checkSameBarDataMarkPrice', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500 focus:ring-2"
                            />
                            <label htmlFor="checkSameBarDataMarkPrice" className="text-sm text-gray-200">
                                심볼 간 마크 가격 바 데이터 중복 검사
                            </label>
                        </div>
                    </div>
                </div>
            </div>

            {/* 프로젝트 디렉토리 설정 다이얼로그 */}
            {showProjectDialog && (
                <div className="fixed inset-0 z-50 flex items-center justify-center">
                    <div className="absolute inset-0 bg-black/60" onClick={() => setShowProjectDialog(false)}/>
                    <div
                        className="relative bg-[#1a1a1a] rounded-lg p-6 w-[520px] border border-gray-700 z-10 text-white">
                        <h3 className="text-lg font-semibold mb-4">프로젝트 디렉토리 설정</h3>
                        <div className="py-4">
                            <label className="text-sm text-gray-300 mb-2 block">
                                프로젝트 루트 경로 (절대 경로)
                            </label>
                            <Input
                                value={projectDirectoryInput}
                                onChange={(e) => setProjectDirectoryInput(e.target.value)}
                                placeholder="예: D:/Programming/Backtesting"
                                className="bg-[#0a0a0a] border-gray-600 text-white"
                            />
                            <p className="text-xs text-gray-400 mt-2">
                                이 경로는 데이터 파일 등을 찾을 때 기준 경로로 사용됩니다.
                            </p>
                        </div>
                        <div className="flex justify-end gap-2 mt-4">
                            <Button
                                variant="outline"
                                onClick={() => setShowProjectDialog(false)}
                                className="border-gray-600 text-gray-300 hover:bg-gray-800 hover:text-white"
                            >
                                취소
                            </Button>
                            <Button
                                onClick={handleSetProjectDirectory}
                                className="bg-blue-600 hover:bg-blue-700 text-white"
                            >
                                확인
                            </Button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
