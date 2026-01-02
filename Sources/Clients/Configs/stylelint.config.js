module.exports = {
    plugins: ["stylelint-order"],
    extends: ["stylelint-config-standard"],
    ignoreFiles: [
        "**/*.d.ts",
        "**/node_modules/**"
    ],
    overrides: [
        {
            files: ["**/*.{js,jsx,ts,tsx}"],
            customSyntax: "postcss-styled-syntax"
        }
    ],
    rules: {
        "order/properties-order": [
            [
                // Positioning
                "position",
                "top",
                "right",
                "bottom",
                "left",
                "z-index",

                // Display & Box Model
                "display",
                "flex",
                "flex-direction",
                "flex-wrap",
                "flex-flow",
                "flex-grow",
                "flex-shrink",
                "flex-basis",
                "justify-content",
                "align-items",
                "align-content",
                "align-self",
                "order",
                "gap",
                "row-gap",
                "column-gap",
                "grid",
                "grid-template",
                "grid-template-rows",
                "grid-template-columns",
                "grid-template-areas",
                "grid-auto-rows",
                "grid-auto-columns",
                "grid-auto-flow",
                "grid-column",
                "grid-row",
                "grid-area",

                // Box Sizing
                "box-sizing",
                "width",
                "min-width",
                "max-width",
                "height",
                "min-height",
                "max-height",

                // Margin & Padding
                "margin",
                "margin-top",
                "margin-right",
                "margin-bottom",
                "margin-left",
                "padding",
                "padding-top",
                "padding-right",
                "padding-bottom",
                "padding-left",

                // Overflow
                "overflow",
                "overflow-x",
                "overflow-y",
                "overflow-wrap",

                // Border
                "border",
                "border-top",
                "border-right",
                "border-bottom",
                "border-left",
                "border-width",
                "border-style",
                "border-color",
                "border-radius",
                "border-top-left-radius",
                "border-top-right-radius",
                "border-bottom-right-radius",
                "border-bottom-left-radius",

                // Background
                "background",
                "background-color",
                "background-image",
                "background-position",
                "background-size",
                "background-repeat",
                "background-origin",
                "background-clip",
                "background-attachment",
                "background-blend-mode",

                // Shadow & Opacity
                "box-shadow",
                "opacity",

                // Typography
                "color",
                "font",
                "font-family",
                "font-size",
                "font-weight",
                "font-style",
                "font-variant",
                "line-height",
                "letter-spacing",
                "word-spacing",
                "text-align",
                "text-decoration",
                "text-transform",
                "text-indent",
                "text-overflow",
                "white-space",
                "word-break",
                "word-wrap",

                // List
                "list-style",
                "list-style-type",
                "list-style-position",
                "list-style-image",

                // Transform & Animation
                "transform",
                "transform-origin",
                "transition",
                "transition-property",
                "transition-duration",
                "transition-timing-function",
                "transition-delay",
                "animation",
                "animation-name",
                "animation-duration",
                "animation-timing-function",
                "animation-delay",
                "animation-iteration-count",
                "animation-direction",
                "animation-fill-mode",
                "animation-play-state",

                // Pointer & Visibility
                "cursor",
                "pointer-events",
                "visibility",
                "user-select",

                // Other
                "content",
                "clip",
                "clip-path",
                "filter",
                "backdrop-filter",
                "object-fit",
                "object-position"
            ],
            {
                unspecified: "bottomAlphabetical"
            }
        ],
        "at-rule-no-unknown": [
            true,
            {
                ignoreAtRules: [
                    "tailwind",
                    "apply",
                    "layer",
                    "variants",
                    "responsive",
                    "custom-variant",
                    "theme"
                ],
            },
        ],
    }
};
