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
import queryHelper from '@queryEditorSrc/store/queryHelper'
import { lodash } from '@share/utils/helpers'
import { Model } from '@vuex-orm/core'

/**
 * The class inherits this extender must have getNonKeyFields static method
 */
export default class extends Model {
    /**
     * The static method getNonKeyFields returns value as an object which is
     * needed by vuex-orm. This function replaces the object value with the actual value
     * @public
     * @param {Object} getNonKeyFields - return of the class entity static method getNonKeyFields
     * @returns {Object} - return object fields with the actual value
     */
    static getNonKeyFieldsValue(keyFields) {
        return Object.keys(keyFields).reduce((obj, key) => {
            obj[key] = keyFields[key].value
            return obj
        }, {})
    }
    /**
     * This function refreshes value of non-key and non-relational fields of the provided entities.
     * Entities to be updated is provided as an argument in the payload parameter.
     * "this" refers to the Model entity
     * @param {String|Function} payload - either an id or a callback function that return Boolean (filter)
     * @param {Array} ignoreKeys - keys to be ignored
     */
    static refresh(payload, ignoreKeys = []) {
        const models = queryHelper.filterEntity(this, payload)
        models.forEach(model => {
            const target = this.query()
                .withAll()
                .whereId(model.id)
                .first()
            if (target) {
                const defData = this.getNonKeyFieldsValue(this.getNonKeyFields())
                this.update({
                    where: model.id,
                    data: lodash.pickBy(defData, (v, key) => !ignoreKeys.includes(key)),
                })
            }
        })
    }
}