import {useEffect} from 'react';

/**
 * 드롭다운 컨테이너에서 드롭다운이 열릴 때 선택된 항목으로 자동 스크롤
 */
export function useDropdownAutoScroll(
    containerRef: React.RefObject<HTMLElement | null>,
    isOpen: boolean,
    optionsSelector = '.strategy-editor-dropdown-options',
    selectedOptionSelector = '.strategy-editor-dropdown-option.selected'
) {
    useEffect(() => {
        if (!isOpen) {
            return;
        }

        const container = containerRef.current;
        if (!container) {
            return;
        }

        let cancelled = false;

        // 다음 프레임에 옵션 엘리먼트가 렌더링돼 있을 가능성에 대비
        const rafId = requestAnimationFrame(() => {
            if (cancelled) {
                return;
            }

            const options = container.querySelector(optionsSelector) as HTMLElement | null;
            if (!options) {
                return;
            }

            const selected = options.querySelector(selectedOptionSelector) as HTMLElement | null;

            // 선택된 항목이 없으면 리스트 상단으로 이동
            if (!selected) {
                options.scrollTop = 0;
                return;
            }

            const elTop = selected.offsetTop;
            const elBottom = elTop + selected.offsetHeight;
            const viewTop = options.scrollTop;
            const viewBottom = viewTop + options.clientHeight;

            if (elTop < viewTop) {
                options.scrollTop = elTop;
            } else if (elBottom > viewBottom) {
                options.scrollTop = elBottom - options.clientHeight;
            }
        });

        return () => {
            cancelled = true;
            cancelAnimationFrame(rafId);
        };
    }, [containerRef, isOpen, optionsSelector, selectedOptionSelector]);
}
