import React, { useEffect, useState } from "react";
import { useTradeFilter } from "../TradeFilter";

const HoldingTimeFilter: React.FC = () => {
    const { filter, setFilter } = useTradeFilter();
    // Context에 저장된 값을 기본값으로 사용 (있으면 유지됨)
    const [minValue, setMinValue] = useState<number | undefined>(
        filter.holdingTimeMin !== undefined ? filter.holdingTimeMin / 86400 : undefined
    );
    const [minUnit, setMinUnit] = useState<string>("일");
    const [maxValue, setMaxValue] = useState<number | undefined>(
        filter.holdingTimeMax !== undefined ? filter.holdingTimeMax / 86400 : undefined
    );
    const [maxUnit, setMaxUnit] = useState<string>("일");

    // 단위를 기준으로 입력된 값을 초 단위로 변환하는 함수
    const convertToSeconds = (value: number | undefined, unit: string): number | undefined => {
        if (value === undefined) return undefined;
        switch (unit) {
            case "연": return value * 31536000;
            case "월": return value * 2592000;
            case "일": return value * 86400;
            case "시간": return value * 3600;
            case "분": return value * 60;
            case "초": return value;
            default: return value;
        }
    };

    // 값 변경 시 Context에 바로 반영 (컴포넌트가 unmount 되어도 Context에 값은 유지됨)
    useEffect(() => {
        const minSeconds = minValue === 0 ? 0 : convertToSeconds(minValue, minUnit);
        const maxSeconds = maxValue === 0 ? 0 : convertToSeconds(maxValue, maxUnit);

        setFilter((prev) => ({
            ...prev,
            holdingTimeMin: minSeconds,
            holdingTimeMax: maxSeconds,
        }));
    }, [minValue, minUnit, maxValue, maxUnit, setFilter]);

    return (
        <div className="space-y-2">
            <div className="font-semibold">보유 시간</div>
            <div className="flex flex-col space-y-2">
                {/* 최소 (이상) */}
                <div>
                    <div className="flex items-center space-x-1">
                        <input
                            type="number"
                            value={minValue !== undefined ? minValue : ""}
                            onChange={(e) =>
                                setMinValue(e.target.value ? Number(e.target.value) : undefined)
                            }
                            placeholder="최소"
                            className="border rounded p-1 w-16"
                        />
                        <select
                            value={minUnit}
                            onChange={(e) => setMinUnit(e.target.value)}
                            className="border rounded p-1 text-sm w-16 text-white bg-transparent"
                        >
                            <option className="text-black">년</option>
                            <option className="text-black">달</option>
                            <option className="text-black">일</option>
                            <option className="text-black">시간</option>
                            <option className="text-black">분</option>
                            <option className="text-black">초</option>
                        </select>
                    </div>
                    <label className="block text-sm">이상</label>
                </div>
                {/* 최대 (이하) */}
                <div>
                    <div className="flex items-center space-x-1">
                        <input
                            type="number"
                            value={maxValue !== undefined ? maxValue : ""}
                            onChange={(e) =>
                                setMaxValue(e.target.value ? Number(e.target.value) : undefined)
                            }
                            placeholder="최대"
                            className="border rounded p-1 w-16"
                        />
                        <select
                            value={maxUnit}
                            onChange={(e) => setMaxUnit(e.target.value)}
                            className="border rounded p-1 text-sm w-16 text-white bg-transparent"
                        >
                            <option className="text-black">년</option>
                            <option className="text-black">달</option>
                            <option className="text-black">일</option>
                            <option className="text-black">시간</option>
                            <option className="text-black">분</option>
                            <option className="text-black">초</option>
                        </select>
                    </div>
                    <label className="block text-sm">이하</label>
                </div>
            </div>
        </div>
    );
};

export default HoldingTimeFilter;
