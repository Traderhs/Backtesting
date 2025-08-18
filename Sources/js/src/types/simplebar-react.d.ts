declare module 'simplebar-react' {
  import React from 'react';
  
  interface SimpleBarProps {
    scrollableNodeProps?: React.HTMLAttributes<HTMLDivElement>;
    contentNodeProps?: React.HTMLAttributes<HTMLDivElement>;
    children?: React.ReactNode;
    className?: string;
    style?: React.CSSProperties;
    autoHide?: boolean;
    forceVisible?: boolean | 'x' | 'y';
    direction?: 'ltr' | 'rtl';
    timeout?: number;
  }
  
  const SimpleBar: React.FC<SimpleBarProps>;
  
  export default SimpleBar;
} 