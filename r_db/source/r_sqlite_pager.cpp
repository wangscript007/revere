
#include "r_db/r_sqlite_pager.h"
#include "r_utils/r_string.h"
#include "r_utils/r_exception.h"
#include <algorithm>

using namespace r_db;
using namespace r_utils;
using namespace std;

r_sqlite_pager::r_sqlite_pager(const string& columnsToInclude,
                               const string& tableName,
                               const string& indexColumn,
                               uint32_t requestedPageSize) :
    _columnsToInclude(columnsToInclude),
    _tableName(tableName),
    _indexColumn(indexColumn),
    _requestedPageSize(requestedPageSize),
    _index(0),
    _results()
{
}

void r_sqlite_pager::find(const r_sqlite_conn& conn, const std::string& val)
{
    // XXX if val is empty(), assume they want to start from the beginning
    _results = conn.exec(r_string::format("SELECT %s FROM %s %s ORDER BY %s ASC LIMIT %s;",
                                          _columnsToInclude.c_str(),
                                          _tableName.c_str(),
                                          (!val.empty())?r_string::format("WHERE %s >= %s", _indexColumn.c_str(), val.c_str()).c_str():"",
                                          _indexColumn.c_str(),
                                          r_string::uint32_to_s(_requestedPageSize).c_str()));
    _index = 0;
}

map<string, string> r_sqlite_pager::current() const
{
    if(_index >= _results.size())
        R_STHROW(r_not_found_exception, ("Invalid current position in r_sqlite_pager!"));

    return _results[_index];
}

void r_sqlite_pager::next(const r_sqlite_conn& conn)
{
    if((_index + 1) >= _results.size())
    {
        _results = conn.exec(r_string::format("SELECT %s FROM %s WHERE %s > %s ORDER BY %s ASC LIMIT %s;",
                                              _columnsToInclude.c_str(),
                                              _tableName.c_str(),
                                              _indexColumn.c_str(),
                                              _results.back()[_indexColumn].c_str(),
                                              _indexColumn.c_str(),
                                              r_string::uint32_to_s(_requestedPageSize).c_str()));
        _index = 0;
    }
    else ++_index;
}

void r_sqlite_pager::prev(const r_sqlite_conn& conn)
{
    if(_results.empty())
        R_STHROW(r_not_found_exception, ("Unable to prev() before empty page."));

    if(_index == 0)
    {
        _results = conn.exec(r_string::format("SELECT %s FROM %s WHERE %s < %s ORDER BY %s DESC LIMIT %s;",
                                              _columnsToInclude.c_str(),
                                              _tableName.c_str(),
                                              _indexColumn.c_str(),
                                              _results.front()[_indexColumn].c_str(),
                                              _indexColumn.c_str(),
                                              r_string::uint32_to_s(_requestedPageSize).c_str()));

        reverse(begin(_results), end(_results));

        _index = (_results.size() - 1);
    }
    else --_index;
}

bool r_sqlite_pager::valid() const
{
    return !_results.empty();
}