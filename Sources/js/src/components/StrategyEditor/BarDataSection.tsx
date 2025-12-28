import React, {useEffect, useState} from 'react';
import {Button} from '../ui/button';
import {BarDataConfig, BarDataType, TimeframeUnit} from '@/types/barData.ts';
import {Select, SelectContent, SelectItem, SelectTrigger, SelectValue} from '@/components/ui/select';
import {Input} from '@/components/ui/input';

interface BarDataSectionProps {
    barDataConfigs: BarDataConfig[];
    setBarDataConfigs: React.Dispatch<React.SetStateAction<BarDataConfig[]>>;
    useBarMagnifier: boolean;
    setUseBarMagnifier: React.Dispatch<React.SetStateAction<boolean>>;
    projectDirectory: string;
    configLoaded: boolean;
}

/**
 * 바 데이터 설정 섹션 컴포넌트
 * 트레이딩/돋보기/참조/마크 가격 바 데이터 설정 관리
 */
export default function BarDataSection({
                                           barDataConfigs,
                                           setBarDataConfigs,
                                           useBarMagnifier,
                                           setUseBarMagnifier,
                                           projectDirectory
                                       }: BarDataSectionProps) {
    // 바 데이터 타임프레임 Select의 열린 인덱스
    const [openTimeframeSelectIndex, setOpenTimeframeSelectIndex] = useState<number | null>(null);

    // 바 데이터 Select가 열려 있을 때 페이지 휠 스크롤이 막히는 문제를 우회하기 위한 캡처 단계 핸들러
    useEffect(() => {
        if (openTimeframeSelectIndex === null) {
            return;
        }

        const onWheelCapture = (e: WheelEvent) => {
            try {
                e.stopImmediatePropagation();
            } catch (err) {
                // 무시
            }
        };

        document.addEventListener('wheel', onWheelCapture as EventListener, {capture: true, passive: true});

        return () => {
            document.removeEventListener('wheel', onWheelCapture as EventListener, {
                capture: true,
                passive: true
            } as any);
        };
    }, [openTimeframeSelectIndex]);

    // 참조 바 데이터 추가
    const handleAddReferenceBar = () => {
        const safeProjectDir = projectDirectory ? projectDirectory.replace(/\\/g, '/') : '';
        const defaultKlinesDir = safeProjectDir ? `${safeProjectDir}/Data/Continuous Klines` : '';

        const newConfig: BarDataConfig = {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: defaultKlinesDir,
            barDataType: BarDataType.REFERENCE
        };

        const markPriceIndex = barDataConfigs.findIndex(c => c.barDataType === BarDataType.MARK_PRICE);
        const newConfigs = [...barDataConfigs];
        const insertIndex = markPriceIndex === -1 ? newConfigs.length : markPriceIndex;

        newConfigs.splice(insertIndex, 0, newConfig);
        setBarDataConfigs(newConfigs);
    };

    // 참조 바 데이터 삭제
    const handleRemoveReferenceBar = (index: number) => {
        if (barDataConfigs[index] && barDataConfigs[index].barDataType === BarDataType.REFERENCE) {
            const newConfigs = barDataConfigs.filter((_, i) => i !== index);

            setBarDataConfigs(newConfigs);
        }
    };

    // 바 데이터 설정 업데이트
    const updateBarDataConfig = (index: number, updates: Partial<BarDataConfig>) => {
        const newConfigs = [...barDataConfigs];
        newConfigs[index] = {...newConfigs[index], ...updates};

        setBarDataConfigs(newConfigs);
    };

    return (
        <div className="bg-[#1a1a1a] rounded-lg border border-gray-700 p-4 mb-4">
            <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-semibold text-white">바 데이터 설정</h2>
                <Button
                    onClick={handleAddReferenceBar}
                    className="bg-green-600 hover:bg-green-700 text-white px-3 py-1.5 text-sm rounded-lg flex items-center gap-1"
                >
                    <span className="text-lg leading-none">+</span>
                    <span>참조 바 추가</span>
                </Button>
            </div>

            <div className="grid grid-cols-2 gap-4">
                {barDataConfigs.map((config, index) => {
                    const isDisabled = config.barDataType === BarDataType.MAGNIFIER && !useBarMagnifier;
                    const canDelete = config.barDataType === BarDataType.REFERENCE;

                    const barDataTypeLabel = {
                        [BarDataType.TRADING]: '트레이딩 바 데이터',
                        [BarDataType.MAGNIFIER]: '돋보기 바 데이터',
                        [BarDataType.REFERENCE]: '참조 바 데이터',
                        [BarDataType.MARK_PRICE]: '마크 가격 바 데이터'
                    }[config.barDataType];

                    return (
                        <div
                            key={index}
                            className={`bg-[#252525] rounded-lg p-4 border border-gray-600 relative ${
                                isDisabled ? 'opacity-50' : ''
                            }`}
                        >
                            <div className="flex items-center justify-between mb-3">
                                <h3 className="text-sm font-semibold text-white">{barDataTypeLabel}</h3>
                                <div className="flex items-center gap-2">
                                    {config.barDataType === BarDataType.MAGNIFIER && (
                                        <label className="flex items-center gap-2 text-xs text-gray-300">
                                            <input
                                                type="checkbox"
                                                checked={useBarMagnifier}
                                                onChange={(e) => {
                                                    setUseBarMagnifier(e.target.checked);
                                                }}
                                                className="w-3.5 h-3.5"
                                            />
                                            바 돋보기 기능
                                        </label>
                                    )}
                                    {canDelete && (
                                        <button
                                            onClick={() => handleRemoveReferenceBar(index)}
                                            className="text-red-500 hover:text-red-400 text-xl leading-none px-1"
                                            title="참조 바 삭제"
                                        >
                                            ×
                                        </button>
                                    )}
                                </div>
                            </div>

                            <div className="space-y-3">
                                {/* 타임프레임 */}
                                <div>
                                    <label className="block text-xs text-gray-400 mb-1">타임프레임</label>
                                    <div className="grid grid-cols-2 gap-2">
                                        <Input
                                            type="text"
                                            inputMode="numeric"
                                            pattern="[0-9]*"
                                            value={config.timeframe.value ?? ''}
                                            onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                                if (e.ctrlKey || e.metaKey) {
                                                    return;
                                                }

                                                const allowed = /^(?:[0-9]|Backspace|Delete|ArrowLeft|ArrowRight|Tab|Home|End)$/;
                                                if (!allowed.test(e.key)) {
                                                    e.preventDefault();
                                                }
                                            }}
                                            onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                                                const raw = e.target.value || '';
                                                const digits = raw.replace(/\D/g, '');
                                                const noLeading = digits.replace(/^0+/, '');

                                                const finalValue = noLeading === '' ? null : parseInt(noLeading, 10);

                                                updateBarDataConfig(index, {
                                                    timeframe: {
                                                        ...config.timeframe,
                                                        value: finalValue
                                                    }
                                                });
                                            }}
                                            disabled={isDisabled}
                                            className="bg-[#1a1a1a] border-gray-600 text-sm"
                                        />
                                        <Select
                                            value={config.timeframe.unit}
                                            onValueChange={(value: TimeframeUnit) => updateBarDataConfig(index, {
                                                timeframe: {...config.timeframe, unit: value as TimeframeUnit}
                                            })}
                                            disabled={isDisabled}
                                            open={openTimeframeSelectIndex === index}
                                            onOpenChange={(isOpen: boolean) => {
                                                if (isOpen) {
                                                    setOpenTimeframeSelectIndex(index);
                                                } else {
                                                    setOpenTimeframeSelectIndex(prev => prev === index ? null : prev);
                                                }
                                            }}
                                        >
                                            <SelectTrigger
                                                className="w-full bg-[#1a1a1a] border-gray-600 text-sm">
                                                <SelectValue/>
                                            </SelectTrigger>
                                            <SelectContent
                                                position="popper"
                                                side="bottom"
                                                sideOffset={0}
                                                align="start"
                                                avoidCollisions={false}
                                                sticky="always"
                                                hideWhenDetached={false}
                                                onCloseAutoFocus={(e: Event) => {
                                                    try {
                                                        e.preventDefault();
                                                    } catch (err) {
                                                    }
                                                }}
                                            >
                                                <SelectItem value={TimeframeUnit.SECOND}>초</SelectItem>
                                                <SelectItem value={TimeframeUnit.MINUTE}>분</SelectItem>
                                                <SelectItem value={TimeframeUnit.HOUR}>시간</SelectItem>
                                                <SelectItem value={TimeframeUnit.DAY}>일</SelectItem>
                                                <SelectItem value={TimeframeUnit.WEEK}>주</SelectItem>
                                                <SelectItem value={TimeframeUnit.MONTH}>개월</SelectItem>
                                                <SelectItem value={TimeframeUnit.YEAR}>년</SelectItem>
                                            </SelectContent>
                                        </Select>
                                    </div>
                                </div>

                                {/* 폴더 경로 */}
                                <div>
                                    <label className="block text-xs text-gray-400 mb-1">폴더 경로</label>
                                    <Input
                                        type="text"
                                        value={config.klinesDirectory}
                                        onChange={(e: React.ChangeEvent<HTMLInputElement>) => updateBarDataConfig(index, {klinesDirectory: e.target.value})}
                                        placeholder="바 데이터 폴더 경로 입력"
                                        disabled={isDisabled}
                                        className="w-full bg-[#1a1a1a] border-gray-600 text-xs"
                                    />
                                </div>
                            </div>
                        </div>
                    );
                })}
            </div>
        </div>
    );
}
