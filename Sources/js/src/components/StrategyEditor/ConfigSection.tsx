import {useState} from 'react';
import {Button} from '../ui/button';
import {Input} from '@/components/ui/input';
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
