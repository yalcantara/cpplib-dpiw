#pragma once

#include <cstring>
#include <optional>
#include <functional>

#include <dpi.h>
#include <ylib/core/lang.h>
#include <ylib/logging/Logger.h>

using ylib::logging::Logger;
using namespace ylib::core;


namespace ylib{
namespace db{
namespace dpiw{

static Logger log = Logger::get( "ylib::db::odpi" );
    
class DBException : public Exception{
private:
    const string _funcName;
    const string _action;
    const string _message;

public:
    DBException(const string& message):Exception(message), _message{message}{

    }

    DBException(const string& funcName, 
                const string& action, 
                const string& message):
            _funcName{funcName},
            _action{action},
            _message{message}{

        //_msg is a parent protected field
        _msg = "ODPI Error: " + _message;
    }

    static DBException build(dpiContext* ctx, dpiErrorInfo* err){
        
        if(ctx){
            dpiContext_getError(ctx, err);
        }
        
        string __fn = err->fnName;
        string __ac = err->action;
        string __tx {err->message, err->messageLength};
    
        DBException ex{__fn, __ac, __tx};
        return ex;
    }

    static DBException build(dpiContext* ctx){
        dpiErrorInfo err;
        return build(ctx, &err);
    }

    string funcName(){
        return _funcName;
    }

    string action(){
        return _action;
    }

    string msg(){
        return _message;
    }
};



class ResultSet{
private:
    dpiContext* _ctx = nullptr; //not owned
    dpiStmt* _stmt = nullptr; //not owned
    Bool _found{False};

    //column based 
    //---------------------------------------------
    dpiData* _data = nullptr; //not owned
    unsigned int _col = 0;
    dpiNativeTypeNum _nativeTypeNum;
    //---------------------------------------------


    double dataToDouble(){
        if(_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE){
            double val = _data->value.asDouble;
            return (double)val;
        }

        if(_nativeTypeNum == DPI_NATIVE_TYPE_FLOAT){
            float val = _data->value.asFloat;
            return (double)val;
        }
    }

    Int64 dataToInt64() {

        if(_nativeTypeNum == DPI_NATIVE_TYPE_INT64){
            int64_t val = _data->value.asInt64;
            return (Int64)val;
        }

        if(_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE){
            double val = _data->value.asDouble;
            return (Int64)val;
        }
        
        throw Exception(sfput(  "Could not convert column index ${} to Int32. "
                                "The dpiNativeTypeNum is: ${}.", _col, _nativeTypeNum));
    }

    dpiTimestamp dataToTimestamp() {

        if(_nativeTypeNum == DPI_NATIVE_TYPE_TIMESTAMP){
            dpiTimestamp val = _data->value.asTimestamp;
            return (dpiTimestamp)val;
        }
        
        throw Exception(sfput(  "Could not convert column index ${} to dpiTimestamp. "
                                "The dpiNativeTypeNum is: ${}.", _col, _nativeTypeNum));
    }

    string dataToString() {
        if(_nativeTypeNum == DPI_NATIVE_TYPE_BYTES){
            dpiBytes val = _data->value.asBytes;
            UInt32 len = val.length;
            char* ptr = val.ptr;
            string ans{ ptr, len };
            return ans;
        }

        if(_nativeTypeNum == DPI_NATIVE_TYPE_DOUBLE){
            double val = dataToDouble();
            return std::to_string(val);
        }
    }

    void fetchCol(unsigned int col){
        checkParamIsPositive("col", col);

        if(_found == False){
            throw DBException("Can not fetch column, no rows currently available.");
        }
        _col = col;
        if (dpiStmt_getQueryValue(_stmt, col, &_nativeTypeNum, &_data) < 0){
            DBException ex = DBException::build(_ctx);
            throw ex;
        }
    }
    
public:
    ResultSet(dpiContext* ctx, dpiStmt* stmt){
        _ctx = ctx;
        _stmt = stmt;
        if (dpiStmt_execute(_stmt, DPI_MODE_EXEC_DEFAULT, NULL) < 0){
            DBException ex = DBException::build(_ctx);
            throw ex;
        }
    }

    Bool next(){

        int found; //boolean
        uint32_t bufferRowIndex; //not used
        if(dpiStmt_fetch(_stmt, &found, &bufferRowIndex) < 0){
            DBException ex = DBException::build(_ctx);
            throw ex;
        }

        if(found == 0){
            _found = False;
            return False;
        }

        _found = True;
        return True;
    }

    

    string getString(unsigned int col){
        fetchCol(col);
        return dataToString();
    }

    optional<string> getStringOpt(unsigned int col){
        fetchCol(col);
        if(dpiData_getIsNull(_data) == 1){
            return std::nullopt;
        }
        return dataToString();
    }

    Int64 getInt64(unsigned int col){
        fetchCol(col);
        return dataToInt64();
    }

    Int32 getInt32(unsigned int col){
        return (Int32)getInt64(col);
    }

    Date getDate(unsigned int col){
        fetchCol(col);
        dpiTimestamp timestamp = dataToTimestamp();

        tm t = ctimeGMT();
        t.tm_year   = timestamp.year - 1900;
        t.tm_mon    = timestamp.month - 1;
        t.tm_mday   = timestamp.day;

        time_t tt = timegm(&t) + 
                    (timestamp.tzHourOffset     * 60 * 60) + 
                    (timestamp.tzMinuteOffset   * 60);

        tm t2 = *gmtime(&tt);

        return Date(t2);
    }
};



class DBStatement {
private:

    dpiContext* _ctx = nullptr; //not owned
    dpiConn*    _conn = nullptr; //not owned
    dpiStmt*    _stmt = nullptr;


    void bindByPos(unsigned int col, dpiNativeTypeNum nativeTypeNum, dpiData& data){
        int res = DPI_FAILURE; //let's asume failure as default
        res = dpiStmt_bindValueByPos(_stmt, col, nativeTypeNum, &data);
        if (res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
    }

    void bindByName(const char* param, dpiNativeTypeNum nativeTypeNum, dpiData& data){
        size_t len = strlen(param);

        int res = DPI_FAILURE; //let's asume failure as default
        res = dpiStmt_bindValueByName(_stmt, param, len, nativeTypeNum, &data);
        if (res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
    }

public:
    DBStatement(dpiContext* ctx, dpiConn* conn, const char* sql){
        _ctx = ctx;
        _conn = conn;
        if (dpiConn_prepareStmt(_conn, 0, sql, strlen(sql), NULL, 0, &_stmt) == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
    }

    void setNull(unsigned int col, dpiNativeTypeNum typeNum){
        checkParamIsPositive("col", col);

        dpiData data;
        dpiData_setNull(&data);
    
        bindByPos(col, typeNum, data);
    }

    void setNull(const char* param, dpiNativeTypeNum typeNum){

        dpiData data;
        dpiData_setNull(&data);
       
        bindByName(param, typeNum, data);
    }

    void setNullString(const char* param){
        setNull(param, DPI_NATIVE_TYPE_BYTES);
    }

    void setString(unsigned int col, const string& val){
        checkParamIsPositive("col", col);

        const char* str = val.c_str();
        size_t valLen = val.length();

        dpiData data;
        dpiData_setBytes(&data, (char*)str, valLen);
       
        bindByPos(col, DPI_NATIVE_TYPE_BYTES, data);
    }

    void setString(const char* param, const string& val){

        size_t len = strlen(param);

        const char* str = val.c_str();
        size_t valLen = val.length();

        dpiData data;
        dpiData_setBytes(&data, (char*)str, valLen);
        
        bindByName(param, DPI_NATIVE_TYPE_BYTES, data);
    }

    void setString(unsigned int col, const char* val){
        string sval = val;
        setString(col, sval);
    }

    void setStringOpt(unsigned int col, std::optional<string> opt){
        if(opt.has_value()){
            setString(col, opt.value());
        } else {
            setNull(col, DPI_NATIVE_TYPE_BYTES);
        }
    }

    void setStringOpt(const char* param, std::optional<string> opt){
        if(opt.has_value()){
            setString(param, opt.value());
        } else {
            setNull(param, DPI_NATIVE_TYPE_BYTES);
        }
    }

    void setInt64(unsigned int col, Int64 val){
        checkParamIsPositive("col", col);

        dpiData data;
        dpiData_setInt64(&data, (int64_t)val);

        bindByPos(col, DPI_NATIVE_TYPE_INT64, data);
    }

    void setInt64(const char* param, Int64 val){

        dpiData data;
        dpiData_setInt64(&data, (int64_t)val);

        bindByName(param, DPI_NATIVE_TYPE_INT64, data);
    }

    void setInt64Opt(unsigned int col, std::optional<Int64> opt){
        if(opt.has_value()){
            setInt64(col, opt.value());
        } else {
            setNull(col, DPI_NATIVE_TYPE_INT64);
        }
    }

    void setInt64Opt(const char* param, std::optional<Int64> opt){
        if(opt.has_value()){
            setInt64(param, opt.value());
        } else {
            setNull(param, DPI_NATIVE_TYPE_INT64);
        }
    }


    void setDouble(unsigned int col, double val){

        dpiData data;
        dpiData_setDouble(&data, (double)val);

        bindByPos(col, DPI_NATIVE_TYPE_DOUBLE, data);
    }

    void setDouble(const char* param, double val){

        dpiData data;
        dpiData_setDouble(&data, (double)val);

        bindByName(param, DPI_NATIVE_TYPE_DOUBLE, data);
    }

    void setDoubleOpt(unsigned int col, std::optional<double> opt){
        if(opt.has_value()){
            setDouble(col, opt.value());
        } else {
            setNull(col, DPI_NATIVE_TYPE_DOUBLE);
        }
    }

    void setDoubleOpt(const char* param, std::optional<double> opt){
        if(opt.has_value()){
            setDouble(param, opt.value());
        } else {
            setNull(param, DPI_NATIVE_TYPE_DOUBLE);
        }
    }

    void setBool(unsigned int col, Bool val){
        dpiData data;
        if(val == True){
            dpiData_setBool(&data, true);
        } else {
            dpiData_setBool(&data, false);
        }

        bindByPos(col, DPI_NATIVE_TYPE_BOOLEAN, data);
    }

    void setDateTime(const char* param, const core::DateTime val){
        dpiData data;
        Date date = val.date();
        Time time = val.time();
        Int32 fractions =  time.milli();
        fractions = fractions * ((Int32)1000000);
        dpiData_setTimestamp(&data, date.year(), 
                                    date.month(), 
                                    date.day(), 
                                    time.hour(), 
                                    time.min(), 
                                    time.sec(), 
                                    fractions,
                                    0, 0);
        bindByName(param, DPI_NATIVE_TYPE_TIMESTAMP, data);
    }

    void setDateTime(unsigned int col, const core::DateTime val){
        dpiData data;
        Date date = val.date();
        Time time = val.time();
        Int32 fractions =  time.milli();
        fractions = fractions * ((Int32)1000000);
        dpiData_setTimestamp(&data, date.year(), 
                                    date.month(), 
                                    date.day(), 
                                    time.hour(), 
                                    time.min(), 
                                    time.sec(), 
                                    fractions,
                                    0, 0);
        bindByPos(col, DPI_NATIVE_TYPE_TIMESTAMP, data);
    }

    void setDateTimeOpt(const char* param, const optional<core::DateTime> val){
        if(val.has_value()){
            setDateTime(param, val.value());
        }else{
            setNull(param, DPI_NATIVE_TYPE_TIMESTAMP);
        }
    }

    void setDateTimeOpt(unsigned int col, const optional<core::DateTime> val){
        if(val.has_value()){
            setDateTime(col, val.value());
        }else{
            setNull(col, DPI_NATIVE_TYPE_TIMESTAMP);
        }
    }

    void setDate(const char* param, const core::Date& date){
        dpiData data;

        dpiData_setTimestamp(&data, date.year(), 
                                    date.month(), 
                                    date.day(), 
                                    0, 
                                    0, 
                                    0, 
                                    0,
                                    0, 0);
        bindByName(param, DPI_NATIVE_TYPE_TIMESTAMP, data);
    }

    void exec(){
        int res = DPI_FAILURE; //let's asume failure as default
        //execute
        res = dpiStmt_execute(_stmt, DPI_MODE_EXEC_DEFAULT, NULL);
        if (res == DPI_FAILURE){
            throw DBException::build(_ctx);
        } 
    }
    

    Int64 getRowCount(){
        uint64_t count;
        int res = DPI_FAILURE;
        res = dpiStmt_getRowCount(_stmt, &count);
        if(res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
        return (Int64)count;
    }

    ResultSet execQuery(){
        ResultSet rs{_ctx, _stmt};
        return rs;
    }

    Int64 execCount(){
        ResultSet rs = execQuery();
        if(rs.next() == False){
            throw DBException("Not a count query. ResultSet is empty.");
        }

        return rs.getInt64(1);
    }

    string getLastRowId(){
        dpiRowid* id;
        int res = DPI_FAILURE;
        res = dpiStmt_getLastRowid(_stmt, &id);
        if(res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
        
        const char* base64;
        uint32_t len;
        res = dpiRowid_getStringValue(id, &base64, &len);
        if(res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }

        string ans{base64, len};
        return ans;
    }

    

    virtual ~DBStatement(){
        try{
            if(_stmt){
                dpiStmt_release(_stmt);
            }
        }catch(std::exception& ex){
            log.error(ex);
        }
    }
};


class DBConnection{
private:
    dpiContext* _ctx = nullptr; //not owned
    dpiConn* _conn = nullptr;

public:

    DBConnection(dpiContext* ctx, const char* user, const char* pass, const char* connStr){
        _ctx = ctx;
        if (dpiConn_create(
                _ctx, 
                user, strlen(user), 
                pass, strlen(pass),
                connStr, strlen(connStr), 
                NULL, NULL, &_conn) < 0){
            throw DBException::build(ctx);
        }
    }

    DBConnection(   dpiContext* ctx, 
                    const string& user, 
                    const string& pass, 
                    const string& connStr): DBConnection(   ctx, 
                                user.c_str(), 
                                pass.c_str(), 
                                connStr.c_str()){

    }

    DBStatement statement(const char* sql){
        DBStatement stmt{_ctx, _conn, sql};
        return stmt;
    }

    DBStatement statement(string sql){
        DBStatement stmt{_ctx, _conn, sql.c_str()};
        return stmt;
    }




    void commit(){
        int res = DPI_FAILURE; //let's asume failure as default
        // commit changes
        res = dpiConn_commit(_conn);
        if (res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
    }

    void rollack(){
        int res = DPI_FAILURE; //let's asume failure as default
        // rollback changes
        res = dpiConn_rollback(_conn);
        if (res == DPI_FAILURE){
            throw DBException::build(_ctx);
        }
    }
    
   
    template<typename R>
    auto transaction(std::function<R(DBConnection&)> f){
        try{
            DBConnection& conn = *this;
            R ans = f(conn);
            conn.commit();
            return ans;
        }catch(std::exception& ex){
            rollack();
            throw;
        }
    }

    void transaction(std::function<void()> f){
        try{
            DBConnection& conn = *this;
            f();
            conn.commit();
        }catch(std::exception& ex){
            rollack();
            throw;
        }
    }

    template<typename R>
    R transaction(std::function<R()> f){
        try{
            DBConnection& conn = *this;
            R ans = f();
            conn.commit();
            return ans;
        }catch(std::exception& ex){
            rollack();
            throw;
        }
    }

    virtual ~DBConnection(){
        try{
            if(_conn){
                dpiConn_release(_conn);
            }
        }catch(std::exception& ex){
            log.error(ex);
        }
    }
};


class DBEnvironment{
private:
    dpiContext* _ctx = nullptr;
    

public:
    DBEnvironment(){
        dpiErrorInfo err;
        if (dpiContext_create(DPI_MAJOR_VERSION, DPI_MINOR_VERSION, &_ctx, &err) < 0){
            throw DBException::build(_ctx, &err);
        }
    }

    DBConnection connect(string user, string pass, string connStr){
        DBConnection conn{_ctx, user, pass, connStr};
        return conn;
    }

    DBConnection connect(const char* user, const char* pass, const char* connStr){
        DBConnection conn{_ctx, user, pass, connStr};
        return conn;
    }

    virtual ~DBEnvironment(){
        try{
            if(_ctx){
                dpiContext_destroy(_ctx);
            }
        }catch(std::exception& ex){
            log.error(ex);
        }
    }
};



Int64 getLastInsertedPKInt64(DBConnection& conn, DBStatement& stmt, string table){

    string rowId = stmt.getLastRowId(); //Oracle's ROWID value, not PK

    stringstream ss;
    ss << "select id from ";
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

Int64 getLastInsertedPKInt64(DBConnection& conn, DBStatement& stmt, const char* table){
    string stable{table};
    return getLastInsertedPKInt64(conn, stmt, stable);
}


}
}
}