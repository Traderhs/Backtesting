import {cva, type VariantProps} from "class-variance-authority"
import {cn} from "@/Lib/Utils.ts"
import * as React from "react"

const buttonVariants = cva(
    "inline-flex items-center justify-center rounded-md text-sm font-medium ring-offset-background disabled:pointer-events-none disabled:opacity-50",
    {
        variants: {
            variant: {
                default: "bg-primary text-primary-foreground shadow-sm",
                destructive: "bg-destructive text-destructive-foreground shadow-sm",
                outline: "border border-input bg-background",
                secondary: "bg-secondary text-secondary-foreground shadow-sm",
                ghost: "",
                link: "text-primary underline-offset-4 decoration-2",
            },
            size: {
                default: "h-10 px-4 py-2",
                sm: "h-9 rounded-md px-3",
                lg: "h-11 rounded-md px-8",
                icon: "h-10 w-10",
            },
        },
        defaultVariants: {
            variant: "default",
            size: "default",
        },
    } as const  // 이걸 추가해서 구성 객체를 리터럴 타입으로 고정
)

export interface ButtonProps
    extends React.ButtonHTMLAttributes<HTMLButtonElement>,
        VariantProps<typeof buttonVariants> {
}

const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
    ({className, variant, size, ...props}, ref) => {
        return (
            <button
                className={cn(buttonVariants({variant, size, className}))}
                ref={ref}
                {...props}
            />
        )
    }
)

Button.displayName = "Button"

export {Button, buttonVariants}
