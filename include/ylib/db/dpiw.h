#pragma once

#include <cstring>
#include <optional>
#include <functional>

#include <dpi.h>
#include <ylib/core/lang.h>
#include <ylib/logging/Logger.h>

using ylib::logging::Logger;
using namespace ylib::core;


namespace ylib {
    namespace db {
        namespace dpiw {

            static Logger log = Logger::get("ylib::db::odpi");

            class DBException : public Exception {
            private:
                const string _funcName;
                const string _action;
                const string _message;

            public:
                DBException(const string &message) : Exception(message), _message{message} {

                }

                DBException(const string &funcName,
                            const string &action,
                            const string &message) :
                        _funcName{funcName},
                        _action{action},
                        _message{message} {

                    //_msg is a parent protected field
                    _msg = "ODPI Error: " + _message;
                }

                static DBException build(dpiContext *ctx, dpiErrorInfo *err) {

                    if (ctx) {
                        dpiContext_getError(ctx, err);
                    }

                    // The only way dpiContext *ctx can be null, is when an error occurs during context creation.
                    // In this particular case, the dpiErrorInfo *err will be already populated with these 4 fields: message,
                    // messageLength, encoding, fnName and action.

                    /* https://oracle.github.io/odpi/doc/functions/dpiContext.html#c.dpiContext_createWithParams
                     * According to ODPI documentation:
                     * Note that the only members of the structure that should be examined when an error occurs are message,
                     * messageLength, encoding, fnName and action.
                     */

                    string __fn = err->fnName;
                    string __ac = err->action;
                    string __tx{err->message, err->messageLength};

                    DBException ex{__fn, __ac, __tx};
                    return ex;
                }

                static DBException build(dpiContext *ctx) {
                    dpiErrorInfo err;
                    return build(ctx, &err);
                }

                string funcName() {
                    return _funcName;
                }

                string action() {
                    return _action;
                }

                string msg() {
                    return _message;
                }
            };

            class ResultSet {
            private:
                dpiContext *_ctx = nullptr; //not owned
                dpiStmt *_stmt = nullptr; //not owned
                Bool _found{False};

                //column based
                //---------------------------------------------
                dpiData *_data = nullptr; //not owned
                unsigned int _col = 0;
                UInt32 _columnCount = 0;
                dpiNativeTypeNum _nativeTypeNum;
                //---------------------------------------------


                double dataToDouble() {
                    if (_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE) {
                        return _data->value.asDouble;
                    }

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_FLOAT) {
                        return _data->value.asFloat;
                    }

                    throw Exception(sfput("Could not convert column index {} to double. "
                                          "The dpiNativeTypeNum is: {}.", _col, _nativeTypeNum));
                }

                Int64 dataToInt64() {

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_INT64) {
                        return _data->value.asInt64;
                    }

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE) {
                        return (Int64) _data->value.asDouble;
                    }

                    throw Exception(sfput("Could not convert column index {} to Int64. "
                                          "The dpiNativeTypeNum is: {}.", _col, _nativeTypeNum));
                }

                UInt64 dataToUInt64() {

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_UINT64) {
                        return _data->value.asUint64;
                    }

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE) {
                        return (UInt64) _data->value.asDouble;
                    }

                    if(_nativeTypeNum == DPI_NATIVE_TYPE_INT64){
                        Int64 val = _data->value.asInt64;
                        if(val < 0){
                            throw Exception(sfput("Could not convert column index {} to UInt64, "
                                                  "negative value: {}. The dpiNativeTypeNum is: {}.", _col,
                                                  val, _nativeTypeNum));
                        }
                        return (UInt64)val;
                    }

                    throw Exception(sfput("Could not convert column index {} to UInt64. "
                                          "The dpiNativeTypeNum is: {}.", _col, _nativeTypeNum));
                }

                dpiTimestamp dataToTimestamp() {

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_TIMESTAMP) {
                        return _data->value.asTimestamp;
                    }

                    throw Exception(sfput("Could not convert column index ${} to dpiTimestamp. "
                                          "The dpiNativeTypeNum is: ${}.", _col, _nativeTypeNum));
                }

                string dataToString() {
                    if (_nativeTypeNum == DPI_NATIVE_TYPE_BYTES) {
                        dpiBytes val = _data->value.asBytes;
                        UInt32 len = val.length;
                        char *ptr = val.ptr;
                        string ans{ptr, len};
                        return ans;
                    }

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE) {
                        return std::to_string(dataToDouble());
                    }

                    if (_nativeTypeNum == DPI_NATIVE_TYPE_INT64) {
                        return std::to_string(dataToInt64());
                    }

                    throw Exception(sfput("Could not convert column index ${} to std::string. "
                                          "The dpiNativeTypeNum is: ${}.", _col, _nativeTypeNum));
                }

                void fetchCol(unsigned int col) {
                    checkParamIsPositive("col", col);

                    if (_found == False) {
                        throw DBException("Can not fetch column, no rows currently available.");
                    }

                    /*
                     * https://oracle.github.io/odpi/doc/functions/dpiStmt.html#c.dpiStmt_getQueryValue
                     * From Oracle ODPI documentation:
                     * data [OUT] – a pointer to a reference to an internally created dpiData structure which will be populated
                     * upon successful completion of this function. The structure contains the value of the column at the specified
                     * position. Note that any references to LOBs, statements, objects and rowids are owned by the statement.
                     *
                     * That means, that we don't own the _data pointer, and should not free it ourselves.
                     */
                    _col = col;
                    if (dpiStmt_getQueryValue(_stmt, col, &_nativeTypeNum, &_data) < 0) {
                        throw DBException::build(_ctx);
                    }
                }

                tm toTimeGMT(dpiTimestamp& timestamp){
                    tm t = ctimeGMT();
                    t.tm_year = timestamp.year - 1900; //tm year is since 1900
                    t.tm_mon = timestamp.month - 1; //tm month is [0, 11]
                    t.tm_mday = timestamp.day;

                    time_t tt = timegm(&t) +
                                (timestamp.tzHourOffset * 60 * 60) +
                                (timestamp.tzMinuteOffset * 60);

                    return *gmtime(&tt);
                }

            public:
                ResultSet(dpiContext *ctx, dpiStmt *stmt) {
                    _ctx = ctx;
                    _stmt = stmt;
                    if (dpiStmt_execute(_stmt, DPI_MODE_EXEC_DEFAULT, &_columnCount) < 0) {
                        DBException ex = DBException::build(_ctx);
                        throw ex;
                    }
                }

                // Rule of five
                // =========================================================================
                // 1. Copy Constructor
                // No copy constructor allowed
                ResultSet(const ResultSet &) = delete;

                // 2. Copy Assignment
                // No copy assignment allowed
                ResultSet &operator=(const ResultSet &other) = delete;

                // 3. Move Constructor
                // Allowed

                // 4. Move Assignment
                // Allowed

                // 5. Destructor
                // Implemented
                // =========================================================================

                UInt32 columnCount() {
                    return _columnCount;
                }

                Bool next() {

                    int found; //boolean
                    uint32_t bufferRowIndex; //not used
                    if (dpiStmt_fetch(_stmt, &found, &bufferRowIndex) < 0) {
                        DBException ex = DBException::build(_ctx);
                        throw ex;
                    }

                    if (found == 0) {
                        _found = False;
                        return False;
                    }

                    _found = True;
                    return True;
                }


                string getString(unsigned int col) {
                    fetchCol(col);
                    return dataToString();
                }

                optional<string> getStringOpt(unsigned int col) {
                    fetchCol(col);
                    if (dpiData_getIsNull(_data) == 1) {
                        return std::nullopt;
                    }
                    return dataToString();
                }

                Int64 getInt64(unsigned int col) {
                    fetchCol(col);
                    return dataToInt64();
                }

                Int64 getUInt64(unsigned int col) {
                    fetchCol(col);
                    return dataToUInt64();
                }

                Int32 getInt32(unsigned int col) {
                    Int64 val = getInt64(col);
                    if (val > std::numeric_limits<Int32>::max() ||
                        val < std::numeric_limits<Int32>::lowest()) {
                        throw Exception(sfput("The value for colum ${}, is outside of the Int32 limits.", col));
                    }
                    return (Int32) val;
                }

                Date getDate(unsigned int col) {
                    fetchCol(col);
                    dpiTimestamp timestamp = dataToTimestamp();

                    return Date(toTimeGMT(timestamp));
                }


                DateTime getDateTime(unsigned int col) {
                    fetchCol(col);
                    dpiTimestamp timestamp = dataToTimestamp();

                    tm t2 = toTimeGMT(timestamp);
                    UInt16 millis = (UInt16)(timestamp.fsecond / 1'000'000.0);

                    Date date{t2};
                    Time time{t2, millis};

                    return DateTime(date, time);
                }

                void forEach(std::function<void(ResultSet &)> f) {
                    while (next() == True) {
                        f(*this);
                    }
                }
            };

            class DBStatement {
            private:

                dpiContext *_ctx = nullptr; //not owned
                dpiConn *_conn = nullptr; //not owned
                dpiStmt *_stmt = nullptr;


                void bindByPos(unsigned int col, dpiNativeTypeNum nativeTypeNum, dpiData &data) {
                    if (dpiStmt_bindValueByPos(_stmt, col, nativeTypeNum, &data) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }

                void bindByName(const char *param, dpiNativeTypeNum nativeTypeNum, dpiData &data) {
                    size_t len = strlen(param);
                    if (dpiStmt_bindValueByName(_stmt, param, len, nativeTypeNum, &data) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }

            public:
                DBStatement(dpiContext *ctx, dpiConn *conn, const char *sql) {
                    _ctx = ctx;
                    _conn = conn;
                    if (dpiConn_prepareStmt(_conn, 0, sql, strlen(sql), NULL, 0, &_stmt) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }

                // Rule of five
                // =========================================================================
                // 1. Copy Constructor
                // No copy constructor allowed
                DBStatement(const DBStatement &) = delete;

                // 2. Copy Assignment
                // No copy assignment allowed
                DBStatement &operator=(const DBStatement &other) = delete;

                // 3. Move Constructor
                // Allowed

                // 4. Move Assignment
                // Allowed

                // 5. Destructor
                // Implemented
                // =========================================================================

                void setNull(unsigned int col, dpiNativeTypeNum typeNum) {
                    checkParamIsPositive("col", col);

                    dpiData data;
                    dpiData_setNull(&data);

                    bindByPos(col, typeNum, data);
                }

                void setNull(const char *param, dpiNativeTypeNum typeNum) {

                    dpiData data;
                    dpiData_setNull(&data);

                    bindByName(param, typeNum, data);
                }

                void setNullString(const char *param) {
                    setNull(param, DPI_NATIVE_TYPE_BYTES);
                }

                void setString(unsigned int col, const string &val) {
                    checkParamIsPositive("col", col);

                    const char *str = val.c_str();
                    size_t valLen = val.length();

                    dpiData data;
                    dpiData_setBytes(&data, (char *) str, valLen);

                    bindByPos(col, DPI_NATIVE_TYPE_BYTES, data);
                }

                void setString(const char *param, const string &val) {

                    size_t len = strlen(param);

                    const char *str = val.c_str();
                    size_t valLen = val.length();

                    dpiData data;
                    dpiData_setBytes(&data, (char *) str, valLen);

                    bindByName(param, DPI_NATIVE_TYPE_BYTES, data);
                }

                void setString(unsigned int col, const char *val) {
                    string sval = val;
                    setString(col, sval);
                }

                void setStringOpt(unsigned int col, std::optional<string> opt) {
                    if (opt.has_value()) {
                        setString(col, opt.value());
                    } else {
                        setNull(col, DPI_NATIVE_TYPE_BYTES);
                    }
                }

                void setStringOpt(const char *param, std::optional<string> opt) {
                    if (opt.has_value()) {
                        setString(param, opt.value());
                    } else {
                        setNull(param, DPI_NATIVE_TYPE_BYTES);
                    }
                }

                void setInt64(unsigned int col, Int64 val) {
                    checkParamIsPositive("col", col);

                    dpiData data;
                    dpiData_setInt64(&data, (int64_t) val);

                    bindByPos(col, DPI_NATIVE_TYPE_INT64, data);
                }

                void setInt64(const char *param, Int64 val) {

                    dpiData data;
                    dpiData_setInt64(&data, (int64_t) val);

                    bindByName(param, DPI_NATIVE_TYPE_INT64, data);
                }

                void setUInt64(const char *param, UInt64 val) {

                    dpiData data;
                    dpiData_setUint64(&data,  val);

                    bindByName(param, DPI_NATIVE_TYPE_UINT64, data);
                }

                void setInt64Opt(unsigned int col, std::optional<Int64> opt) {
                    if (opt.has_value()) {
                        setInt64(col, opt.value());
                    } else {
                        setNull(col, DPI_NATIVE_TYPE_INT64);
                    }
                }

                void setInt64Opt(const char *param, std::optional<Int64> opt) {
                    if (opt.has_value()) {
                        setInt64(param, opt.value());
                    } else {
                        setNull(param, DPI_NATIVE_TYPE_INT64);
                    }
                }


                void setDouble(unsigned int col, double val) {

                    dpiData data;
                    dpiData_setDouble(&data, (double) val);

                    bindByPos(col, DPI_NATIVE_TYPE_DOUBLE, data);
                }

                void setDouble(const char *param, double val) {

                    dpiData data;
                    dpiData_setDouble(&data, (double) val);

                    bindByName(param, DPI_NATIVE_TYPE_DOUBLE, data);
                }

                void setDoubleOpt(unsigned int col, std::optional<double> opt) {
                    if (opt.has_value()) {
                        setDouble(col, opt.value());
                    } else {
                        setNull(col, DPI_NATIVE_TYPE_DOUBLE);
                    }
                }

                void setDoubleOpt(const char *param, std::optional<double> opt) {
                    if (opt.has_value()) {
                        setDouble(param, opt.value());
                    } else {
                        setNull(param, DPI_NATIVE_TYPE_DOUBLE);
                    }
                }

                void setBool(unsigned int col, Bool val) {
                    dpiData data;
                    if (val == True) {
                        dpiData_setBool(&data, true);
                    } else {
                        dpiData_setBool(&data, false);
                    }

                    bindByPos(col, DPI_NATIVE_TYPE_BOOLEAN, data);
                }

                void setDateTime(const char *param, const core::DateTime val) {
                    dpiData data;
                    Date date = val.date();
                    Time time = val.time();
                    Int32 fractions = time.milli();
                    fractions = fractions * ((Int32) 1000000);
                    dpiData_setTimestamp(&data, date.year(),
                                         monthToUInt(date.month()),
                                         date.day(),
                                         time.hour(),
                                         time.min(),
                                         time.sec(),
                                         fractions,
                                         0, 0);
                    bindByName(param, DPI_NATIVE_TYPE_TIMESTAMP, data);
                }

                void setDateTime(unsigned int col, const core::DateTime val) {
                    dpiData data;
                    Date date = val.date();
                    Time time = val.time();
                    Int32 fractions = time.milli();
                    fractions = fractions * ((Int32) 1000000);
                    dpiData_setTimestamp(&data, date.year(),
                                         monthToUInt(date.month()),
                                         date.day(),
                                         time.hour(),
                                         time.min(),
                                         time.sec(),
                                         fractions,
                                         0, 0);
                    bindByPos(col, DPI_NATIVE_TYPE_TIMESTAMP, data);
                }

                void setDateTimeOpt(const char *param, const optional<core::DateTime> val) {
                    if (val.has_value()) {
                        setDateTime(param, val.value());
                    } else {
                        setNull(param, DPI_NATIVE_TYPE_TIMESTAMP);
                    }
                }

                void setDateTimeOpt(unsigned int col, const optional<core::DateTime> val) {
                    if (val.has_value()) {
                        setDateTime(col, val.value());
                    } else {
                        setNull(col, DPI_NATIVE_TYPE_TIMESTAMP);
                    }
                }

                void setDate(const char *param, const core::Date date) {
                    dpiData data;

                    dpiData_setTimestamp(&data, date.year(),
                                         monthToUInt(date.month()),
                                         date.day(),
                                         0,
                                         0,
                                         0,
                                         0,
                                         0, 0);
                    bindByName(param, DPI_NATIVE_TYPE_TIMESTAMP, data);
                }

                void exec() {
                    if (dpiStmt_execute(_stmt, DPI_MODE_EXEC_DEFAULT, NULL) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }


                UInt64 getRowCount() {
                    uint64_t count;

                    // According to ODPI documentation:
                    // Returns the number of rows affected by the last DML statement that was executed, the number of
                    // rows currently fetched from a query, or the number of successful executions of a PL/SQL block.
                    // In all other cases 0 is returned.
                    //
                    // https://oracle.github.io/odpi/doc/functions/dpiStmt.html
                    if (dpiStmt_getRowCount(_stmt, &count) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                    return count;
                }

                ResultSet execQuery() {
                    return {_ctx, _stmt};
                }

                UInt64 execCount() {
                    auto rs = execQuery();
                    if (rs.next() == False) {
                        throw DBException("Not a count query. ResultSet is empty.");
                    }

                    return rs.getUInt64(1);
                }

                string getLastRowId() {

                    /*
                     * According to Oracle documentation:
                     * If a rowid is returned, the reference will remain valid until the next call to this function or until the
                     * statement is closed.
                     *
                     * That means, that we don't own the dpiRowid pointer.
                     */

                    dpiRowid *id; //not owning

                    if (dpiStmt_getLastRowid(_stmt, &id) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }

                    const char *base64; //not owning
                    uint32_t len;
                    if (dpiRowid_getStringValue(id, &base64, &len) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }

                    string ans{base64, len};
                    return ans;
                }

                virtual ~DBStatement() {
                    try {
                        if (_stmt) {
                            dpiStmt_release(_stmt);
                        }
                    } catch (std::exception &ex) {
                        log.error(ex);
                    }
                }
            };

            class DBConnection {
            private:
                dpiContext *_ctx = nullptr; //not owned
                dpiConn *_conn = nullptr;

            public:

                DBConnection(dpiContext *ctx, const char *user, const char *pass, const char *connStr) {
                    _ctx = ctx;
                    if (dpiConn_create(
                            _ctx,
                            user, strlen(user),
                            pass, strlen(pass),
                            connStr, strlen(connStr),
                            NULL, NULL, &_conn) == DPI_FAILURE) {
                        throw DBException::build(ctx);
                    }
                }

                DBConnection(dpiContext *ctx,
                             const string &user,
                             const string &pass,
                             const string &connStr) : DBConnection(ctx,
                                                                   user.c_str(),
                                                                   pass.c_str(),
                                                                   connStr.c_str()) {

                }

                // Rule of five
                // =========================================================================
                // 1. Copy Constructor
                // No copy constructor allowed
                DBConnection(const DBConnection &) = delete;

                // 2. Copy Assignment
                // No copy assignment allowed
                DBConnection &operator=(const DBConnection &other) = delete;

                // 3. Move Constructor
                // Allowed

                // 4. Move Assignment
                // Allowed

                // 5. Destructor
                // Implemented
                // =========================================================================

                DBStatement statement(const char *sql) {
                    return {_ctx, _conn, sql};
                }

                DBStatement statement(string sql) {
                    return {_ctx, _conn, sql.c_str()};
                }


                void commit() {
                    // commit changes
                    if (dpiConn_commit(_conn) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }

                void rollack() {
                    // rollback changes
                    if (dpiConn_rollback(_conn) == DPI_FAILURE) {
                        throw DBException::build(_ctx);
                    }
                }


                template<typename F>
                auto transaction(F f) {
                    try {
                        DBConnection &conn = *this;
                        auto ans = f();
                        conn.commit();
                        return ans;
                    } catch (...) {
                        rollack();
                        throw;
                    }
                }

                virtual ~DBConnection() {
                    try {
                        if (_conn) {
                            dpiConn_release(_conn);
                        }
                    } catch (std::exception &ex) {
                        log.error(ex);
                    }
                }
            };


            class DBEnvironment {
            private:
                dpiContext *_ctx = nullptr;


            public:
                DBEnvironment() {
                    dpiErrorInfo err;
                    if (dpiContext_createWithParams(DPI_MAJOR_VERSION,
                                                    DPI_MINOR_VERSION,
                                                    NULL, &_ctx, &err) == DPI_FAILURE) {


                        throw DBException::build(_ctx, &err);
                    }
                }

                // Rule of five
                // =========================================================================
                // 1. Copy Constructor
                // No copy constructor allowed
                DBEnvironment(const DBConnection &) = delete;

                // 2. Copy Assignment
                // No copy assignment allowed
                DBEnvironment &operator=(const DBEnvironment &other) = delete;

                // 3. Move Constructor
                // Allowed

                // 4. Move Assignment
                // Allowed

                // 5. Destructor
                // Implemented
                // =========================================================================

                DBConnection connect(string user, string pass, string connStr) {
                    return {_ctx, user, pass, connStr};
                }

                DBConnection connect(const char *user, const char *pass, const char *connStr) {
                    return {_ctx, user, pass, connStr};
                }

                virtual ~DBEnvironment() {
                    try {
                        if (_ctx) {
                            dpiContext_destroy(_ctx);
                        }
                    } catch (std::exception &ex) {
                        log.error(ex);
                    }
                }
            };


            UInt64 getLastInsertedPKInt64(DBConnection &conn, DBStatement &stmt, const char* table, const char* col) {

                string rowId = stmt.getLastRowId(); //Oracle's ROWID value, not PK

                stringstream ss;
                ss << "select ";
                ss << col;
                ss << " from ";
                ss << table;
                ss << " where rowid = :1";
                string sql = ss.str();
                auto idstmt = conn.statement(sql);

                idstmt.setString(1, rowId);
                auto rs = idstmt.execQuery();

                if (rs.next() == True) {
                    return rs.getInt64(1);
                }

                throw Exception("Could not get the PK of the last inserted record.");
            }
        }
    }
}