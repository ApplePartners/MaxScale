/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-11-16
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
export default {
    namespaced: true,
    state: {
        selected_query_txt: '',
        file_dlg_data: {
            is_opened: false,
            title: '',
            confirm_msg: '',
            on_save: () => null,
            dont_save: () => null,
        },
        // states for DDL_EDITOR
        charset_collation_map: {},
        def_db_charset_map: {},
        engines: [],
    },
    mutations: {
        SET_SELECTED_QUERY_TXT(state, payload) {
            state.selected_query_txt = payload
        },
        SET_FILE_DLG_DATA(state, payload) {
            state.file_dlg_data = payload
        },
        SET_CHARSET_COLLATION_MAP(state, payload) {
            state.charset_collation_map = payload
        },
        SET_DEF_DB_CHARSET_MAP(state, payload) {
            state.def_db_charset_map = payload
        },
        SET_ENGINES(state, payload) {
            state.engines = payload
        },
    },
    actions: {
        async queryCharsetCollationMap({ commit, rootGetters }) {
            const { id: connId } = rootGetters['queryConns/getActiveQueryTabConn']
            const [e, res] = await this.vue.$helpers.asyncTryCatch(
                this.vue.$queryHttp.post(`/sql/${connId}/queries`, {
                    sql:
                        // eslint-disable-next-line vue/max-len
                        'SELECT character_set_name, collation_name, is_default FROM information_schema.collations',
                })
            )
            if (!e) {
                let charsetCollationMap = {}
                const data = this.vue.$typy(res, 'data.data.attributes.results[0].data').safeArray
                data.forEach(row => {
                    const charset = row[0]
                    const collation = row[1]
                    const isDefCollation = row[2] === 'Yes'
                    let charsetObj = charsetCollationMap[`${charset}`] || {
                        collations: [],
                    }
                    if (isDefCollation) charsetObj.defCollation = collation
                    charsetObj.collations.push(collation)
                    charsetCollationMap[charset] = charsetObj
                })
                commit('SET_CHARSET_COLLATION_MAP', charsetCollationMap)
            }
        },
        async queryDefDbCharsetMap({ rootGetters, commit }) {
            const { id: connId } = rootGetters['queryConns/getActiveQueryTabConn']
            const [e, res] = await this.vue.$helpers.asyncTryCatch(
                this.vue.$queryHttp.post(`/sql/${connId}/queries`, {
                    sql:
                        // eslint-disable-next-line vue/max-len
                        'SELECT schema_name, default_character_set_name FROM information_schema.schemata',
                })
            )
            if (!e) {
                let defDbCharsetMap = {}
                const data = this.vue.$typy(res, 'data.data.attributes.results[0].data').safeArray
                data.forEach(row => {
                    const schema_name = row[0]
                    const default_character_set_name = row[1]
                    defDbCharsetMap[schema_name] = default_character_set_name
                })
                commit('SET_DEF_DB_CHARSET_MAP', defDbCharsetMap)
            }
        },
        async queryEngines({ rootGetters, commit }) {
            const { id: connId } = rootGetters['queryConns/getActiveQueryTabConn']
            const [e, res] = await this.vue.$helpers.asyncTryCatch(
                this.vue.$queryHttp.post(`/sql/${connId}/queries`, {
                    sql: 'SELECT engine FROM information_schema.ENGINES',
                })
            )
            if (!e) commit('SET_ENGINES', res.data.data.attributes.results[0].data.flat())
        },
        async queryAlterTblSuppData({ state, dispatch }) {
            if (this.vue.$typy(state.engines).isEmptyArray) await dispatch('queryEngines')
            if (this.vue.$typy(state.charset_collation_map).isEmptyObject)
                await dispatch('queryCharsetCollationMap')
            if (this.vue.$typy(state.def_db_charset_map).isEmptyObject)
                await dispatch('queryDefDbCharsetMap')
        },
    },
}
