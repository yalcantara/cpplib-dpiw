//
// Created by Yaison2 on 12/2/23.
//

#include <ylib/db/dpiw.h>

using namespace ylib::db::dpiw;


int main(int argc, char** argv){

    string first = string{argv[0]};
    println(first);
    auto arguments = argToVec(argc, argv, 4);
    auto user = arguments.at(1);
    auto pass = arguments.at(2);
    auto tnsp = arguments.at(3);

    DBEnvironment env;

    auto conn = env.connect(user, pass, tnsp);

    std::cout << "hello" << std::endl;
    return 0;
}