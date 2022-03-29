/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-03-29
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
// Required for Vuetify (Create div with a data-app attribute)
const app = document.createElement('div')
app.setAttribute('data-app', 'true')
document.body.appendChild(app)

global.requestAnimationFrame = () => null
global.cancelAnimationFrame = () => null

const localStorageMock = (() => {
    let store = {}

    return {
        getItem(key) {
            return store[key] || null
        },
        setItem(key, value) {
            store[key] = value.toString()
        },
        clear() {
            store = {}
        },
    }
})()

// global define

global.localStorage = localStorageMock
// this prevents console from being printed out
console.error = () => {}
console.info = () => {}
console.warn = () => {}

window.HTMLElement.prototype.scrollIntoView = () => {}
