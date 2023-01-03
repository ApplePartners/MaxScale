/*
 * Copyright (c) 2016 MariaDB Corporation Ab
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

#define MXB_MODULE_NAME "cache"
#include "rules.hh"

#include <errno.h>
#include <stdio.h>
#include <new>
#include <vector>

#include <maxbase/alloc.hh>
#include <maxbase/string.hh>
#include <maxscale/config.hh>
#include <maxscale/modutil.hh>
#include <maxscale/mysql_utils.hh>
#include <maxscale/protocol/mariadb/mysql.hh>
#include <maxscale/protocol/mariadb/query_classifier.hh>
#include <maxscale/session.hh>

#include "cachefilter.hh"

using mxb::sv_case_eq;

static const char KEY_ATTRIBUTE[] = "attribute";
static const char KEY_OP[] = "op";
static const char KEY_STORE[] = "store";
static const char KEY_USE[] = "use";
static const char KEY_VALUE[] = "value";

static const char VALUE_ATTRIBUTE_COLUMN[] = "column";
static const char VALUE_ATTRIBUTE_DATABASE[] = "database";
static const char VALUE_ATTRIBUTE_QUERY[] = "query";
static const char VALUE_ATTRIBUTE_TABLE[] = "table";
static const char VALUE_ATTRIBUTE_USER[] = "user";

static const char VALUE_OP_EQ[] = "=";
static const char VALUE_OP_NEQ[] = "!=";
static const char VALUE_OP_LIKE[] = "like";
static const char VALUE_OP_UNLIKE[] = "unlike";

struct cache_attribute_mapping
{
    const char*            name;
    cache_rule_attribute_t value;
};

static struct cache_attribute_mapping cache_store_attributes[] =
{
    {VALUE_ATTRIBUTE_COLUMN,   CACHE_ATTRIBUTE_COLUMN                },
    {VALUE_ATTRIBUTE_DATABASE, CACHE_ATTRIBUTE_DATABASE              },
    {VALUE_ATTRIBUTE_QUERY,    CACHE_ATTRIBUTE_QUERY                 },
    {VALUE_ATTRIBUTE_TABLE,    CACHE_ATTRIBUTE_TABLE                 },
    {nullptr,                  static_cast<cache_rule_attribute_t>(0)}
};

static struct cache_attribute_mapping cache_use_attributes[] =
{
    {VALUE_ATTRIBUTE_USER, CACHE_ATTRIBUTE_USER                  },
    {nullptr,              static_cast<cache_rule_attribute_t>(0)}
};

static bool cache_rule_attribute_get(struct cache_attribute_mapping* mapping,
                                     const char* s,
                                     cache_rule_attribute_t* attribute);

static bool cache_rule_op_get(const char* s, cache_rule_op_t* op);

static bool       cache_rule_compare_n(CacheRule* rule, const char* value, size_t length);
static CacheRule* cache_rule_create_regexp(cache_rule_attribute_t attribute,
                                           cache_rule_op_t op,
                                           const char* value,
                                           uint32_t debug);
static CacheRule* cache_rule_create_simple(cache_rule_attribute_t attribute,
                                           cache_rule_op_t op,
                                           const char* value,
                                           uint32_t debug);
static CacheRule* cache_rule_create_simple_ctd(cache_rule_attribute_t attribute,
                                               cache_rule_op_t op,
                                               const char* cvalue,
                                               uint32_t debug);
static CacheRule* cache_rule_create_simple_user(cache_rule_attribute_t attribute,
                                                cache_rule_op_t op,
                                                const char* cvalue,
                                                uint32_t debug);
static CacheRule* cache_rule_create_simple_query(cache_rule_attribute_t attribute,
                                                 cache_rule_op_t op,
                                                 const char* cvalue,
                                                 uint32_t debug);
static CacheRule* cache_rule_create(cache_rule_attribute_t attribute,
                                    cache_rule_op_t op,
                                    const char* value,
                                    uint32_t debug);
static bool cache_rule_matches_column_regexp(CacheRule* rule,
                                             const char* default_db,
                                             const GWBUF* query);
static bool cache_rule_matches_column_simple(CacheRule* rule,
                                             const char* default_db,
                                             const GWBUF* query);
static bool cache_rule_matches_column(CacheRule* rule,
                                      const char* default_db,
                                      const GWBUF* query);
static bool cache_rule_matches_database(CacheRule* rule,
                                        const char* default_db,
                                        const GWBUF* query);
static bool cache_rule_matches_query(CacheRule* rule,
                                     const char* default_db,
                                     const GWBUF* query);
static bool cache_rule_matches_table(CacheRule* rule,
                                     const char* default_db,
                                     const GWBUF* query);
static bool cache_rule_matches_table_regexp(CacheRule* rule,
                                            const char* default_db,
                                            const GWBUF* query);
static bool cache_rule_matches_table_simple(CacheRule* rule,
                                            const char* default_db,
                                            const GWBUF* query);
static bool cache_rule_matches_user(CacheRule* rule, const char* user);
static bool cache_rule_matches(CacheRule* rule,
                               const char* default_db,
                               const GWBUF* query);

static void         cache_rules_add_store_rule(CACHE_RULES* self, CacheRule* rule);
static void         cache_rules_add_use_rule(CACHE_RULES* self, CacheRule* rule);
static CACHE_RULES* cache_rules_create_from_json(json_t* root, uint32_t debug);
static bool         cache_rules_create_from_json(json_t* root,
                                                 uint32_t debug,
                                                 CACHE_RULES*** ppRules,
                                                 int32_t* pnRules);
static bool cache_rules_parse_json(CACHE_RULES* self, json_t* root);

typedef bool (* cache_rules_parse_element_t)(CACHE_RULES* self, json_t* object, size_t index);

static bool cache_rules_parse_array(CACHE_RULES * self, json_t* store, const char* name,
                                    cache_rules_parse_element_t);
static bool cache_rules_parse_store_element(CACHE_RULES* self, json_t* object, size_t index);
static bool cache_rules_parse_use_element(CACHE_RULES* self, json_t* object, size_t index);

/*
 * API begin
 */

const char* cache_rule_attribute_to_string(cache_rule_attribute_t attribute)
{
    switch (attribute)
    {
    case CACHE_ATTRIBUTE_COLUMN:
        return "column";

    case CACHE_ATTRIBUTE_DATABASE:
        return "database";

    case CACHE_ATTRIBUTE_QUERY:
        return "query";

    case CACHE_ATTRIBUTE_TABLE:
        return "table";

    case CACHE_ATTRIBUTE_USER:
        return "user";

    default:
        mxb_assert(!true);
        return "<invalid>";
    }
}

const char* cache_rule_op_to_string(cache_rule_op_t op)
{
    switch (op)
    {
    case CACHE_OP_EQ:
        return "=";

    case CACHE_OP_NEQ:
        return "!=";

    case CACHE_OP_LIKE:
        return "like";

    case CACHE_OP_UNLIKE:
        return "unlike";

    default:
        mxb_assert(!true);
        return "<invalid>";
    }
}

CACHE_RULES* cache_rules_create(uint32_t debug)
{
    CACHE_RULES* rules = (CACHE_RULES*)MXB_CALLOC(1, sizeof(CACHE_RULES));

    if (rules)
    {
        rules->debug = debug;
    }

    return rules;
}

bool cache_rules_load(const char* zPath,
                      uint32_t debug,
                      CACHE_RULES*** pppRules,
                      int32_t* pnRules)
{
    bool rv = false;

    *pppRules = nullptr;
    *pnRules = 0;

    FILE* pF = fopen(zPath, "r");

    if (pF)
    {
        json_error_t error;
        json_t* pRoot = json_loadf(pF, JSON_DISABLE_EOF_CHECK, &error);

        if (pRoot)
        {
            rv = cache_rules_create_from_json(pRoot, debug, pppRules, pnRules);

            if (!rv)
            {
                json_decref(pRoot);
            }
        }
        else
        {
            MXB_ERROR("Loading rules file failed: (%s:%d:%d): %s",
                      zPath,
                      error.line,
                      error.column,
                      error.text);
        }

        fclose(pF);
    }
    else
    {
        MXB_ERROR("Could not open rules file %s for reading: %s",
                  zPath,
                  mxb_strerror(errno));
    }

    return rv;
}

bool cache_rules_parse(const char* zJson,
                       uint32_t debug,
                       CACHE_RULES*** pppRules,
                       int32_t* pnRules)
{
    bool rv = false;

    *pppRules = nullptr;
    *pnRules = 0;

    json_error_t error;
    json_t* pRoot = json_loads(zJson, JSON_DISABLE_EOF_CHECK, &error);

    if (pRoot)
    {
        rv = cache_rules_create_from_json(pRoot, debug, pppRules, pnRules);

        if (!rv)
        {
            json_decref(pRoot);
        }
    }
    else
    {
        MXB_ERROR("Parsing rules failed: (%d:%d): %s",
                  error.line,
                  error.column,
                  error.text);
    }

    return rv;
}

void cache_rules_free(CACHE_RULES* rules)
{
    if (rules)
    {
        if (rules->root)
        {
            json_decref(rules->root);
        }

        delete rules->store_rules;
        rules->store_rules = nullptr;
        delete rules->use_rules;
        rules->use_rules = nullptr;
        MXB_FREE(rules);
    }
}

void cache_rules_free_array(CACHE_RULES** ppRules, int32_t nRules)
{
    for (auto i = 0; i < nRules; ++i)
    {
        cache_rules_free(ppRules[i]);
    }

    MXB_FREE(ppRules);
}

bool cache_rules_should_store(CACHE_RULES* self, const char* default_db, const GWBUF* query)
{
    bool should_store = false;

    CacheRule* rule = self->store_rules;

    if (rule)
    {
        while (rule && !should_store)
        {
            should_store = cache_rule_matches(rule, default_db, query);
            rule = rule->m_pNext;
        }
    }
    else
    {
        should_store = true;
    }

    return should_store;
}

bool cache_rules_should_use(CACHE_RULES* self, const MXS_SESSION* session)
{
    bool should_use = false;

    CacheRule* rule = self->use_rules;
    const char* user = session->user().c_str();
    const char* host = session->client_remote().c_str();

    if (rule)
    {
        char account[strlen(user) + 1 + strlen(host) + 1];
        sprintf(account, "%s@%s", user, host);

        while (rule && !should_use)
        {
            should_use = cache_rule_matches_user(rule, account);
            rule = rule->m_pNext;
        }
    }
    else
    {
        should_use = true;
    }

    return should_use;
}


CacheRules::CacheRules(CACHE_RULES* pRules)
    : m_pRules(pRules)
{
}

CacheRules::~CacheRules()
{
    cache_rules_free(m_pRules);
}

// static
std::unique_ptr<CacheRules> CacheRules::create(uint32_t debug)
{
    std::unique_ptr<CacheRules> sThis;

    CACHE_RULES* pRules = cache_rules_create(debug);

    if (pRules)
    {
        sThis = std::unique_ptr<CacheRules>(new(std::nothrow) CacheRules(pRules));
    }

    return sThis;
}

// static
bool CacheRules::parse(const char* zJson, uint32_t debug, std::vector<SCacheRules>* pRules)
{
    bool rv = false;

    pRules->clear();

    CACHE_RULES** ppRules;
    int32_t nRules;

    if (cache_rules_parse(zJson, debug, &ppRules, &nRules))
    {
        rv = create_cache_rules(ppRules, nRules, pRules);
    }

    return rv;
}

// static
bool CacheRules::load(const char* zPath, uint32_t debug, std::vector<SCacheRules>* pRules)
{
    bool rv = false;

    pRules->clear();

    CACHE_RULES** ppRules;
    int32_t nRules;

    if (cache_rules_load(zPath, debug, &ppRules, &nRules))
    {
        rv = create_cache_rules(ppRules, nRules, pRules);
    }

    return rv;
}

// static
bool CacheRules::create_cache_rules(CACHE_RULES** ppRules, int32_t nRules, std::vector<SCacheRules>* pRules)
{
    bool rv = false;

    int j = 0;

    try
    {
        std::vector<SCacheRules> rules;
        rules.reserve(nRules);

        for (int i = 0; i < nRules; ++i)
        {
            j = i;
            CacheRules* pRules = new CacheRules(ppRules[i]);
            j = i + 1;

            rules.push_back(SCacheRules(pRules));
        }

        pRules->swap(rules);
        rv = true;
    }
    catch (const std::exception&)
    {
        // Free all CACHE_RULES objects that were not pushed into 'rules' above.
        for (; j < nRules; ++j)
        {
            cache_rules_free(ppRules[j]);
        }
    }

    MXB_FREE(ppRules);

    return rv;
}

const json_t* CacheRules::json() const
{
    return m_pRules->root;
}

bool CacheRules::should_store(const char* zDefault_db, const GWBUF* pQuery) const
{
    return cache_rules_should_store(m_pRules, zDefault_db, pQuery);
}

bool CacheRules::should_use(const MXS_SESSION* pSession) const
{
    return cache_rules_should_use(m_pRules, pSession);
}

/*
 * API end
 */

/**
 * Converts a string to an attribute
 *
 * @param           Name/value mapping.
 * @param s         A string
 * @param attribute On successful return contains the corresponding attribute type.
 *
 * @return True if the string could be converted, false otherwise.
 */
static bool cache_rule_attribute_get(struct cache_attribute_mapping* mapping,
                                     const char* s,
                                     cache_rule_attribute_t* attribute)
{
    mxb_assert(attribute);

    while (mapping->name)
    {
        if (strcmp(s, mapping->name) == 0)
        {
            *attribute = mapping->value;
            return true;
        }
        ++mapping;
    }

    return false;
}

/**
 * Converts a string to an operator
 *
 * @param s A string
 * @param op On successful return contains the corresponding operator.
 *
 * @return True if the string could be converted, false otherwise.
 */
static bool cache_rule_op_get(const char* s, cache_rule_op_t* op)
{
    if (strcmp(s, VALUE_OP_EQ) == 0)
    {
        *op = CACHE_OP_EQ;
        return true;
    }

    if (strcmp(s, VALUE_OP_NEQ) == 0)
    {
        *op = CACHE_OP_NEQ;
        return true;
    }

    if (strcmp(s, VALUE_OP_LIKE) == 0)
    {
        *op = CACHE_OP_LIKE;
        return true;
    }

    if (strcmp(s, VALUE_OP_UNLIKE) == 0)
    {
        *op = CACHE_OP_UNLIKE;
        return true;
    }

    return false;
}

/**
 * Creates a CacheRule object doing regexp matching.
 *
 * @param attribute What attribute this rule applies to.
 * @param op        An operator, CACHE_OP_LIKE or CACHE_OP_UNLIKE.
 * @param value     A regular expression.
 * @param debug     The debug level.
 *
 * @return A new rule object or nullptr in case of failure.
 */
static CacheRule* cache_rule_create_regexp(cache_rule_attribute_t attribute,
                                           cache_rule_op_t op,
                                           const char* zValue,
                                           uint32_t debug)
{
    mxb_assert((op == CACHE_OP_LIKE) || (op == CACHE_OP_UNLIKE));

    CacheRule* pRule = nullptr;

    int errcode;
    PCRE2_SIZE erroffset;
    pcre2_code* pCode = pcre2_compile((PCRE2_SPTR)zValue,
                                      PCRE2_ZERO_TERMINATED,
                                      0,
                                      &errcode,
                                      &erroffset,
                                      nullptr);

    if (pCode)
    {
        // We do not care about the result. If JIT is not present, we have
        // complained about it already.
        pcre2_jit_compile(pCode, PCRE2_JIT_COMPLETE);

        pRule = new CacheRuleRegex(attribute, op, zValue, debug);
        pRule->m_regexp.code = pCode;
    }
    else
    {
        PCRE2_UCHAR errbuf[512];
        pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
        MXB_ERROR("Regex compilation failed at %d for regex '%s': %s",
                  (int)erroffset,
                  zValue,
                  errbuf);
    }

    return pRule;
}

/**
 * Creates a CacheRule object matching users.
 *
 * @param attribute CACHE_ATTRIBUTE_USER
 * @param op        An operator, CACHE_OP_EQ or CACHE_OP_NEQ.
 * @param cvalue    A string in the MySQL user format.
 * @param debug     The debug level.
 *
 * @return A new rule object or nullptr in case of failure.
 */
static CacheRule* cache_rule_create_simple_user(cache_rule_attribute_t attribute,
                                                cache_rule_op_t op,
                                                const char* cvalue,
                                                uint32_t debug)
{
    CacheRule* pRule = nullptr;

    mxb_assert(attribute == CACHE_ATTRIBUTE_USER);
    mxb_assert((op == CACHE_OP_EQ) || (op == CACHE_OP_NEQ));

    bool error = false;
    size_t len = strlen(cvalue);

    char value[strlen(cvalue) + 1];
    strcpy(value, cvalue);

    char* at = strchr(value, '@');
    char* user = value;
    char* host;
    char any[2];    // sizeof("%")

    if (at)
    {
        *at = 0;
        host = at + 1;
    }
    else
    {
        strcpy(any, "%");
        host = any;
    }

    if (mxs_mysql_trim_quotes(user))
    {
        char pcre_user[2 * len + 1];    // Surely enough

        if (*user == 0)
        {
            strcpy(pcre_user, ".*");
        }
        else
        {
            mxs_mysql_name_to_pcre(pcre_user, user, MXS_PCRE_QUOTE_VERBATIM);
        }

        if (mxs_mysql_trim_quotes(host))
        {
            char pcre_host[2 * len + 1];    // Surely enough

            mxs_mysql_name_kind_t rv = mxs_mysql_name_to_pcre(pcre_host, host, MXS_PCRE_QUOTE_WILDCARD);

            if (rv == MXS_MYSQL_NAME_WITH_WILDCARD)
            {
                op = (op == CACHE_OP_EQ ? CACHE_OP_LIKE : CACHE_OP_UNLIKE);

                char regexp[strlen(pcre_user) + 1 + strlen(pcre_host) + 1];

                sprintf(regexp, "%s@%s", pcre_user, pcre_host);

                pRule = cache_rule_create_regexp(attribute, op, regexp, debug);
            }
            else
            {
                // No wildcard, no need to use regexp.

                std::string value = user;
                value += "@";
                value += host;

                pRule = new CacheRuleSimple(attribute, op, std::move(value), debug);
            }
        }
        else
        {
            MXB_ERROR("Could not trim quotes from host %s.", cvalue);
        }
    }
    else
    {
        MXB_ERROR("Could not trim quotes from user %s.", cvalue);
    }

    return pRule;
}

/**
 * Creates a CacheRule object matching column/table/database.
 *
 * @param attribute CACHE_ATTRIBUTE_[COLUMN|TABLE|DATABASE]
 * @param op        An operator, CACHE_OP_EQ or CACHE_OP_NEQ.
 * @param cvalue    A name, with 0, 1 or 2 dots.
 * @param debug     The debug level.
 *
 * @return A new rule object or nullptr in case of failure.
 */
static CacheRule* cache_rule_create_simple_ctd(cache_rule_attribute_t attribute,
                                               cache_rule_op_t op,
                                               const char* zValue,
                                               uint32_t debug)
{
    mxb_assert((attribute == CACHE_ATTRIBUTE_COLUMN)
               || (attribute == CACHE_ATTRIBUTE_TABLE)
               || (attribute == CACHE_ATTRIBUTE_DATABASE));
    mxb_assert((op == CACHE_OP_EQ) || (op == CACHE_OP_NEQ));

    CacheRule* pRule = new CacheRuleSimple(attribute, op, zValue, debug);

    bool error = false;

    char buffer[strlen(zValue) + 1];
    strcpy(buffer, zValue);

    const char* zFirst = nullptr;
    const char* zSecond = nullptr;
    const char* zThird = nullptr;
    char* zDot1 = strchr(buffer, '.');
    char* zDot2 = zDot1 ? strchr(zDot1 + 1, '.') : nullptr;

    if (zDot1 && zDot2)
    {
        zFirst = buffer;
        *zDot1 = 0;
        zSecond = zDot1 + 1;
        *zDot2 = 0;
        zThird = zDot2 + 1;
    }
    else if (zDot1)
    {
        zFirst = buffer;
        *zDot1 = 0;
        zSecond = zDot1 + 1;
    }
    else
    {
        zFirst = buffer;
    }

    switch (attribute)
    {
    case CACHE_ATTRIBUTE_COLUMN:
        {
            if (zThird)      // implies also 'first' and 'second'
            {
                pRule->m_simple.column = zThird;
                pRule->m_simple.table = zSecond;
                pRule->m_simple.database = zFirst;
            }
            else if (zSecond)    // implies also 'first'
            {
                pRule->m_simple.column = zSecond;
                pRule->m_simple.table = zFirst;
            }
            else    // only 'zFirst'
            {
                pRule->m_simple.column = zFirst;
            }
        }
        break;

    case CACHE_ATTRIBUTE_TABLE:
        if (zThird)
        {
            MXB_ERROR("A cache rule value for matching a table name, cannot contain two dots: '%s'",
                      zValue);
            error = true;
        }
        else
        {
            if (zSecond)     // implies also 'zFirst'
            {
                pRule->m_simple.database = zFirst;
                pRule->m_simple.table = zSecond;
            }
            else    // only 'zFirst'
            {
                pRule->m_simple.table = zFirst;
            }
        }
        break;

    case CACHE_ATTRIBUTE_DATABASE:
        if (zSecond)
        {
            MXB_ERROR("A cache rule value for matching a database, cannot contain a dot: '%s'",
                      zValue);
            error = true;
        }
        else
        {
            pRule->m_simple.database = zFirst;
        }
        break;

    default:
        mxb_assert(!true);
    }

    if (error)
    {
        delete pRule;
        pRule = nullptr;
    }

    return pRule;
}

/**
 * Creates a CacheRule object matching an entire query.
 *
 * @param attribute CACHE_ATTRIBUTE_QUERY.
 * @param op        An operator, CACHE_OP_EQ or CACHE_OP_NEQ.
 * @param cvalue    A string.
 * @param debug     The debug level.
 *
 * @return A new rule object or nullptr in case of failure.
 */
static CacheRule* cache_rule_create_simple_query(cache_rule_attribute_t attribute,
                                                 cache_rule_op_t op,
                                                 const char* zValue,
                                                 uint32_t debug)
{
    mxb_assert(attribute == CACHE_ATTRIBUTE_QUERY);
    mxb_assert((op == CACHE_OP_EQ) || (op == CACHE_OP_NEQ));

    CacheRule* pRule = new CacheRuleSimple(attribute, op, zValue, debug);

    return pRule;
}

/**
 * Creates a CacheRule object doing simple matching.
 *
 * @param attribute What attribute this rule applies to.
 * @param op        An operator, CACHE_OP_EQ or CACHE_OP_NEQ.
 * @param value     A string.
 * @param debug     The debug level.
 *
 * @return A new rule object or nullptr in case of failure.
 */
static CacheRule* cache_rule_create_simple(cache_rule_attribute_t attribute,
                                           cache_rule_op_t op,
                                           const char* cvalue,
                                           uint32_t debug)
{
    mxb_assert((op == CACHE_OP_EQ) || (op == CACHE_OP_NEQ));

    CacheRule* rule = nullptr;

    switch (attribute)
    {
    case CACHE_ATTRIBUTE_COLUMN:
    case CACHE_ATTRIBUTE_TABLE:
    case CACHE_ATTRIBUTE_DATABASE:
        rule = cache_rule_create_simple_ctd(attribute, op, cvalue, debug);
        break;

    case CACHE_ATTRIBUTE_USER:
        rule = cache_rule_create_simple_user(attribute, op, cvalue, debug);
        break;

    case CACHE_ATTRIBUTE_QUERY:
        rule = cache_rule_create_simple_query(attribute, op, cvalue, debug);
        break;

    default:
        MXB_ERROR("Unknown attribute type: %d", (int)attribute);
        mxb_assert(!true);
    }

    return rule;
}

/**
 * Creates a CacheRule object.
 *
 * @param attribute What attribute this rule applies to.
 * @param op        What operator is used.
 * @param value     The value.
 * @param debug     The debug level.
 *
 * @param rule The rule to be freed.
 */
static CacheRule* cache_rule_create(cache_rule_attribute_t attribute,
                                    cache_rule_op_t op,
                                    const char* value,
                                    uint32_t debug)
{
    CacheRule* rule = nullptr;

    switch (op)
    {
    case CACHE_OP_EQ:
    case CACHE_OP_NEQ:
        rule = cache_rule_create_simple(attribute, op, value, debug);
        break;

    case CACHE_OP_LIKE:
    case CACHE_OP_UNLIKE:
        rule = cache_rule_create_regexp(attribute, op, value, debug);
        break;

    default:
        mxb_assert(!true);
        MXB_ERROR("Internal error.");
        break;
    }

    return rule;
}

/**
 * Frees a CacheRule object (and the one it points to).
 *
 * @param rule The rule to be freed.
 */
CacheRule::~CacheRule()
{
    if (this->m_pNext)
    {
        delete this->m_pNext;
        this->m_pNext = nullptr;
    }

    if ((this->m_op == CACHE_OP_LIKE) || (this->m_op == CACHE_OP_UNLIKE))
    {
        pcre2_code_free(this->m_regexp.code);
    }
}

/**
 * Check whether a value matches a rule.
 *
 * @param self       The rule object.
 * @param value      The value to check.
 *
 * @return True if the value matches, false otherwise.
 */
bool CacheRule::compare(const std::string_view& value) const
{
    bool rv;

    if (!value.empty())
    {
        rv = cache_rule_compare_n(const_cast<CacheRule*>(this), value.data(), value.length());
    }
    else
    {
        if ((m_op == CACHE_OP_EQ) || (m_op == CACHE_OP_LIKE))
        {
            rv = false;
        }
        else
        {
            rv = true;
        }
    }

    return rv;
}

/**
 * Check whether a value matches a rule.
 *
 * @param self       The rule object.
 * @param value      The value to check.
 * @param len        The length of value.
 *
 * @return True if the value matches, false otherwise.
 */
static bool cache_rule_compare_n(CacheRule* self, const char* zValue, size_t length)
{
    bool compares = false;

    switch (self->m_op)
    {
    case CACHE_OP_EQ:
    case CACHE_OP_NEQ:
        compares = (strncmp(self->m_value.c_str(), zValue, length) == 0);
        break;

    case CACHE_OP_LIKE:
    case CACHE_OP_UNLIKE:
        {
            pcre2_match_data* pData = pcre2_match_data_create_from_pattern(self->m_regexp.code, nullptr);
            compares = (pcre2_match(self->m_regexp.code,
                                    (PCRE2_SPTR)zValue,
                                    length,
                                    0,
                                    0,
                                    pData,
                                    nullptr) >= 0);
            pcre2_match_data_free(pData);
        }
        break;

    default:
        mxb_assert(!true);
    }

    if ((self->m_op == CACHE_OP_NEQ) || (self->m_op == CACHE_OP_UNLIKE))
    {
        compares = !compares;
    }

    return compares;
}

/**
 * Returns boolean indicating whether the column rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_column_regexp(CacheRule* self,
                                             const char* default_db,
                                             const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_COLUMN);
    mxb_assert((self->m_op == CACHE_OP_LIKE) || (self->m_op == CACHE_OP_UNLIKE));

    std::string_view default_database;

    int n_databases;
    auto databases = qc_get_database_names((GWBUF*)query);

    if (databases.empty())
    {
        // If no databases have been mentioned, then we can assume that all
        // tables and columns that are not explcitly qualified refer to the
        // default database.
        if (default_db)
        {
            default_database = default_db;
        }
    }
    else if ((default_db == nullptr) && (databases.size() == 1))
    {
        // If there is no default database and exactly one database has been
        // explicitly mentioned, then we can assume all tables and columns that
        // are not explicitly qualified refer to that database.
        default_database = databases[0];
    }

    size_t default_database_len = default_database.length();

    auto tables = qc_get_table_names((GWBUF*)query);

    std::string_view default_table;

    if (tables.size() == 1)
    {
        // Only if we have exactly one table can we assume anything
        // about a table that has not been mentioned explicitly.
        default_table = tables[0].table;
    }

    size_t default_table_len = default_table.length();

    const QC_FIELD_INFO* infos;
    size_t n_infos;

    qc_get_field_info((GWBUF*)query, &infos, &n_infos);

    bool matches = false;

    size_t i = 0;
    while (!matches && (i < n_infos))
    {
        const QC_FIELD_INFO* info = (infos + i);

        std::string_view database;
        size_t database_len;

        if (!info->database.empty())
        {
            database = info->database;
            database_len = database.length();
        }
        else
        {
            database = default_database;
            database_len = default_database_len;
        }

        size_t table_len;
        std::string_view table;

        if (!info->table.empty())
        {
            table = info->table;
            table_len = table.length();
        }
        else
        {
            table = default_table;
            table_len = default_table_len;
        }

        char buffer[database_len + 1 + table_len + 1 + info->column.length() + 1];
        buffer[0] = 0;

        if (!database.empty())
        {
            strncat(buffer, database.data(), database.length());
            strcat(buffer, ".");
        }

        if (!table.empty())
        {
            strncat(buffer, table.data(), table.length());
            strcat(buffer, ".");
        }

        strncat(buffer, info->column.data(), info->column.length());

        matches = self->compare(buffer);

        ++i;
    }

    return matches;
}

/**
 * Returns boolean indicating whether the column rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_column_simple(CacheRule* self, const char* default_db, const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_COLUMN);
    mxb_assert((self->m_op == CACHE_OP_EQ) || (self->m_op == CACHE_OP_NEQ));
    mxb_assert(!self->m_simple.column.empty());

    const char* zRule_column = self->m_simple.column.c_str();
    const char* zRule_table = self->m_simple.table.empty() ? nullptr : self->m_simple.table.c_str();
    const char* zRule_database = self->m_simple.database.empty() ? nullptr : self->m_simple.database.c_str();

    std::string_view default_database;

    auto databases = qc_get_database_names((GWBUF*)query);

    if (databases.empty())
    {
        // If no databases have been mentioned, then we can assume that all
        // tables and columns that are not explcitly qualified refer to the
        // default database.
        if (default_db)
        {
            default_database = default_db;
        }
    }
    else if ((default_db == nullptr) && (databases.size() == 1))
    {
        // If there is no default database and exactly one database has been
        // explicitly mentioned, then we can assume all tables and columns that
        // are not explicitly qualified refer to that database.
        default_database = databases[0];
    }

    auto tables = qc_get_table_names((GWBUF*)query);

    std::string_view default_table;

    if (tables.size() == 1)
    {
        // Only if we have exactly one table can we assume anything
        // about a table that has not been mentioned explicitly.
        default_table = tables[0].table;
    }

    const QC_FIELD_INFO* infos;
    size_t n_infos;

    qc_get_field_info((GWBUF*)query, &infos, &n_infos);

    bool matches = false;

    size_t i = 0;
    while (!matches && (i < n_infos))
    {
        const QC_FIELD_INFO* info = (infos + i);

        if (sv_case_eq(info->column, zRule_column) || strcmp(zRule_column, "*") == 0)
        {
            if (zRule_table)
            {
                std::string_view check_table = !info->table.empty() ? info->table : default_table;

                if (!check_table.empty())
                {
                    if (sv_case_eq(check_table, zRule_table))
                    {
                        if (zRule_database)
                        {
                            std::string_view check_database =
                                !info->database.empty() ? info->database : default_database;

                            if (!check_database.empty())
                            {
                                if (sv_case_eq(check_database, zRule_database))
                                {
                                    // The column, table and database matched.
                                    matches = true;
                                }
                                else
                                {
                                    // The column, table matched but the database did not.
                                    matches = false;
                                }
                            }
                            else
                            {
                                // If the rules specify a database but we do not know the database,
                                // we consider the databases not to match.
                                matches = false;
                            }
                        }
                        else
                        {
                            // If the rule specifies no database, then if the column and the table
                            // matches, the rule matches.
                            matches = true;
                        }
                    }
                    else
                    {
                        // The column matched, but the table did not.
                        matches = false;
                    }
                }
                else
                {
                    // If the rules specify a table but we do not know the table, we
                    // consider the tables not to match.
                    matches = false;
                }
            }
            else
            {
                // The column matched and there is no table rule.
                matches = true;
            }
        }
        else
        {
            // The column did not match.
            matches = false;
        }

        if (self->m_op == CACHE_OP_NEQ)
        {
            matches = !matches;
        }

        ++i;
    }

    return matches;
}

/**
 * Returns boolean indicating whether the column rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_column(CacheRule* self,
                                      const char* default_db,
                                      const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_COLUMN);

    bool matches = false;

    switch (self->m_op)
    {
    case CACHE_OP_EQ:
    case CACHE_OP_NEQ:
        matches = cache_rule_matches_column_simple(self, default_db, query);
        break;

    case CACHE_OP_LIKE:
    case CACHE_OP_UNLIKE:
        matches = cache_rule_matches_column_regexp(self, default_db, query);
        break;

    default:
        mxb_assert(!true);
    }

    return matches;
}

/**
 * Returns boolean indicating whether the database rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_database(CacheRule* self,
                                        const char* default_db,
                                        const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_DATABASE);

    bool matches = false;
    bool fullnames = true;

    // TODO: Make qc const-correct.
    for (const auto& name : qc_get_table_names((GWBUF*)query))
    {
        if (!name.db.empty())
        {
            matches = self->compare(name.db);
        }
        else
        {
            matches = self->compare(default_db ? default_db : "");
        }

        if (matches)
        {
            break;
        }
    }

    return matches;
}

/**
 * Returns boolean indicating whether the query rule matches the query or not.
 *
 * @param self        The CacheRule object.
 * @param default_db  The current default db.
 * @param query       The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_query(CacheRule* self,
                                     const char* default_db,
                                     const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_QUERY);

    const char* sql;
    int len;

    // Will succeed, query contains a contiguous COM_QUERY.
    modutil_extract_SQL(*query, &sql, &len);

    return cache_rule_compare_n(self, sql, len);
}

/**
 * Returns boolean indicating whether the table regexp rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_table_regexp(CacheRule* self,
                                            const char* default_db,
                                            const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_TABLE);
    mxb_assert((self->m_op == CACHE_OP_LIKE) || (self->m_op == CACHE_OP_UNLIKE));

    bool matches = false;

    auto names = qc_get_table_names((GWBUF*)query);

    if (!names.empty())
    {
        std::string db = default_db ? default_db : "";

        for (const auto& name : names)
        {
            if (name.db.empty())
            {
                // Only "tbl"

                if (default_db)
                {
                    matches = self->compare(db + '.' + std::string(name.table));
                }
                else
                {
                    matches = self->compare(name.table);
                }
            }
            else
            {
                // A qualified name "db.tbl".
                std::string qname(name.db);
                qname += '.';
                qname += name.table;
                matches = self->compare(qname);
            }

            if (matches)
            {
                break;
            }
        }
    }
    else if (self->m_op == CACHE_OP_UNLIKE)
    {
        matches = true;
    }

    return matches;
}

/**
 * Returns boolean indicating whether the table simple rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_table_simple(CacheRule* self, const char* default_db, const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_TABLE);
    mxb_assert((self->m_op == CACHE_OP_EQ) || (self->m_op == CACHE_OP_NEQ));

    bool matches = false;
    bool fullnames = !self->m_simple.database.empty();

    for (const auto& name : qc_get_table_names((GWBUF*)query))
    {
        std::string_view database;
        std::string_view table;

        if (fullnames)
        {
            if (!name.db.empty())
            {
                database = name.db;
                table = name.table;
            }
            else
            {
                database = default_db;
                table = name.table;
            }

            if (!database.empty())
            {
                matches = sv_case_eq(self->m_simple.database, database) && sv_case_eq(self->m_simple.table, table);
            }
        }
        else
        {
            matches = sv_case_eq(self->m_simple.table, name.table);
        }

        if (self->m_op == CACHE_OP_NEQ)
        {
            matches = !matches;
        }

        if (matches)
        {
            break;
        }
    }

    return matches;
}

/**
 * Returns boolean indicating whether the table rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_table(CacheRule* self,
                                     const char* default_db,
                                     const GWBUF* query)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_TABLE);

    bool matches = false;

    switch (self->m_op)
    {
    case CACHE_OP_EQ:
    case CACHE_OP_NEQ:
        matches = cache_rule_matches_table_simple(self, default_db, query);
        break;

    case CACHE_OP_LIKE:
    case CACHE_OP_UNLIKE:
        matches = cache_rule_matches_table_regexp(self, default_db, query);
        break;

    default:
        mxb_assert(!true);
    }

    return matches;
}

/**
 * Returns boolean indicating whether the user rule matches the account or not.
 *
 * @param self       The CacheRule object.
 * @param account    The account.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches_user(CacheRule* self, const char* account)
{
    mxb_assert(self->m_attribute == CACHE_ATTRIBUTE_USER);

    bool matches = self->compare(account);

    if ((matches && (self->m_debug & CACHE_DEBUG_MATCHING))
        || (!matches && (self->m_debug & CACHE_DEBUG_NON_MATCHING)))
    {
        const char* text;
        if (matches)
        {
            text = "MATCHES";
        }
        else
        {
            text = "does NOT match";
        }

        MXB_NOTICE("Rule { \"attribute\": \"%s\", \"op\": \"%s\", \"value\": \"%s\" } %s \"%s\".",
                   cache_rule_attribute_to_string(self->m_attribute),
                   cache_rule_op_to_string(self->m_op),
                   self->m_value.c_str(),
                   text,
                   account);
    }

    return matches;
}

/**
 * Returns boolean indicating whether the rule matches the query or not.
 *
 * @param self       The CacheRule object.
 * @param default_db The current default db.
 * @param query      The query.
 *
 * @return True, if the rule matches, false otherwise.
 */
static bool cache_rule_matches(CacheRule* self, const char* default_db, const GWBUF* query)
{
    bool matches = false;

    switch (self->m_attribute)
    {
    case CACHE_ATTRIBUTE_COLUMN:
        matches = cache_rule_matches_column(self, default_db, query);
        break;

    case CACHE_ATTRIBUTE_DATABASE:
        matches = cache_rule_matches_database(self, default_db, query);
        break;

    case CACHE_ATTRIBUTE_TABLE:
        matches = cache_rule_matches_table(self, default_db, query);
        break;

    case CACHE_ATTRIBUTE_QUERY:
        matches = cache_rule_matches_query(self, default_db, query);
        break;

    case CACHE_ATTRIBUTE_USER:
        mxb_assert(!true);
        break;

    default:
        mxb_assert(!true);
    }

    if ((matches && (self->m_debug & CACHE_DEBUG_MATCHING))
        || (!matches && (self->m_debug & CACHE_DEBUG_NON_MATCHING)))
    {
        const char* sql;
        int sql_len;
        modutil_extract_SQL(*query, &sql, &sql_len);
        const char* text;

        if (matches)
        {
            text = "MATCHES";
        }
        else
        {
            text = "does NOT match";
        }

        MXB_NOTICE("Rule { \"attribute\": \"%s\", \"op\": \"%s\", \"value\": \"%s\" } %s \"%.*s\".",
                   cache_rule_attribute_to_string(self->m_attribute),
                   cache_rule_op_to_string(self->m_op),
                   self->m_value.c_str(),
                   text,
                   sql_len,
                   sql);
    }

    return matches;
}

/**
 * Append a rule to the tail of a chain or rules.
 *
 * @param head The head of the chain, can be nullptr.
 * @param tail The tail to be added to the chain.
 *
 * @return The head.
 */
static CacheRule* cache_rule_append(CacheRule* head, CacheRule* tail)
{
    mxb_assert(tail);

    if (!head)
    {
        head = tail;
    }
    else
    {
        CacheRule* rule = head;

        while (rule->m_pNext)
        {
            rule = rule->m_pNext;
        }

        rule->m_pNext = tail;
    }

    return head;
}

/**
 * Adds a "store" rule to the rules object
 *
 * @param self Pointer to the CACHE_RULES object that is being built.
 * @param rule The rule to be added.
 */
static void cache_rules_add_store_rule(CACHE_RULES* self, CacheRule* rule)
{
    self->store_rules = cache_rule_append(self->store_rules, rule);
}

/**
 * Adds a "store" rule to the rules object
 *
 * @param self Pointer to the CACHE_RULES object that is being built.
 * @param rule The rule to be added.
 */
static void cache_rules_add_use_rule(CACHE_RULES* self, CacheRule* rule)
{
    self->use_rules = cache_rule_append(self->use_rules, rule);
}

/**
 * Creates a rules object from a JSON object.
 *
 * @param root  The root JSON rule object.
 * @param debug The debug level.
 *
 * @return A rules object if the json object could be parsed, nullptr otherwise.
 */
static CACHE_RULES* cache_rules_create_from_json(json_t* root, uint32_t debug)
{
    mxb_assert(root);

    CACHE_RULES* rules = cache_rules_create(debug);

    if (rules)
    {
        if (cache_rules_parse_json(rules, root))
        {
            rules->root = root;
        }
        else
        {
            cache_rules_free(rules);
            rules = nullptr;
        }
    }

    return rules;
}

/**
 * Parses the caching rules from a json object and returns corresponding object(s).
 *
 * @param pRoot    The root JSON object in the rules file.
 * @param debug    The debug level.
 * @param pppRules [out] Pointer to array of pointers to CACHE_RULES objects.
 * @param pnRules  [out] Pointer to number of items in *ppRules.
 *
 * @note The caller must free the array @c *ppRules and each rules
 *       object in the array.
 *
 * @return bool True, if the rules could be parsed, false otherwise.
 */
static bool cache_rules_create_from_json(json_t* pRoot,
                                         uint32_t debug,
                                         CACHE_RULES*** pppRules,
                                         int32_t* pnRules)
{
    bool rv = false;

    *pppRules = nullptr;
    *pnRules = 0;

    if (json_is_array(pRoot))
    {
        int32_t nRules = json_array_size(pRoot);

        CACHE_RULES** ppRules = (CACHE_RULES**)MXB_MALLOC(nRules * sizeof(CACHE_RULES*));

        if (ppRules)
        {
            int i;
            for (i = 0; i < nRules; ++i)
            {
                json_t* pObject = json_array_get(pRoot, i);
                mxb_assert(pObject);

                CACHE_RULES* pRules = cache_rules_create_from_json(pObject, debug);

                if (pRules)
                {
                    ppRules[i] = pRules;
                    // The array element reference was borrowed, so now that we
                    // know a rule could be created, we must increase the reference
                    // count. Otherwise bad things will happen when the reference of
                    // the root object is decreased.
                    json_incref(pObject);
                }
                else
                {
                    break;
                }
            }

            if (i == nRules)
            {
                *pppRules = ppRules;
                *pnRules = nRules;

                // We only store the objects in the array, so now we must get rid
                // of the array so that it does not leak.
                json_decref(pRoot);

                rv = true;
            }
            else
            {
                // Ok, so something went astray.
                for (int j = 0; j < i; ++j)
                {
                    cache_rules_free(ppRules[j]);
                }

                MXB_FREE(ppRules);
            }
        }
    }
    else
    {
        CACHE_RULES** ppRules = (CACHE_RULES**)MXB_MALLOC(1 * sizeof(CACHE_RULES*));

        if (ppRules)
        {
            CACHE_RULES* pRules = cache_rules_create_from_json(pRoot, debug);

            if (pRules)
            {
                ppRules[0] = pRules;

                *pppRules = ppRules;
                *pnRules = 1;

                rv = true;
            }
            else
            {
                MXB_FREE(ppRules);
            }
        }
    }

    return rv;
}

/**
 * Parses the JSON object used for configuring the rules.
 *
 * @param self  Pointer to the CACHE_RULES object that is being built.
 * @param root  The root JSON object in the rules file.
 *
 * @return True, if the object could be parsed, false otherwise.
 */
static bool cache_rules_parse_json(CACHE_RULES* self, json_t* root)
{
    bool parsed = false;
    json_t* store = json_object_get(root, KEY_STORE);

    if (store)
    {
        if (json_is_array(store))
        {
            parsed = cache_rules_parse_array(self, store, KEY_STORE, cache_rules_parse_store_element);
        }
        else
        {
            MXB_ERROR("The cache rules object contains a `%s` key, but it is not an array.", KEY_STORE);
        }
    }

    if (!store || parsed)
    {
        json_t* use = json_object_get(root, KEY_USE);

        if (use)
        {
            if (json_is_array(use))
            {
                parsed = cache_rules_parse_array(self, use, KEY_USE, cache_rules_parse_use_element);
            }
            else
            {
                MXB_ERROR("The cache rules object contains a `%s` key, but it is not an array.", KEY_USE);
            }
        }
        else
        {
            parsed = true;
        }
    }

    return parsed;
}

/**
 * Parses a array.
 *
 * @param self          Pointer to the CACHE_RULES object that is being built.
 * @param array         An array.
 * @param name          The name of the array.
 * @param parse_element Function for parsing an element.
 *
 * @return True, if the array could be parsed, false otherwise.
 */
static bool cache_rules_parse_array(CACHE_RULES* self,
                                    json_t* store,
                                    const char* name,
                                    cache_rules_parse_element_t parse_element)
{
    mxb_assert(json_is_array(store));

    bool parsed = true;

    size_t n = json_array_size(store);
    size_t i = 0;

    while (parsed && (i < n))
    {
        json_t* element = json_array_get(store, i);
        mxb_assert(element);

        if (json_is_object(element))
        {
            parsed = parse_element(self, element, i);
        }
        else
        {
            MXB_ERROR("Element %lu of the '%s' array is not an object.", i, name);
            parsed = false;
        }

        ++i;
    }

    return parsed;
}

/**
 * Parses an object in an array.
 *
 * @param self   Pointer to the CACHE_RULES object that is being built.
 * @param object An object from the "store" array.
 * @param index  Index of the object in the array.
 *
 * @return True, if the object could be parsed, false otherwise.
 */
static CacheRule* cache_rules_parse_element(CACHE_RULES* self,
                                            json_t* object,
                                            const char* array_name,
                                            size_t index,
                                            struct cache_attribute_mapping* mapping)
{
    mxb_assert(json_is_object(object));

    CacheRule* rule = nullptr;

    json_t* a = json_object_get(object, KEY_ATTRIBUTE);
    json_t* o = json_object_get(object, KEY_OP);
    json_t* v = json_object_get(object, KEY_VALUE);

    if (a && o && v && json_is_string(a) && json_is_string(o) && json_is_string(v))
    {
        cache_rule_attribute_t attribute;

        if (cache_rule_attribute_get(mapping, json_string_value(a), &attribute))
        {
            cache_rule_op_t op;

            if (cache_rule_op_get(json_string_value(o), &op))
            {
                rule = cache_rule_create(attribute, op, json_string_value(v), self->debug);
            }
            else
            {
                MXB_ERROR("Element %lu in the `%s` array has an invalid value "
                          "\"%s\" for 'op'.",
                          index,
                          array_name,
                          json_string_value(o));
            }
        }
        else
        {
            MXB_ERROR("Element %lu in the `%s` array has an invalid value "
                      "\"%s\" for 'attribute'.",
                      index,
                      array_name,
                      json_string_value(a));
        }
    }
    else
    {
        MXB_ERROR("Element %lu in the `%s` array does not contain "
                  "'attribute', 'op' and/or 'value', or one or all of them "
                  "is not a string.",
                  index,
                  array_name);
    }

    return rule;
}


/**
 * Parses an object in the "store" array.
 *
 * @param self   Pointer to the CACHE_RULES object that is being built.
 * @param object An object from the "store" array.
 * @param index  Index of the object in the array.
 *
 * @return True, if the object could be parsed, false otherwise.
 */
static bool cache_rules_parse_store_element(CACHE_RULES* self, json_t* object, size_t index)
{
    CacheRule* rule = cache_rules_parse_element(self, object, KEY_STORE, index, cache_store_attributes);

    if (rule)
    {
        cache_rules_add_store_rule(self, rule);
    }

    return rule != nullptr;
}

/**
 * Parses an object in the "use" array.
 *
 * @param self   Pointer to the CACHE_RULES object that is being built.
 * @param object An object from the "store" array.
 * @param index  Index of the object in the array.
 *
 * @return True, if the object could be parsed, false otherwise.
 */
static bool cache_rules_parse_use_element(CACHE_RULES* self, json_t* object, size_t index)
{
    CacheRule* rule = cache_rules_parse_element(self, object, KEY_USE, index, cache_use_attributes);

    if (rule)
    {
        cache_rules_add_use_rule(self, rule);
    }

    return rule != nullptr;
}
