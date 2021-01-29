#include <DMap.hpp>
#include <iostream>
#include <chrono>

#define INPUT_SIZE 100000
#define EXECUTION_TIME 100
#define THREADS 40
#define INPUT_ELEMENT_SIZE 8
#define OUTPUT_ELEMENT_SIZE 8
#define CHUNK_SIZE 0   // 0 => static sxcheduling, dynamic scheduling otherwise

float active_delay(int msecs) {
  // read current time
  float x = 1.25f;
  auto start = std::chrono::high_resolution_clock::now();
  auto end   = false;
  while(!end) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    x *= sin(x) / atan(x) * tanh(x) * sqrt(x);
    if(msec>=msecs)
      end = true;
  }
  return x;
}

struct MyTypeIn {
    char data[INPUT_ELEMENT_SIZE];

    template <class Archive>
    void serialize( Archive & ar ){
        ar(data);
    }
};

struct MyTypeOut {
    char data[OUTPUT_ELEMENT_SIZE];

    template <class Archive>
    void serialize( Archive & ar ){
        ar(data);
    }
};

struct MyEnv {
    int add_parameter;

    MyEnv(){}

    template <class Archive>
    void serialize( Archive & ar ){
        ar(add_parameter);
    }
};

int main(int argc, char*argv[]){
    DMap::Exec exec(argc, argv);
    std::vector<MyTypeIn> input;
    std::vector<MyTypeOut> output;
    MyEnv* env = nullptr;
    if (exec.isMaster){
        // master execution => populate input
        /*
        std::srand(unsigned(std::time(nullptr)));
        std::generate(input.begin(), input.end(), std::rand);
        */

       input = std::vector<MyTypeIn>(INPUT_SIZE);
       output = std::vector<MyTypeOut>(INPUT_SIZE);
       
        env = new MyEnv();
        env->add_parameter = 100; 
    }

    if (DMap::map(exec, [](MyTypeIn& i){active_delay(EXECUTION_TIME); return MyTypeOut();}, input.begin(), input.end(), output.begin(), CHUNK_SIZE, (void*) nullptr, THREADS) < 0){
        std::cout << "ERROR" << std::endl;
        return 1;
    }

    // if i'm here it means that the execution is fine and i'm the master so i can consume the results

    /*for (auto i : output)
        std::cout << i << std::endl;
*/
    return 0;
}