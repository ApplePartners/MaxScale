/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-12-27
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
import Worksheet from '@wsModels/Worksheet'
import QueryConn from '@wsModels/QueryConn'
import { insertQueryEditor } from '@wsSrc/store/orm/initEntities'

export default {
    namespaced: true,
    state: {
        clusters: {}, // key is the name of the monitor, value is the monitor cluster
        current_cluster: {},
        config_graph_data: [],
    },
    mutations: {
        SET_CLUSTERS(state, payload) {
            state.clusters = payload
        },
        SET_CURR_CLUSTER(state, payload) {
            state.current_cluster = payload
        },
        SET_CONFIG_GRAPH_DATA(state, payload) {
            state.config_graph_data = payload
        },
    },
    actions: {
        async discoveryClusters({ commit, dispatch, rootState, getters }) {
            try {
                await Promise.all([
                    dispatch('server/fetchAllServers', {}, { root: true }),
                    dispatch('monitor/fetchAllMonitors', {}, { root: true }),
                ])
                let clusters = {}
                rootState.monitor.all_monitors.forEach(monitor => {
                    //TODO: Handle other monitors, now it only handles mariadbmon
                    if (monitor.attributes.module === 'mariadbmon') {
                        let cluster = getters.getMariadbmonCluster(monitor)
                        clusters[monitor.id] = cluster
                    }
                })
                commit('SET_CLUSTERS', clusters)
            } catch (e) {
                this.vue.$logger.error(e)
            }
        },
        async fetchClusterById({ commit, dispatch, rootState, getters }, id) {
            try {
                await Promise.all([
                    dispatch('server/fetchAllServers', {}, { root: true }),
                    dispatch('monitor/fetchMonitorById', id, { root: true }),
                ])
                let cluster = {}
                const monitor = rootState.monitor.current_monitor
                //TODO: Handle other monitors, now it only handles mariadbmon
                if (monitor.attributes.module === 'mariadbmon')
                    cluster = getters.getMariadbmonCluster(monitor)
                commit('SET_CURR_CLUSTER', cluster)
            } catch (e) {
                this.vue.$logger.error(e)
            }
        },
        async fetchConfigData({ commit, dispatch, getters }) {
            try {
                await Promise.all([
                    dispatch('monitor/fetchAllMonitors', {}, { root: true }),
                    dispatch('server/fetchAllServers', {}, { root: true }),
                    dispatch('service/fetchAllServices', {}, { root: true }),
                    dispatch('filter/fetchAllFilters', {}, { root: true }),
                    dispatch('listener/fetchAllListeners', {}, { root: true }),
                ])
                commit('SET_CONFIG_GRAPH_DATA', getters.getConfigGraphData)
            } catch (e) {
                this.vue.$logger.error(e)
            }
        },
        /**
         * Check if there is a worksheet connected to the provided conn_name make it the current
         * active worksheet if it's not. Otherwise, find an empty worksheet(has not been bound to a connection),
         * set it as active and dispatch SET_PRE_SELECT_CONN_RSRC to open connection dialog
         * @param {String} param.conn_name - connection name
         */
        async chooseActiveWorkspaceWke({ commit }, { type, conn_name }) {
            const wkeConns = QueryConn.getters('getWkeConns')
            // Find connection
            const wkeConn = wkeConns.find(c => c.name === conn_name)
            // If it is already bound to a worksheet, set that worksheet as active
            if (this.vue.$typy(wkeConn, 'worksheet_id').safeString)
                Worksheet.commit(state => (state.active_wke_id = wkeConn.worksheet_id))
            else {
                const unavailableWkeIds = wkeConns.map(c => c.worksheet_id)
                const blankQueryEditorWke = Worksheet.query()
                    .where(
                        w =>
                            !unavailableWkeIds.includes(w.id) &&
                            !this.vue.$typy(w, 'active_query_tab_id').isNull
                    )
                    .first()
                // Use a blank query editor wke if there is one, otherwise create a new one
                if (blankQueryEditorWke)
                    Worksheet.commit(state => (state.active_wke_id = blankQueryEditorWke.id))
                else insertQueryEditor()
                // call onChangeWkeConn to handle connection binding, otherwise popup connection dialog
                if (wkeConn) await QueryConn.dispatch('onChangeWkeConn', wkeConn)
                else
                    commit(
                        'queryConnsMem/SET_PRE_SELECT_CONN_RSRC',
                        { type, id: conn_name },
                        { root: true }
                    )
            }
        },
    },
    getters: {
        getServerData: (state, getters, rootState, rootGetters) => {
            return id => {
                return rootGetters['server/getAllServersMap'].get(id)
            }
        },
        genSlaveNode: (state, getters) => {
            /**
             *
             * @param {object} param.server - server object in monitor_diagnostics.server_info
             * @param {String} param.masterName - master server name
             * @param {Array} param.connectionsToMaster - slave_connections to master
             * @returns
             */
            return ({ server, masterName, connectionsToMaster = [] }) => ({
                id: server.name,
                name: server.name,
                serverData: getters.getServerData(server.name),
                isMaster: false,
                masterServerName: masterName,
                server_info: {
                    ...server,
                    slave_connections: connectionsToMaster,
                },
                linkColor: '#0e9bc0',
            })
        },
        getMariadbmonCluster: (state, getters) => {
            return monitor => {
                const {
                    id: monitorId,
                    attributes: {
                        monitor_diagnostics: { master: masterName, server_info } = {},
                        state,
                        module,
                    },
                } = monitor
                // root node contain monitor data
                let root = {
                    id: monitorId,
                    name: monitorId,
                    state,
                    module,
                    linkColor: '#0e9bc0',
                    children: [], // contains a master server data
                    monitorData: monitor.attributes,
                }
                if (masterName)
                    root.children.push({
                        id: masterName,
                        name: masterName,
                        serverData: getters.getServerData(masterName),
                        isMaster: true,
                        linkColor: '#0e9bc0',
                        children: [], // contains replicate servers data
                    })
                if (root.children.length)
                    server_info.forEach(server => {
                        const connectionsToMaster = server.slave_connections.filter(
                            conn => conn.master_server_name === masterName
                        )
                        if (connectionsToMaster.length)
                            root.children[0].children.push(
                                getters.genSlaveNode({ server, masterName, connectionsToMaster })
                            )
                    })
                return root
            }
        },
        /*
         * This generates data for d3-dag StratifyOperator
         */
        getConfigGraphData: (state, getters, rootState) => {
            const {
                service: { all_services },
                server: { all_servers },
                monitor: { all_monitors },
                listener: { all_listeners },
            } = rootState
            let data = []
            const { SERVICES, SERVERS, LISTENERS } = rootState.app_config.RELATIONSHIP_TYPES
            const rsrcData = [all_services, all_servers, all_listeners, all_monitors]
            rsrcData.forEach(rsrc =>
                rsrc.forEach(item => {
                    const { id, type, relationships } = item
                    let node = { id, type, nodeData: item, parentIds: [] }
                    /**
                     * DAG graph requires root nodes.
                     * With current data from API, accurate links between nodes can only be found by
                     * checking the relationships data of a service. So monitors are root nodes here.
                     * This adds parent node ids for services, servers and listeners node to create links except
                     * monitors, as the links between monitors and servers or monitors and services are created
                     * already. This is an intention to prevent circular reference.
                     */
                    let relationshipTypes = []
                    switch (type) {
                        case SERVICES:
                            // a service can also target services or monitors
                            relationshipTypes = ['servers', 'services', 'monitors']
                            break
                        case SERVERS:
                            relationshipTypes = ['monitors']
                            break
                        case LISTENERS:
                            relationshipTypes = ['services']
                            break
                    }
                    Object.keys(relationships).forEach(key => {
                        if (relationshipTypes.includes(key))
                            relationships[key].data.forEach(n => {
                                node.parentIds.push(n.id) // create links
                            })
                    })
                    data.push(node)
                })
            )
            return data
        },
    },
}
