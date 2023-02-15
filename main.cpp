//
// Created by Yaison on 12/2/23.
//

#include <ylib/db/dpiw.h>

using namespace ylib::db::dpiw;


int main(){

    string user = checkAndGetEnv("app_user");
    string pass = checkAndGetEnv("app_pass");
    string tnsp = checkAndGetEnv("app_tnsp");

    DBEnvironment env;

    DBConnection conn = env.connect(user, pass, tnsp);
    DBStatement stm = conn.statement("SELECT table_name, num_rows FROM user_tables");

    println("table_name     num_rows");
    stm.execQuery().forEach([](ResultSet &r){
        printf("%s  %d\n", r.getString(1).c_str(), r.getInt32(2));
    });

    return EXIT_SUCCESS;
}