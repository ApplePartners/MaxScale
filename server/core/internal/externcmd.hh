/*
 * Copyright (c) 2018 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2022-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */
#pragma once

#include <maxscale/ccdefs.hh>
#include <unistd.h>
#include <memory>

class ExternalCmd
{
public:
    /**
     * Create a new external command. The name and parameters are copied so
     * the original memory can be freed.
     *
     * @param argstr Command to execute with the parameters
     * @param timeout Command timeout in seconds
     * @return Pointer to new external command struct or NULL if an error occurred
     */
    static std::unique_ptr<ExternalCmd> create(const std::string& argstr, int timeout);

    /**
     * Execute a command
     *
     * The output of the command must be freed by the caller by calling MXS_FREE.
     *
     * @return The return value of the executed command or -1 on error
     */
    int externcmd_execute();

    /**
    * Substitute all occurrences of @c match with @c replace in the arguments.
    *
    * @param match Match string
    * @param replace Replacement string
    */
    void substitute_arg(const std::string& match, const std::string& replace);

    /**
     * Simple matching of string and command
     *
     * @param match String to search for
     *
     * @return True if the string matched
     */
    bool externcmd_matches(const char* match);

    void reset_substituted();

    const char* substituted() const;

private:
    static const int MAX_ARGS {256};

    std::string m_command;              /**< Original command */
    std::string m_command_substituted;  /**< Command with substitutions */
    int         m_timeout;              /**< Command timeout in seconds */

    ExternalCmd(const std::string& script, int timeout);

    int tokenize_args(char* dest[], int dest_size);
};

