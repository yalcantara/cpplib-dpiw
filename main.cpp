//
// Created by Yaison on 12/2/23.
//
#include <ylib/core/lang.h>
#include <ylib/db/dpiw.h>
#include <ylib/utils/properties.h>

namespace fs = std::filesystem;
using namespace ylib::utils;
using namespace ylib::db::dpiw;


int main() {

    auto configPath = fs::path(checkAndGetEnv("app_config_path"));

    auto props = loadProperties(configPath / "db.properties");

    auto user = props.get("app_user");
    auto pass = props.get("app_pass");
    auto tnsn = props.get("app_tnsn");

    DBEnvironment env;

    auto conn = env.connect(user, pass, tnsn);
    auto stm = conn.statement("SELECT table_name, num_rows FROM user_tables");

    println("table_name     num_rows");
    stm.execQuery().forEach([](ResultSet &r) {
        printf("%s  %d\n", r.getString(1).c_str(), r.getInt32(2));
    });

    conn.transaction([]() -> Int64 {
        println("======>>Inside transaction");
        return 0;
    });

    return EXIT_SUCCESS;
}