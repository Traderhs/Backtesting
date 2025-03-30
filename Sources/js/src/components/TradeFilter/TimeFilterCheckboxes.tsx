import React from "react";

interface TimeFilterCheckboxesProps {
    label: string;
    options: number[];
    selectedValues?: number[];
    onChange: (option: number, checked: boolean) => void;
}

const TimeFilterCheckboxes: React.FC<TimeFilterCheckboxesProps> = ({
                                                                       options,
                                                                       selectedValues = [],
                                                                       onChange,
                                                                   }) => {
    return (
        <div className="mb-4">
            <div className="flex flex-wrap">
                {options.map((option) => (
                    <label key={option} className="mr-2">
                        <input
                            type="checkbox"
                            checked={selectedValues.includes(option)}
                            onChange={(e) => onChange(option, e.target.checked)}
                        />
                        <span className="ml-1">{option}</span>
                    </label>
                ))}
            </div>
        </div>
    );
};

export default TimeFilterCheckboxes;
