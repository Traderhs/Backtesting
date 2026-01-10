import {Input} from '@/Components/UI/Input';
import {useStrategy} from './StrategyContext';

/**
 * 펀딩 비율 설정 섹션 컴포넌트
 * 펀딩 비율 폴더 경로를 설정
 */
export default function FundingRateSection() {
    const {fundingRatesDirectory, setFundingRatesDirectory} = useStrategy();

    return (
        <div className="mb-6 p-4 bg-[#071029] border border-gray-700 rounded-lg">
            <h2 className="text-lg font-semibold text-white mb-4">펀딩 비율 설정</h2>
            <div>
                <label className="text-xs text-gray-300 block mb-1">펀딩 비율 폴더</label>
                <Input
                    type="text"
                    value={fundingRatesDirectory}
                    onChange={(e) => setFundingRatesDirectory(e.target.value)}
                    placeholder="예: Data/Funding Rates"
                    className="bg-[#050a12] border-gray-600 w-full"
                />
            </div>
        </div>
    );
}
