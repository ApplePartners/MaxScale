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
import Extender from '@wsSrc/store/orm/Extender'
import { ORM_PERSISTENT_ENTITIES, ETL_STATUS, ETL_STAGE_INDEX } from '@wsSrc/store/config'
import { uuidv1 } from '@share/utils/helpers'

export default class EtlTask extends Extender {
    static entity = ORM_PERSISTENT_ENTITIES.ETL_TASKS

    /**
     * @returns {Object} - return fields that are not key, relational fields
     */
    static getNonKeyFields() {
        return {
            name: this.string(''),
            status: this.string(ETL_STATUS.INITIALIZING),
            active_stage_index: this.number(ETL_STAGE_INDEX.OVERVIEW),
            /**
             * @property {string} src_type  - mariadb||postgresql||generic
             * @property {string} dest_name - server name in MaxScale
             * @property {string} async_query_id - query_id of async query
             * @property {boolean} is_loading - is loading etl prepare or  start result
             */
            meta: this.attr({}),
            /**
             * @property {number} timestamp
             * @property {string} name
             */
            logs: this.attr({
                [ETL_STAGE_INDEX.CONN]: [],
                [ETL_STAGE_INDEX.SRC_OBJ]: [],
                [ETL_STAGE_INDEX.MIGR_SCRIPT]: [],
                [ETL_STAGE_INDEX.DATA_MIGR]: [],
            }),
            created: this.number(Date.now()),
        }
    }

    static fields() {
        return {
            id: this.uid(() => uuidv1()),
            ...this.getNonKeyFields(),
            connections: this.hasMany(ORM_PERSISTENT_ENTITIES.QUERY_CONNS, 'etl_task_id'),
        }
    }
}