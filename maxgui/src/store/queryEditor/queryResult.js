/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-04-08
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import queryHelper from './queryHelper'
/**
 * @returns Initial result related states
 */
export function resultStatesToBeSynced() {
    return {
        curr_query_mode: 'QUERY_VIEW',
        show_vis_sidebar: false,
    }
}
/**
 * Below states are stored in hash map structure.
 * Using worksheet's id as key. This helps to preserve
 * multiple worksheet's data in memory.
 * Use `queryHelper.memStatesMutationCreator` to create corresponding mutations
 * Some keys will have mutation name starts with either `SET` or `PATCH`
 * prefix. Check queryResultMemStateMutationTypeMap for more info
 * @returns {Object} - returns states that are stored in memory
 */
function memStates() {
    return {
        prvw_data_map: {},
        prvw_data_details_map: {},
        query_results_map: {},
        is_stopping_query_map: {},
    }
}
export function queryResultMemStateMutationTypeMap() {
    const keysWithPrefixSet = ['is_stopping_query_map']
    return Object.keys(memStates()).reduce((res, key) => {
        return { ...res, [key]: keysWithPrefixSet.includes(key) ? 'SET' : 'PATCH' }
    }, {})
}
export default {
    namespaced: true,
    state: {
        ...memStates(),
        ...resultStatesToBeSynced(),
    },
    mutations: {
        ...queryHelper.memStatesMutationCreator({
            mutationTypesMap: queryResultMemStateMutationTypeMap(),
        }),
        ...queryHelper.syncedStateMutationsCreator(resultStatesToBeSynced()),
        ...queryHelper.syncWkeToFlatStateMutationCreator({
            statesToBeSynced: resultStatesToBeSynced(),
            suffix: 'QUERY_RESULT',
        }),
    },
    actions: {
        /**
         * @param {String} tblId - Table id (database_name.table_name).
         */
        async fetchPrvw({ rootState, commit, dispatch }, { tblId, prvwMode }) {
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const active_wke_id = rootState.query.active_wke_id
            const request_sent_time = new Date().valueOf()
            try {
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_wke_id,
                    payload: {
                        request_sent_time,
                        total_duration: 0,
                        [`loading_${prvwMode.toLowerCase()}`]: true,
                    },
                })
                let sql, queryName
                const escapedTblId = this.vue.$help.escapeIdentifiers(tblId)
                switch (prvwMode) {
                    case rootState.app_config.SQL_QUERY_MODES.PRVW_DATA:
                        sql = `SELECT * FROM ${escapedTblId} LIMIT 1000;`
                        queryName = `Preview ${escapedTblId} data`
                        break
                    case rootState.app_config.SQL_QUERY_MODES.PRVW_DATA_DETAILS:
                        sql = `DESCRIBE ${escapedTblId};`
                        queryName = `View ${escapedTblId} details`
                        break
                }

                let res = await this.$queryHttp.post(`/sql/${active_sql_conn.id}/queries`, {
                    sql,
                    max_rows: rootState.persisted.query_max_rows,
                })
                const now = new Date().valueOf()
                const total_duration = ((now - request_sent_time) / 1000).toFixed(4)
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_wke_id,
                    payload: {
                        data: Object.freeze(res.data.data),
                        total_duration: parseFloat(total_duration),
                        [`loading_${prvwMode.toLowerCase()}`]: false,
                    },
                })
                dispatch(
                    'persisted/pushQueryLog',
                    {
                        startTime: now,
                        name: queryName,
                        sql,
                        res,
                        connection_name: active_sql_conn.name,
                        queryType: rootState.app_config.QUERY_LOG_TYPES.ACTION_LOGS,
                    },
                    { root: true }
                )
            } catch (e) {
                commit(`PATCH_${prvwMode}_MAP`, {
                    id: active_wke_id,
                    payload: {
                        [`loading_${prvwMode.toLowerCase()}`]: false,
                    },
                })
                this.vue.$logger(`store-queryResult-fetchPrvw`).error(e)
            }
        },
        /**
         * @param {String} query - SQL query string
         */
        async fetchQueryResult({ commit, dispatch, rootState }, query) {
            const active_wke_id = rootState.query.active_wke_id
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const request_sent_time = new Date().valueOf()
            try {
                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_wke_id,
                    payload: {
                        request_sent_time,
                        total_duration: 0,
                        loading_query_result: true,
                    },
                })

                /**
                 * dispatch openBgConn before running the user's query to prevent concurrent
                 * querying of the same connection.
                 * This "BACKGROUND" connection must be disconnected after finnish the user's query.
                 * i.e. dispatch disconnectBgConn
                 */
                await dispatch('queryConn/openBgConn', active_sql_conn, { root: true })

                let res = await this.$queryHttp.post(`/sql/${active_sql_conn.id}/queries`, {
                    sql: query,
                    max_rows: rootState.persisted.query_max_rows,
                })
                const now = new Date().valueOf()
                const total_duration = ((now - request_sent_time) / 1000).toFixed(4)

                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_wke_id,
                    payload: {
                        results: Object.freeze(res.data.data),
                        total_duration: parseFloat(total_duration),
                        loading_query_result: false,
                    },
                })

                const USE_REG = /(use|drop database)\s/i
                if (query.match(USE_REG))
                    await dispatch('schemaSidebar/updateActiveDb', {}, { root: true })
                dispatch(
                    'persisted/pushQueryLog',
                    {
                        startTime: now,
                        sql: query,
                        res,
                        connection_name: active_sql_conn.name,
                        queryType: rootState.app_config.QUERY_LOG_TYPES.USER_LOGS,
                    },
                    { root: true }
                )
            } catch (e) {
                commit('PATCH_QUERY_RESULTS_MAP', {
                    id: active_wke_id,
                    payload: { loading_query_result: false },
                })
                this.vue.$logger(`store-queryResult-fetchQueryResult`).error(e)
            }
        },
        async stopQuery({ commit, rootGetters, rootState }) {
            const active_sql_conn = rootState.queryConn.active_sql_conn
            const active_wke_id = rootState.query.active_wke_id
            try {
                commit('SET_IS_STOPPING_QUERY_MAP', { id: active_wke_id, payload: true })
                const {
                    data: { data: { attributes: { results = [] } = {} } = {} } = {},
                } = await this.$queryHttp.post(
                    `/sql/${rootGetters['queryConn/getBgConn'].id}/queries`,
                    {
                        sql: `KILL QUERY ${active_sql_conn.attributes.thread_id}`,
                    }
                )
                if (results.length && results[0].errno)
                    commit(
                        'SET_SNACK_BAR_MESSAGE',
                        {
                            text: [
                                'Failed to stop the query',
                                ...Object.keys(results[0]).map(key => `${key}: ${results[0][key]}`),
                            ],
                            type: 'error',
                        },
                        { root: true }
                    )
            } catch (e) {
                this.vue.$logger(`store-queryResult-stopQuery`).error(e)
            }
            commit('SET_IS_STOPPING_QUERY_MAP', { id: active_wke_id, payload: false })
        },
        /**
         * This action clears prvw_data and prvw_data_details to empty object.
         * Call this action when user selects option in the sidebar.
         * This ensure sub-tabs in Data Preview tab are generated with fresh data
         */
        clearDataPreview({ state, commit }) {
            commit(`PATCH_PRVW_DATA_MAP`, {
                id: state.active_wke_id,
                payload: {
                    loading_prvw_data: false,
                    data: {},
                    request_sent_time: 0,
                    total_duration: 0,
                },
            })
            commit(`PATCH_PRVW_DATA_DETAILS_MAP`, {
                id: state.active_wke_id,
                payload: {
                    loading_prvw_data_details: false,
                    data: {},
                    request_sent_time: 0,
                    total_duration: 0,
                },
            })
        },
    },
    getters: {
        getQueryResult: (state, getters, rootState) =>
            state.query_results_map[rootState.query.active_wke_id] || {},
        getLoadingQueryResult: (state, getters) => {
            const { loading_query_result = false } = getters.getQueryResult
            return loading_query_result
        },
        getIsStoppingQuery: (state, getters, rootState) =>
            state.is_stopping_query_map[rootState.query.active_wke_id] || false,
        getResults: (state, getters) => {
            const { results = {} } = getters.getQueryResult
            return results
        },
        getQueryRequestSentTime: (state, getters) => {
            const { request_sent_time = 0 } = getters.getQueryResult
            return request_sent_time
        },
        getQueryExeTime: (state, getters) => {
            if (getters.getLoadingQueryResult) return -1
            const { attributes } = getters.getResults
            if (attributes) return parseFloat(attributes.execution_time.toFixed(4))
            return 0
        },
        getQueryTotalDuration: (state, getters) => {
            const { total_duration = 0 } = getters.getQueryResult
            return total_duration
        },
        // preview data getters
        getPrvwData: (state, getters, rootState) => mode => {
            let map = state[`${mode.toLowerCase()}_map`]
            if (map) return map[rootState.query.active_wke_id] || {}
            return {}
        },
        getLoadingPrvw: (state, getters) => mode => {
            return getters.getPrvwData(mode)[`loading_${mode.toLowerCase()}`] || false
        },
        getPrvwDataRes: (state, getters) => mode => {
            const { data: { attributes: { results = [] } = {} } = {} } = getters.getPrvwData(mode)
            if (results.length) return results[0]
            return {}
        },
        getPrvwExeTime: (state, getters) => mode => {
            if (state[`loading_${mode.toLowerCase()}`]) return -1
            const { data: { attributes } = {} } = getters.getPrvwData(mode)
            if (attributes) return parseFloat(attributes.execution_time.toFixed(4))
            return 0
        },
        getPrvwSentTime: (state, getters) => mode => {
            const { request_sent_time = 0 } = getters.getPrvwData(mode)
            return request_sent_time
        },
        getPrvwTotalDuration: (state, getters) => mode => {
            const { total_duration = 0 } = getters.getPrvwData(mode)
            return total_duration
        },
    },
}