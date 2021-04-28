/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-04-28
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include <maxscale/ccdefs.hh>
#include <maxscale/config2.hh>

class CommentConfig : public mxs::config::Configuration
{
public:
    CommentConfig(const CommentConfig&) = delete;
    CommentConfig& operator = (const CommentConfig&) = delete;

    CommentConfig(const char* zName);

    CommentConfig(CommentConfig&& rhs) = default;

    static void populate(MXS_MODULE& info);

    std::string inject;
};
