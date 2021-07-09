/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2025-07-14
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

//
// https://docs.mongodb.com/manual/reference/command/nav-aggregation/
//

#include "defs.hh"

namespace nosql
{

namespace command
{

// https://docs.mongodb.com/manual/reference/command/aggregate/

// https://docs.mongodb.com/manual/reference/command/count/
class Count final : public SingleCommand
{
public:
    static constexpr const char* const KEY = "count";
    static constexpr const char* const HELP = "";

    using SingleCommand::SingleCommand;

    string generate_sql() override
    {
        ostringstream sql;

        string limit = convert_skip_and_limit();

        if (limit.empty())
        {
            sql << "SELECT count(id) FROM " << table() << " ";
        }
        else
        {
            // A simple 'SELECT count(...) ... LIMIT ...' returns an empty set with no information.
            sql << "SELECT count(id) FROM (SELECT id FROM " << table() << " ";
        }

        bsoncxx::document::view query;
        if (optional(key::QUERY, &query))
        {
            sql << query_to_where_clause(query);
        }

        if (!limit.empty())
        {
            sql << limit << ") AS t";
        }

        return sql.str();
    }

    State translate(mxs::Buffer&& mariadb_response, GWBUF** ppResponse) override
    {
        ComResponse response(mariadb_response.data());

        int32_t ok = 0;
        int32_t n = 0;

        switch (response.type())
        {
        case ComResponse::ERR_PACKET:
            {
                ComERR err(response);

                auto code = err.code();

                if (code == ER_NO_SUCH_TABLE)
                {
                    ok = 1;
                }
                else
                {
                    throw MariaDBError(err);
                }
            }
            break;

        case ComResponse::OK_PACKET:
        case ComResponse::LOCAL_INFILE_PACKET:
            mxb_assert(!true);
            throw_unexpected_packet();

        default:
            ok = 1;
            n = get_n(GWBUF_DATA(mariadb_response.get()));
        }

        DocumentBuilder doc;

        doc.append(kvp(key::N, n));
        doc.append(kvp(key::OK, ok));

        *ppResponse = create_response(doc.extract());
        return READY;
    }

private:
    int32_t get_n(uint8_t* pBuffer)
    {
        int32_t n = 0;

        ComQueryResponse cqr(&pBuffer);
        mxb_assert(cqr.nFields());

        ComQueryResponse::ColumnDef column_def(&pBuffer);
        vector<enum_field_types> types { column_def.type() };

        ComResponse eof(&pBuffer);
        mxb_assert(eof.type() == ComResponse::EOF_PACKET);

        CQRTextResultsetRow row(&pBuffer, types);

        auto it = row.begin();
        mxb_assert(it != row.end());

        const auto& value = *it++;
        mxb_assert(it == row.end());

        n = std::stoi(value.as_string().to_string());

        return n;
    }
};

// https://docs.mongodb.com/manual/reference/command/distinct/
class Distinct final : public SingleCommand
{
public:
    static constexpr const char* const KEY = "distinct";
    static constexpr const char* const HELP = "";

    using SingleCommand::SingleCommand;

    string generate_sql() override
    {
        ostringstream sql;

        string key = required<string>(key::KEY);

        // TODO: Move these key checks somewhere more common.
        if (key.size() == 0)
        {
            throw SoftError("FieldPath cannot be constructed with empty string", error::LOCATION40352);
        }

        if (key.back() == '.')
        {
            throw SoftError("FieldPath must not end with a '.'.", error::LOCATION40353);
        }

        string extract = "JSON_EXTRACT(doc, '$." + key + "')";

        sql << "SELECT DISTINCT(" << extract << ") FROM " << table() << " ";

        bsoncxx::document::view query;
        if (optional(key::QUERY, &query))
        {
            sql << query_to_where_clause(query);
            sql << "AND ";
        }
        else
        {
            sql << "WHERE ";
        }

        sql << extract << " IS NOT NULL";

        return sql.str();
    }

    State translate(mxs::Buffer&& mariadb_response, GWBUF** ppResponse) override
    {
        uint8_t* pBuffer = mariadb_response.data();

        ComResponse response(pBuffer);

        int32_t ok = 0;
        ostringstream json;
        json << "{ \"values\": [";

        switch (response.type())
        {
        case ComResponse::ERR_PACKET:
            {
                ComERR err(response);

                auto code = err.code();

                if (code == ER_NO_SUCH_TABLE)
                {
                    ok = 1;
                }
                else
                {
                    throw MariaDBError(err);
                }
            }
            break;

        case ComResponse::OK_PACKET:
        case ComResponse::LOCAL_INFILE_PACKET:
            mxb_assert(!true);
            throw_unexpected_packet();

        default:
            {
                ok = 1;

                ComQueryResponse cqr(&pBuffer);
                mxb_assert(cqr.nFields() == 1);

                ComQueryResponse::ColumnDef column_def(&pBuffer);
                vector<enum_field_types> types { column_def.type() };

                ComResponse eof(&pBuffer);
                mxb_assert(eof.type() == ComResponse::EOF_PACKET);

                bool first = true;
                while (ComResponse(pBuffer).type() != ComResponse::EOF_PACKET)
                {
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        json << ", ";
                    }

                    CQRTextResultsetRow row(&pBuffer, types); // Advances pBuffer
                    auto it = row.begin();

                    json << (*it).as_string().to_string();
                }
            }
        }

        json << "], \"ok\": " << ok << "}";

        auto doc = bsoncxx::from_json(json.str());

        *ppResponse = create_response(doc);
        return READY;
    }
};

// https://docs.mongodb.com/manual/reference/command/mapReduce/


}

}
