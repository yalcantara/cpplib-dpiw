#pragma once

#include <memory>

#include <occi.h>
#include <ylib/core/lang.h>
#include <ylib/logging/Logger.h>

using namespace oracle::occi;
using ylib::logging::Logger;

namespace ylib {
namespace rdbms {
namespace orcl {

static Logger log = Logger::get( "DBConnection" );



class DBResultSet {
private:
	Statement* stm = nullptr; //not owning
	ResultSet* rs = nullptr;

public:
	DBResultSet(Statement* stm) {
		this->stm = stm;
		rs = stm->executeQuery();
	}

	Bool next(Int32 numRows) {
		checkParamIsPositive("numRows", numRows);

		ResultSet::Status status = rs->next((unsigned int)numRows);
		if (status == ResultSet::Status::END_OF_FETCH) {
			return False;
		}

		return True;
	}

	Bool next() {
		return next(1);
	}

	string getString(unsigned int colIdx) {
		return rs->getString(colIdx);
	}

	Int64 getInt64(unsigned int colIdx) {
		oracle::occi::Number num = rs->getNumber(colIdx);
		Int64 ans = num;
		return ans;
	}

	virtual ~DBResultSet() {
		try {
			if (rs) {
				if (stm) {
					stm->closeResultSet(rs);
					rs = nullptr;
				}
			}
		} catch (std::exception& ex) {
			log.error(ex);
		}
	}
};



class DBStatement {

private:
	Connection* conn = nullptr; //not owning
	Statement* stm = nullptr;

public:
	DBStatement(Connection* conn, string& sql) {
		this->conn = conn;
		stm = conn->createStatement(sql);
	}

	void setInt64(unsigned int idx, Int64 val) {
		oracle::occi::Number num{ val };
		stm->setNumber(idx, num);
	}

	void setString(unsigned int idx, string val) {
		stm->setString(idx, val);
	}

	DBResultSet executeQuery() {
		DBResultSet rs{ stm };
		return rs;
	}

	Int64 executeCount() {
		var rs = executeQuery();
	
		if (rs.next(1) == True) {
			var ans = rs.getInt64(1);
			return ans;
		}
		
		throw Exception("The ResultSet has no result for 'posible' count query.");
	}

	virtual ~DBStatement() {
		try {
			if (stm) {
				if (conn) {
					conn->terminateStatement(stm);
					stm = nullptr;
				}
			}
		} catch (std::exception& ex) {
			log.error(ex);
		}
	}
};



class DBConnection {
				
private:
	Environment* env = nullptr;
	Connection* conn = nullptr;
				
	void init(string& user, string& pass, string& connString) {
		env = Environment::createEnvironment(Environment::DEFAULT);
		conn = env->createConnection(user, pass, connString);
	}

public:
	DBConnection(string& user, string& pass, string& connString) {
		init(user, pass, connString);
	}

	DBConnection(const char* user, const char* pass, const char* connString)  {
		string suser{ user };
		string spass{ pass };
		string sconnString{ connString };
		init(suser, spass, sconnString);
	}

	DBStatement createStatement(string& sql) {
		DBStatement stm{ conn, sql };
		return stm;
	}

	DBStatement createStatement(const char* sql) {
		string ssql{ sql };
		return createStatement(ssql);
	}


	virtual ~DBConnection() {
		try {
			if (conn) {
				env->terminateConnection(conn);
				conn = nullptr;
			}
		} catch (std::exception ex) {
			log.error(ex);
		}

		try {
			if (env) {
				Environment::terminateEnvironment(env);
				env = nullptr;
			}
		} catch (std::exception& ex) {
			log.error(ex);
		}
	}
};











}
}
}