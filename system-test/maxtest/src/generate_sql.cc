/*
 * Copyright (c) 2023 MariaDB Corporation Ab
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

#include <maxtest/generate_sql.hh>
#include <maxbase/string.hh>

#include <tuple>
#include <array>

namespace
{
std::array integer_types
{
    "TINYINT",
    "SMALLINT",
    "MEDIUMINT",
    "INT",
    "BIGINT",
};

std::array integer_values
{
    "0",
    "1",
    "-1",
    "20",
    "-20",
    "NULL",
};

std::array decimal_types
{
    "FLOAT",
    "DOUBLE",
    "DECIMAL(10, 2)",
    "DECIMAL(32, 2)",
};

std::array decimal_values
{
    "0",
    "1.5",
    "-1.5",
    "20.5",
    "-20.5",
    "NULL",
};

std::array string_types
{
    "CHAR(50)",
    "VARCHAR(50)",
    "TINYTEXT",
    "TEXT",
    "MEDIUMTEXT",
    "LONGTEXT",
};

std::array string_values
{
    "\"Hello world!\"",
    "\"The quick brown fox jumps over the lazy dog\"",
    "NULL",
};

std::array binary_types
{
    "BINARY(50)",
    "VARBINARY(50)",
    "TINYBLOB",
    "BLOB",
    "MEDIUMBLOB",
    "LONGBLOB",
};

std::array binary_values
{
    "\"Hello world!\"",
    "\"The quick brown fox jumps over the lazy dog\"",
    "NULL",
};

std::array datetime_types
{
    "DATETIME",
};

std::array datetime_values
{
    "'2018-01-01 11:11:11'",
    "'0-00-00 00:00:00'",
    "NULL",
};

std::array datetime2_types
{
    "DATETIME(6)",
};

std::array datetime2_values
{
    "'2018-01-01 11:11:11.000001'",
    "'2018-01-01 11:11:11.000010'",
    "'2018-01-01 11:11:11.000100'",
    "'2018-01-01 11:11:11.001000'",
    "'2018-01-01 11:11:11.010000'",
    "'2018-01-01 11:11:11.100000'",
    "'0-00-00 00:00:00.000000'",
    "NULL",
};

std::array timestamp_types
{
    "TIMESTAMP",
};

std::array timestamp_values
{
    "'2018-01-01 11:11:11'",
    "'0-00-00 00:00:00'",
};

std::array timestamp2_types
{
    "TIMESTAMP(6)",
};

std::array timestamp2_values
{
    "'2018-01-01 11:11:11.000001'",
    "'2018-01-01 11:11:11.000010'",
    "'2018-01-01 11:11:11.000100'",
    "'2018-01-01 11:11:11.001000'",
    "'2018-01-01 11:11:11.010000'",
    "'2018-01-01 11:11:11.100000'",
    "'0-00-00 00:00:00.000000'",
};

std::array date_types
{
    "DATE",
};

std::array date_values
{
    "'2018-01-01'",
    "'0-00-00'",
    "NULL",
};

std::array time_types
{
    "TIME",
};

std::array time_values
{
    "'12:00:00'",
    "NULL",
};

std::array time2_types
{
    "TIME(6)",
};

std::array time2_values
{
    "'12:00:00.000001'",
    "'12:00:00.000010'",
    "'12:00:00.000100'",
    "'12:00:00.001000'",
    "'12:00:00.010000'",
    "'12:00:00.100000'",
    "NULL",
};

const std::string_view DATABASE_NAME = "test";
const std::string_view FIELD_NAME = "a";

std::string type_to_table_name(std::string_view type)
{
    std::string name = mxb::cat("type_", type);
    size_t offset = name.find('(');

    if (offset != std::string::npos)
    {
        name[offset] = '_';

        offset = name.find(')');

        if (offset != std::string::npos)
        {
            name = name.substr(0, offset);
        }

        offset = name.find(',');

        if (offset != std::string::npos)
        {
            name = name.substr(0, offset);
        }
    }

    offset = name.find(' ');

    if (offset != std::string::npos)
    {
        name = name.substr(0, offset);
    }

    return name;
}

std::vector<sql_generation::SQLType> init()
{
    std::vector<sql_generation::SQLType> rval;

    auto add_test = [&](auto types, auto values){
        for (const auto& type : types)
        {
            sql_generation::SQLType sql_type;
            std::string name = type_to_table_name(type);
            sql_type.field_name = FIELD_NAME;
            sql_type.database_name = DATABASE_NAME;
            sql_type.table_name = name;
            sql_type.full_name = mxb::cat(DATABASE_NAME, ".", name);
            sql_type.type_name = type;

            sql_type.create_sql = mxb::cat("CREATE TABLE ", sql_type.full_name,
                                           " (", FIELD_NAME, " ", type, ")");
            sql_type.drop_sql = mxb::cat("DROP TABLE ", sql_type.full_name);

            for (const auto& value : values)
            {
                sql_generation::SQLTypeValue sql_value;
                sql_value.value = value;
                sql_value.insert_sql = mxb::cat("INSERT INTO ", sql_type.full_name, " VALUES (", value, ")");
                sql_type.values.push_back(std::move(sql_value));
            }

            rval.push_back(std::move(sql_type));
        }
    };

    add_test(integer_types, integer_values);
    add_test(decimal_types, decimal_values);
    add_test(string_types, string_values);
    add_test(binary_types, binary_values);
    add_test(datetime_types, datetime_values);
    add_test(timestamp_types, timestamp_values);
    add_test(date_types, date_values);
    add_test(time_types, time_values);
    add_test(datetime2_types, datetime2_values);
    add_test(timestamp2_types, timestamp2_values);

    return rval;
}
}

namespace sql_generation
{
const std::vector<SQLType>& mariadb_types()
{
    static auto test_set = init();
    return test_set;
}
}