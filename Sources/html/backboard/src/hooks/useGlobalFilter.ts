import {useContext} from "react"
import {GlobalFilterContext} from "@/lib/GlobalFilterContext"

export const useGlobalFilter = () => useContext(GlobalFilterContext)
