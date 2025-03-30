import {useState, useEffect} from "react";

interface ServerAlertProps {
    serverError: boolean;
    message?: string;
}

export default function ServerAlert({serverError, message}: ServerAlertProps) {
    const [visible, setVisible] = useState(serverError);

    useEffect(() => {
        setVisible(serverError);
    }, [serverError]);

    if (!visible) return null;

    return (
        <div className="fixed top-0 left-0 right-0 p-4 bg-red-600 text-white z-50 flex items-center justify-center">
            <span>{message ? message + "\n서버가 종료되었습니다." : "서버가 종료되었습니다."}</span>
        </div>
    );
}
