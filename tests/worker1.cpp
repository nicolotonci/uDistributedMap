#include <iostream>
#include <ff/ff.hpp>
#include <DMapWorker.hpp>

struct MyEnv {
    int add_parameter;

    MyEnv(){}

    template <class Archive>
    void serialize( Archive & ar ){
        ar(add_parameter);
    }
};

int main(int argc, char*argv[]) {
    if (argc!=3){
        ff::error("Usage: master1 <listen_addr> <master_addr>");
        return 1;
    }

    DMapWorker<int, int, MyEnv> w(([](int &i, MyEnv* env){return i+(env->add_parameter);}), argv[1], argv[2]);

    w.run_and_wait_end();

    return 0;
}