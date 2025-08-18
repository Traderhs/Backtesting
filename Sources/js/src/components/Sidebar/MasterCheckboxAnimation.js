/**
 * 마스터 체크박스 애니메이션 핸들러
 * indeterminate -> unchecked 변경 시 애니메이션 적용
 * 강제 인라인 스타일 적용으로 확실한 애니메이션 보장
 * 서브 체크박스와 마스터 체크박스 애니메이션 타이밍 동기화
 */
(function() {
    // 체크박스 상태 및 애니메이션 추적 데이터
    const checkboxStates = new Map();
    
    // 이전 indeterminate 상태 체크박스 추적
    const indeterminateCheckboxes = new Set();
    
    // 체크박스 그룹 관계 추적 (마스터 체크박스와 그에 속한 서브 체크박스들)
    const checkboxGroups = new Map();
    
    // DOM이 준비된 후 실행
    function initialize() {
        // 강력한 흔들림 애니메이션 적용 함수
        function applyShakeAnimation(checkbox, force = false) {
            // 이미 애니메이션이 적용 중인지 확인
            if (!force && checkbox.style.animation && checkbox.style.animation.includes('shake')) {
                return; // 이미 흔들림 애니메이션 중이면 중복 적용 방지
            }
            
            // 애니메이션 재설정을 위해 스타일 제거 후 강제 리플로우
            checkbox.style.animation = 'none';
            void checkbox.offsetWidth;
            
            // 모든 체크박스에 동일한 shakeCheckbox 애니메이션 적용
            checkbox.style.animation = 'shakeCheckbox 0.5s ease-in-out';
            
            // 애니메이션 종료 후 상태 초기화
            setTimeout(() => {
                // 애니메이션 완료 후 상태 초기화 - 중요
                checkbox._animationComplete = true;
            }, 400); // 0.4초 애니메이션에 맞춤
            
            // 물리적 피드백 (진동)
            if ('vibrate' in navigator) {
                navigator.vibrate(30);
            }
        }
        
        // 마스터-서브 체크박스 관계 초기화
        function initializeCheckboxGroups() {
            // 모든 체크박스 컨테이너 찾기
            const filterSections = document.querySelectorAll('.filter-section, .basic-filter-section');
            
            filterSections.forEach(section => {
                // 각 섹션의 마스터 체크박스 찾기
                const masterCheckbox = section.querySelector('.header-checkbox') || 
                                      section.querySelector('.custom-checkbox.header-checkbox');
                
                if (!masterCheckbox) return;
                
                // 섹션 내 서브 체크박스들 찾기
                const subCheckboxes = Array.from(
                    section.querySelectorAll('.custom-checkbox:not(.header-checkbox)')
                );
                
                // 마스터-서브 관계 설정
                if (subCheckboxes.length > 0) {
                    checkboxGroups.set(masterCheckbox, subCheckboxes);
                    
                    // 각 서브 체크박스가 속한 마스터 체크박스 기록
                    subCheckboxes.forEach(sub => {
                        // 서브 체크박스에 마스터 체크박스 참조 저장
                        sub._masterCheckbox = masterCheckbox;
                    });
                }
            });
        }
        
        // 특수한 이벤트 핸들러로 변경 전달
        function setupCheckboxChangeHandlers() {
            // 모든 체크박스 선택
            const allCheckboxes = document.querySelectorAll('.custom-checkbox');
            
            allCheckboxes.forEach(checkbox => {
                if (checkbox.hasAttribute('data-event-handler-initialized')) {
                    return; // 이미 초기화된 체크박스는 건너뛰기
                }
                
                // 체크박스 초기 상태 저장
                const initialState = checkbox.indeterminate ? 'indeterminate' : 
                                    checkbox.checked ? 'checked' : 'unchecked';
                checkboxStates.set(checkbox, initialState);
                
                if (initialState === 'indeterminate') {
                    indeterminateCheckboxes.add(checkbox);
                }
                
                // 현재 상태를 데이터 속성으로 저장 (CSS 선택자용)
                checkbox.setAttribute('data-prev-state', initialState);
                
                // 애니메이션 상태 초기화
                checkbox._animationComplete = true;
                checkbox._hadIndeterminateState = false;
                
                // click 이벤트 감지 - 서브 체크박스에서 변경 발생 시
                checkbox.addEventListener('click', function() {
                    // 클릭 이벤트 발생 시간 기록
                    checkbox._lastClickTime = Date.now();
                    
                    // 클릭 직후 현재 상태 (toggle 전)
                    const prevState = checkboxStates.get(checkbox) || 'unchecked';
                    
                    // 중립 상태였던 적이 있는지 기록
                    if (prevState === 'indeterminate') {
                        checkbox._hadIndeterminateState = true;
                    }
                    
                    // 다음 tick에서 변경된 상태 확인 및 처리
                    setTimeout(() => {
                        // 상태가 변경된 후의 실제 상태
                        const newState = checkbox.indeterminate ? 'indeterminate' : 
                                        checkbox.checked ? 'checked' : 'unchecked';
                        
                        // 실제 상태 변경이 있는 경우만 처리
                        if (prevState !== newState) {
                            // 체크박스 상태 업데이트
                            checkboxStates.set(checkbox, newState);
                            checkbox.setAttribute('data-prev-state', newState);
                            
                            // indeterminate 상태 추적 업데이트
                            if (prevState === 'indeterminate') {
                                indeterminateCheckboxes.delete(checkbox);
                            } else if (newState === 'indeterminate') {
                                indeterminateCheckboxes.add(checkbox);
                                checkbox._hadIndeterminateState = true;
                            }
                            
                            // 중립 -> 미체크 또는 체크 -> 중립 전환 감지
                            if ((prevState === 'indeterminate' && newState === 'unchecked') ||
                                (prevState === 'checked' && newState === 'indeterminate')) {
                                // 애니메이션 적용
                                checkbox._animationComplete = false;
                                applyShakeAnimation(checkbox);
                            }
                            
                            // 한 번이라도 중립 상태였던 체크박스가 미체크 상태로 변경될 때
                            if (checkbox._hadIndeterminateState && newState === 'unchecked' && 
                                checkbox.classList.contains('header-checkbox')) {
                                // 애니메이션 적용
                                checkbox._animationComplete = false;
                                applyShakeAnimation(checkbox, true);
                            }
                            
                            // 이 체크박스가 서브 체크박스이고 마스터 체크박스가 있는 경우
                            if (checkbox._masterCheckbox) {
                                // 마스터 체크박스 상태 변경 확인을 위한 약간의 지연
                                setTimeout(() => {
                                    const master = checkbox._masterCheckbox;
                                    const prevMasterState = checkboxStates.get(master) || 'unchecked';
                                    const newMasterState = master.indeterminate ? 'indeterminate' : 
                                                          master.checked ? 'checked' : 'unchecked';
                                    
                                    // 마스터 체크박스 상태 변경 감지
                                    if (prevMasterState !== newMasterState) {
                                        // 마스터 체크박스 상태 업데이트
                                        checkboxStates.set(master, newMasterState);
                                        master.setAttribute('data-prev-state', newMasterState);
                                        
                                        // 중립 상태였던 적이 있는지 기록
                                        if (newMasterState === 'indeterminate' || prevMasterState === 'indeterminate') {
                                            master._hadIndeterminateState = true;
                                        }
                                        
                                        // 중립 -> 미체크 또는 체크 -> 중립 전환 감지 (마스터 체크박스)
                                        if ((prevMasterState === 'indeterminate' && newMasterState === 'unchecked') ||
                                            (prevMasterState === 'checked' && newMasterState === 'indeterminate')) {
                                            // 마스터 체크박스에 즉시 애니메이션 적용 (서브 체크박스와 동기화)
                                            master._animationComplete = false;
                                            applyShakeAnimation(master, true);
                                        }
                                    }
                                }, 0);
                            }
                        }
                    }, 0);
                });
                
                // MutationObserver를 통한 체크박스 상태 변경 감지 (DOM 속성 변경)
                const observer = new MutationObserver((mutations) => {
                    mutations.forEach(mutation => {
                        if (mutation.type === 'attributes' && 
                            (mutation.attributeName === 'checked' || mutation.attributeName === 'class')) {
                            
                            // 변경 후 약간의 지연으로 상태 확인
                            setTimeout(() => {
                                const prevState = checkboxStates.get(checkbox) || 'unchecked';
                                const newState = checkbox.indeterminate ? 'indeterminate' : 
                                               checkbox.checked ? 'checked' : 'unchecked';
                                
                                // 실제 상태 변경이 있는 경우만 처리
                                if (prevState !== newState) {
                                    // 체크박스 상태 업데이트
                                    checkboxStates.set(checkbox, newState);
                                    checkbox.setAttribute('data-prev-state', newState);
                                    
                                    // 중립 상태였던 적이 있는지 기록
                                    if (newState === 'indeterminate' || prevState === 'indeterminate') {
                                        checkbox._hadIndeterminateState = true;
                                    }
                                    
                                    // 중립 -> 미체크 또는 체크 -> 중립 전환 감지
                                    if (((prevState === 'indeterminate' && newState === 'unchecked') ||
                                        (prevState === 'checked' && newState === 'indeterminate')) &&
                                        !checkbox._lastClickTime) { // 직접 클릭이 아닌 경우
                                        
                                        // 다른 체크박스 조작으로 인한 변경인 경우 애니메이션 적용
                                        checkbox._animationComplete = false;
                                        applyShakeAnimation(checkbox);
                                    }
                                }
                            }, 0);
                        }
                    });
                });
                
                observer.observe(checkbox, { 
                    attributes: true, 
                    attributeFilter: ['checked', 'class'] 
                });
                
                // 초기화 완료 표시
                checkbox.setAttribute('data-event-handler-initialized', 'true');
            });
        }
        
        // 폴링 방식 모니터링 (indeterminate 속성은 MutationObserver로 감지 불가)
        function startPollingMonitor() {
            // 체크박스 상태 주기적 확인
            // 50ms 간격으로 체크 (빠른 반응성 위해)
            return setInterval(() => {
                // 모든 체크박스 확인
                document.querySelectorAll('.custom-checkbox').forEach(checkbox => {
                    // 현재 체크박스 상태
                    const currentState = checkbox.indeterminate ? 'indeterminate' :
                        checkbox.checked ? 'checked' : 'unchecked';

                    // 이전에 저장된 상태
                    const prevState = checkboxStates.get(checkbox);

                    // 상태 변경 감지
                    if (prevState && prevState !== currentState) {
                        // 중립 상태였던 적이 있는지 기록
                        if (currentState === 'indeterminate' || prevState === 'indeterminate') {
                            checkbox._hadIndeterminateState = true;
                        }
                        
                        // 중립 -> 미체크 또는 체크 -> 중립 전환 시 특별 처리
                        if ((prevState === 'indeterminate' && currentState === 'unchecked') ||
                            (prevState === 'checked' && currentState === 'indeterminate')) {
                            // 직접 클릭 여부 확인 (최근 0.5초 이내 클릭)
                            const wasRecentlyClicked = checkbox._lastClickTime &&
                                (Date.now() - checkbox._lastClickTime < 500);

                            // 직접 클릭이 아닌 경우 (다른 체크박스로 인한 변경)
                            if (!wasRecentlyClicked) {
                                // 애니메이션 적용 (강제)
                                checkbox._animationComplete = false;
                                applyShakeAnimation(checkbox, true);
                            }
                        }
                        
                        // 한 번이라도 중립 상태였던 마스터 체크박스가 미체크 상태로 변경될 때
                        if (checkbox._hadIndeterminateState && 
                            currentState === 'unchecked' && 
                            checkbox.classList.contains('header-checkbox')) {
                            // 애니메이션 강제 적용
                            checkbox._animationComplete = false;
                            applyShakeAnimation(checkbox, true);
                        }

                        // 상태 업데이트
                        checkboxStates.set(checkbox, currentState);
                        checkbox.setAttribute('data-prev-state', currentState);
                    }
                });
            }, 50);
        }
        
        // 강화된 스타일 추가
        function addEnhancedStyles() {
            // 이미 스타일이 있는지 확인
            if (document.getElementById('checkbox-animation-styles')) {
                return;
            }
            
            const style = document.createElement('style');
            style.id = 'checkbox-animation-styles';
            style.textContent = `
                /* 체크박스 흔들림 애니메이션 정의 - 기존 애니메이션 유지 */
                @keyframes shakeCheckbox {
                    0% { transform: rotate(0deg); }
                    20% { transform: rotate(-45deg); }
                    40% { transform: rotate(45deg); }
                    60% { transform: rotate(-20deg); }
                    80% { transform: rotate(10deg); }
                    100% { transform: rotate(0deg); }
                }
                

                
                /* 마스터 체크박스 강조 스타일 */
                .custom-checkbox.header-checkbox:not(:checked)[data-prev-state="indeterminate"],
                .custom-checkbox.header-checkbox:indeterminate[data-prev-state="checked"],
                .custom-checkbox.header-checkbox:not(:checked)[data-animation-target="true"] {
                    animation: shakeCheckbox 0.5s ease-in-out;
                }
            `;
            document.head.appendChild(style);
        }
        
        // 초기화 메인 함수
        function initializeAll() {
            // 체크박스 스타일 강화
            addEnhancedStyles();
            
            // 체크박스 그룹 관계 초기화
            initializeCheckboxGroups();
            
            // 체크박스 이벤트 핸들러 설정
            setupCheckboxChangeHandlers();
            
            // 폴링 모니터링 시작
            const pollingInterval = startPollingMonitor();
            
            // DOM 변경 감지를 위한 MutationObserver
            const observer = new MutationObserver((mutations) => {
                let shouldReinitialize = false;
                
                mutations.forEach(mutation => {
                    if (mutation.type === 'childList' && mutation.addedNodes.length > 0) {
                        // 새로운 체크박스가 추가되었는지 확인
                        mutation.addedNodes.forEach(node => {
                            if (node.nodeType === 1) { // Element 노드
                                if (node.classList && node.classList.contains('custom-checkbox')) {
                                    shouldReinitialize = true;
                                } else if (node.querySelector && node.querySelector('.custom-checkbox')) {
                                    shouldReinitialize = true;
                                }
                            }
                        });
                    }
                });
                
                if (shouldReinitialize) {
                    // 체크박스 그룹 관계 다시 초기화
                    initializeCheckboxGroups();
                    
                    // 새로 추가된 체크박스에 이벤트 핸들러 설정
                    setupCheckboxChangeHandlers();
                }
            });
            
            // 전체 문서 변경 감지
            observer.observe(document.body, { 
                childList: true, 
                subtree: true
            });
            
            // 정리 함수 반환
            return function cleanup() {
                observer.disconnect();
                clearInterval(pollingInterval);
            };
        }
        
        // 초기화 실행
        return initializeAll();
    }
    
    // DOM 로드 확인 후 초기화
    let cleanup;
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            cleanup = initialize();
        });
    } else {
        cleanup = initialize();
    }
    
    // 페이지 언로드 시 정리
    window.addEventListener('unload', () => {
        if (cleanup) cleanup();
    });
})(); 