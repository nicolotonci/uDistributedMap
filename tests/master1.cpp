#include <iostream>
#include <algorithm>
#include <ctime>
#include <ff/ff.hpp>
#include <DMap.hpp>

struct MyEnv {
    int add_parameter;

    MyEnv(){}

    template <class Archive>
    void serialize( Archive & ar ){
        ar(add_parameter);
    }
};

int main(int argc, char*argv[]) {
    if (argc < 3){
        ff::error("Usage: master1 <Name of socket> <list_of_workers>");
        return 1;
    }

    std::srand(unsigned(std::time(nullptr)));
    std::vector<int> input(1000);
    std::generate(input.begin(), input.end(), std::rand);
    //std::vector<int> input = {1,2,3,4,5,6,7,8,9};
    std::vector<int> output(input.size());

    std::vector<std::string> destinations;
	for(int i = 2; i < argc; i++){
		destinations.push_back(std::move(std::string(argv[i])));
		//std::cout << "Destination " << i << ": " << argv[i] << std::endl;
	}

    MyEnv env;
    env.add_parameter = 20;

    DMapMaster map(argv[1], destinations, input.begin(), input.end(), output.begin(), &env);
    
    if (map.run_and_wait_end()<0) {
		error("running pipe\n");
		return -1;
	}

    std::cout << "Map terminated" << std::endl;

    for(auto i : output)
        std::cout << i << std::endl;

    return 0;
}