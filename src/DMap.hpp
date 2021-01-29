#include <ff/ff.hpp>
#include <iterator>
#include <vector>
#include <sstream>
#include <DMapMaster.hpp>
#include <DMapWorker.hpp>

namespace DMap {

struct Exec {
    bool isMaster;
    std::string masterAddr;
    std::vector<std::string> workers_addrs;

    Exec(int argc, char*argv[]){
        if (argc == 1){
            ff::error("Usage: exec true exec true <Listen address> <Worker address> ... \n          OR: exec false <Listen address> <Master address> ");
            exit(EXIT_FAILURE);
        }
        
        std::istringstream(argv[1]) >> std::boolalpha >> isMaster;

        if (isMaster){
            if (argc < 4){
                ff::error("Usage: exec true <Listen address> <Worker address> ...");
                exit(EXIT_FAILURE);
            }
            masterAddr = std::string(argv[2]);
            for(int i = 3; i < argc; i++)
                workers_addrs.push_back(std::string(argv[i]));

        } else {
            if (argc!=4){
                ff::error("Usage: exec false <Listen address> <Master address>");
                exit(EXIT_FAILURE);
            }
            workers_addrs.push_back(std::string(argv[2]));
            masterAddr = std::string(argv[3]);
        }
    }

    Exec() = default;
};

template<typename InputIterator, typename OutputIterator, typename Function, typename Env = void>
int map(Exec& execEnv, Function f, InputIterator begin_in, InputIterator end_in, OutputIterator begin_out, size_t chunk_size = 0, Env* env = nullptr, int wth = FF_AUTO){
    typedef typename std::iterator_traits<InputIterator>::value_type Tin;
    typedef typename  std::iterator_traits<OutputIterator>::value_type Tout;
    if (execEnv.isMaster){
        DMapMaster m(execEnv.masterAddr, execEnv.workers_addrs, begin_in, end_in, begin_out, env, chunk_size);
        return m.run_and_wait_end();
    } else {
        DMapWorker<Tin, Tout, Env> w(f, execEnv.workers_addrs[0], execEnv.masterAddr, wth);
        
        if (w.run_and_wait_end() < 0){
            ff::error("Error executing worker");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }
    return 0;
}
}
