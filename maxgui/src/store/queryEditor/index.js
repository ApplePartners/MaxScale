import query, { defWorksheetState } from './query'
import queryConn from './queryConn'
import editor from './editor'
import schemaSidebar from './schemaSidebar'
import queryResult from './queryResult'

export function getDefWorksheetState() {
    return defWorksheetState()
}
export default { query, queryConn, editor, schemaSidebar, queryResult }