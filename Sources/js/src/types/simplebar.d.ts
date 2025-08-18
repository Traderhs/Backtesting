declare module 'simplebar' {
  interface SimpleBarOptions {
    autoHide?: boolean;
    forceVisible?: boolean | 'x' | 'y';
    clickOnTrack?: boolean;
    classNames?: {
      content?: string;
      scrollContent?: string;
      scrollbar?: string;
      track?: string;
    };
    direction?: 'rtl' | 'ltr';
    scrollbarMinSize?: number;
    scrollbarMaxSize?: number;
    ariaLabel?: string;
    tabIndex?: number;
  }

  export default class SimpleBar {
    constructor(element: HTMLElement, options?: SimpleBarOptions);
    
    recalculate(): void;
    getScrollElement(): HTMLElement;
    getContentElement(): HTMLElement;
    unMount(): void;
    
    static removeObserver(): void;
    static instances: Map<HTMLElement, SimpleBar>;
  }
} 