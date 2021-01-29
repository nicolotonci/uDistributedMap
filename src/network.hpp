#include <iostream>
#include <sstream>
#include <ff/ff.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <cmath>
#include <string>

#include <cereal/cereal.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp> 
#include <cereal/types/array.hpp>
#include <cereal/archives/portable_binary.hpp>

#ifndef DMAPNETWORK_H
#define DMAPNETWORK_H

#define PORT 8080
#define MAXBACKLOG 32
#define MAX_RETRIES 15

//#define LOCAL

using namespace ff;

/*
    Struct representing the task that are sent to/from workers. This struct must be serializable to be sent on the network.
*/
template<typename T>
struct Dtask {
    size_t id_worker; 
    size_t begin_i, end_i; // range of where is collocated the sub-task in the original collection
    std::vector<T> data; 

    Dtask() = default;

    /*
        This constructor is used whene a task is generated from the scheduler. 
    */
    template<typename Iterator>
    Dtask(size_t worker, size_t begin, size_t end, Iterator first, Iterator last) : id_worker(worker), begin_i(begin), end_i(end), data(first, last) {}

    /* 
        This constructor is used when a result is created, it copies the metadata from the original input task
    */
    template<typename TT>
    Dtask(const Dtask<TT>& m){
        this->id_worker = m.id_worker;
        this->begin_i = m.begin_i;
        this->end_i = m.end_i;
        this->data = std::vector<T>(this->end_i - this->begin_i); // allocate a vector of size (end_i - begin_i)
    }

    /*
        Ceral's serialization function
    */
    template <class Archive>
    void serialize( Archive & ar ){
        ar( id_worker, begin_i, end_i, data);
    }

};

/*
    stringbuf implementation which avoid an extra copy when create it from a raw char c array.
    Mainly useful when receving from network and immediately after start deserializing.
*/
class dataBuffer: public std::stringbuf {
public:	
    dataBuffer()
        : std::stringbuf(std::ios::in | std::ios::out | std::ios::binary) {
	}
    dataBuffer(char p[], size_t _len, bool cleanup=false)
        : std::stringbuf(std::ios::in | std::ios::out | std::ios::binary),
		  len(_len),cleanup(cleanup) {
        setg(p, p, p + _len);
    }
	~dataBuffer() {
		if (cleanup) delete [] getPtr();
	}
	size_t getLen() const {
		if (len>=0) return len;
		return str().length();
	}
	char* getPtr() const {
		return eback();
	}

	void doNotCleanup(){
		cleanup = false;
	}

protected:	
	ssize_t len=-1;
	bool cleanup = false;
};

/*
    Helper function to split strings by a delimiter char
*/
std::vector<std::string> split (const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}


/*
    Netowrk receiver node
*/
template<typename Tout, typename Env = void>
class receiver: public ff::ff_node_t<Dtask<Tout>> { 
private:
	
    /*
         Read n bytes from a descriptor fd
    */
    ssize_t readn(int fd, char *ptr, size_t n) {  
        size_t   nleft = n;
        ssize_t  nread;

        while (nleft > 0) {
            if((nread = read(fd, ptr, nleft)) < 0) {
                if (nleft == n) return -1; /* error, return -1 */
                else break; /* error, return amount read so far */
            } else if (nread == 0) break; /* EOF */
            nleft -= nread;
            ptr += nread;
        }
        return(n - nleft); /* return >= 0 */
    }

    /*
        Helper function to read a complete iovector (of size count) from adescriptor
    */
    ssize_t readvn(int fd, struct iovec *v, int count){
        ssize_t rread;
        for (int cur = 0;;) {
            rread = readv(fd, v+cur, count-cur);
            if (rread <= 0) return rread; // error or closed connection
            while (cur < count && rread >= (ssize_t)v[cur].iov_len)
                rread -= v[cur++].iov_len;
            if (cur == count) return 1; // success!!
            v[cur].iov_base = (char *)v[cur].iov_base + rread;
            v[cur].iov_len -= rread;
        }
    }

    /*
        The main function which handle a network request coming from the file descriptor sck. 
        Here is performed the deserialization and the dispatching of data to the next stage.
     */
    int handleRequest(int sck){
		bool isEnv;
        size_t sz;

        // create the iovector representing our micro-protocol. Refer to receiver & sender section of the report.
        struct iovec iov[2];
        iov[0].iov_base = &isEnv;
        iov[0].iov_len = sizeof(isEnv);
        iov[1].iov_base = &sz;
        iov[1].iov_len = sizeof(sz);

        switch (readvn(sck, iov, 2)) {
           case -1: error("Error reading from socket"); // fatal error
           case  0: return -1; // connection close
        }

        // convert values to host byte order
        isEnv  = ntohl(isEnv);
        sz     = ntohl(sz);

        // if the size is greater than zero it means that there is data to read and also that is not an EOS flag.
        if (sz > 0){
            char* buff = new char[sz];
            assert(buff);
            // read from the socket exactly sz bytes
            if(readn(sck, buff, sz) < 0){
                error("Error reading from socket");
                delete [] buff;
                return -1;
            }
            
            // create the stream to perform the de-serialization
            dataBuffer strBuff(buff, sz, true); // <-- Zero copy here. See the dataBuffer definition.
            std::istream iss(&strBuff);
			cereal::PortableBinaryInputArchive iarchive(iss);

            // the received data structure represents an environment
            if (isEnv){
                // if the Environment is not void (it is actually void when the environment feature is not used, it is known at compile time)
                if constexpr (!std::is_void<Env>::value){
                    #ifdef VERBOSE
                        std::cout << "Received ENV! Writing to " << envptr << std::endl;
                    #endif
                    // de-serialize the environment if the destination was set
                    if (envptr)
                        iarchive >> **envptr;
                    
                }
            } else { // it is a task (i.e. Data)
                // create a task container
                Dtask<Tout>* data = new Dtask<Tout>;
                // de-serialize the data into the task
                iarchive >> *data;
                // send it to the next stage
                this->ff_send_out(data);
            }

            return 1;
        }

        // if size == 0 => EOS
         _neos++; // increment the eos received
        #ifdef VERBOSE
            std::cout << "Received EOS!" << std::endl;
        #endif           
        return -1;
    }

public:
    receiver(std::string acceptAddr, size_t input_channels, bool _isMaster = false, Env** _envptr = nullptr, int coreid=-1)
		: input_channels(input_channels), acceptAddr(acceptAddr), coreid(coreid), isMaster(_isMaster), envptr(_envptr) {
        }

    int svc_init() {
  		if (coreid!=-1)
			ff_mapThreadToCpu(coreid);
        
        #ifdef LOCAL
            // create an AF_LOCAL socket
            if ((listen_sck=socket(AF_LOCAL, SOCK_STREAM, 0)) < 0){
                error("Error creating the socket");
                return -1;
            }
            
            struct sockaddr_un serv_addr;
            memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sun_family = AF_LOCAL;
            // set the specified socket path 
            strncpy(serv_addr.sun_path, acceptAddr.c_str(), acceptAddr.size()+1);
        #endif

        #ifdef REMOTE
            // create an AF_INET socket
            if ((listen_sck=socket(AF_INET, SOCK_STREAM, 0)) < 0){
                error("Error creating the socket");
                return -1;
            }
            
            int enable = 1;
            // enable the reuse of the address
            if (setsockopt(listen_sck, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                error("setsockopt(SO_REUSEADDR) failed");

            // parse the port number from the acceptAddr string
            int port = std::stoi(split(acceptAddr, ':')[1]);

            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET; 
            serv_addr.sin_addr.s_addr = INADDR_ANY; // listen actually from any local network interface
            serv_addr.sin_port = htons(port); // on the specified port

        #endif

        if (bind(listen_sck, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0){
            error("Error binding");
            error(acceptAddr.c_str());
            return -1;
        }

        if (listen(listen_sck, MAXBACKLOG) < 0){
            error("Error listening");
            return -1;
        }

        return 0;
    }
    void svc_end() {
        close(this->listen_sck);

        #ifdef LOCAL
            unlink(this->acceptAddr.c_str()); // delete the socket file
        #endif
    }
    /* 
        Here i should not care of input type nor input data since they come from a socket listener.
        Everything will be handled inside a while true.
    */
    Dtask<Tout> *svc(Dtask<Tout>* task) {

        size_t establishedConnections = 0;

        // flag to trigger just once the scheduler if this receiver preceed it in the pipeline. Not used if the receiver preceed a worker
        bool boot = true; 

        fd_set set, tmpset;
        // intialize both sets (master, temp)
        FD_ZERO(&set);
        FD_ZERO(&tmpset);

        // add the listen socket to the master set
        FD_SET(this->listen_sck, &set);

        // hold the greater descriptor
        int fdmax = this->listen_sck; 
        
        // iterate untill i get exactly the number of input_channels EOS flags
        while(_neos < input_channels){

            // copy the master set to the temporary
            tmpset = set;

            // block untill a socket is activated
            switch(select(fdmax+1, &tmpset, NULL, NULL, NULL)){
                case -1: error("Error on selecting socket");
                case  0: continue;
            }

            // iterate over the file descriptor to see which one is active
            for(int i=0; i <= fdmax; i++) 
	            if (FD_ISSET(i, &tmpset)){
                    // if the socket active is the listen socket, it means there is a new connection to accept
                    if (i == this->listen_sck) {
                        int connfd = accept(this->listen_sck, (struct sockaddr*)NULL ,NULL);
                        if (connfd == -1){
                            error("Error accepting client");
                        } else {
                            FD_SET(connfd, &set);
                            if(connfd > fdmax) fdmax = connfd;
                            establishedConnections++;
                            // trigger the scheduler if this is the master and i have already all the workers connected - The condition holds only once
                            if (isMaster && establishedConnections == input_channels && boot){
                                this->ff_send_out(new Dtask<Tout>());
                                boot = false;
                            }
                        }
                        continue;
                    }
                    
                    // it is not a new connection, call receive and handle possible errors
                    if (this->handleRequest(i) < 0){
                        close(i);
                        FD_CLR(i, &set);
                        establishedConnections--;
                        // update the maximum file descriptor
                        if (i == fdmax)
                            for(int i=(fdmax-1);i>=0;--i)
                                if (FD_ISSET(i, &set)){
                                    fdmax = i;
                                    break;
                                }
                    }
                }

        }

        // if this is the receiver of the master, just go out since the rest of the pipline already terminated
        if (isMaster)
            return this->GO_OUT;
        // if this is the receiver of a worker, just propagate the EOS to the next stage
        return this->EOS;
    }

private:
    size_t _neos = 0;
    size_t input_channels;
    int listen_sck;
    std::string acceptAddr;	
	int coreid;
    bool isMaster;
    Env** envptr;
};






/*
    Netowrk sender node
*/
template<typename Tin, typename Env = void>
class sender: public ff::ff_node_t<Dtask<Tin>> { 
private:
    
    size_t _neos=0;
    int distibutedGroupId;
    int next_rr_destination = 0; //next destiation to send for round robin policy
    std::vector<std::string> destinations;
    std::map<int, int> sockets;
	int coreid;
	Env* env;

    /*
        Create a socket based connection to the specified destination
    */
    int create_connect(const std::string& destination){
        int socketFD;

        #ifdef LOCAL
            // create an AF_LOCAL socket
            socketFD = socket(AF_LOCAL, SOCK_STREAM, 0);
            if (socketFD < 0){
                error("\nError creating socket \n");
                return socketFD;
            }
            struct sockaddr_un serv_addr;
            memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sun_family = AF_LOCAL;

            // specify the socket path
            strncpy(serv_addr.sun_path, destination.c_str(), destination.size()+1);

            if (connect(socketFD, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
                close(socketFD);
                return -1;
            }
        #endif

        #ifdef REMOTE
            struct addrinfo hints;
            struct addrinfo *result, *rp;

            // parse name of the destination and port
            std::string port = split(destination, ':')[1];
            std::string dest = split(destination, ':')[0]; // it can be an ip address or a domain name

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
            hints.ai_socktype = SOCK_STREAM; /* Stream socket */
            hints.ai_flags = 0;
            hints.ai_protocol = IPPROTO_TCP;          /* Allow only TCP */

            // resolve the address 
            if (getaddrinfo(dest.c_str(), port.c_str(), &hints, &result) != 0){
                error("\n Error getting resolving the given address");
                return -1;
            }

            // try to connect to a possible one of the resolution results
            for (rp = result; rp != NULL; rp = rp->ai_next) {
               socketFD = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
               if (socketFD == -1)
                   continue;

               if (connect(socketFD, rp->ai_addr, rp->ai_addrlen) != -1)
                   break;                  /* Success */

               close(socketFD);
           }

           if (rp == NULL)            /* No address succeeded */
               return -1;

        #endif

        return socketFD;
    }

    /* 
        Mechanism of retrying when initialize a connection. 
    */
    int tryConnect(const std::string &destination){
        int fd, retries = 0;
        
        // exponential backoff policy of retrying (bounded on the number MAX_RETRIES)
        while((fd = this->create_connect(destination)) < 0 && ++retries < MAX_RETRIES)
            std::this_thread::sleep_for(std::chrono::milliseconds((long)std::pow(2, retries)));

        return fd;
    }

    /*
         Write n bytes to the descriptor fd
    */
    ssize_t writen(int fd, const char *ptr, size_t n) {  
        size_t   nleft = n;
        ssize_t  nwritten;
        
        while (nleft > 0) {
            if((nwritten = write(fd, ptr, nleft)) < 0) {
                if (nleft == n) return -1; /* error, return -1 */
                else break; /* error, return amount written so far */
            } else if (nwritten == 0) break; 
            nleft -= nwritten;
            ptr   += nwritten;
        }
        return(n - nleft); /* return >= 0 */
    }

    /*
        Helper function to write a complete iovector (of size count) to the descriptor fd
    */
    ssize_t writevn(int fd, struct iovec *v, int count){
        ssize_t written;
        for (int cur = 0;;) {
            written = writev(fd, v+cur, count-cur);
            if (written < 0) return -1;
            while (cur < count && written >= (ssize_t)v[cur].iov_len)
                written -= v[cur++].iov_len;
            if (cur == count) return 1; // success!!
            v[cur].iov_base = (char *)v[cur].iov_base + written;
            v[cur].iov_len -= written;
        }
    }

    /* 
        Serialize an object and send it over the specified socket 
    */
    template<typename T>
    int sendToSck(int sck, T* task, bool isEnv_ = false){
        
        // allocate the buffer
        dataBuffer buff;
        std::ostream oss(&buff);
		cereal::PortableBinaryOutputArchive oarchive(oss);
		// serialize the object 
        oarchive << *task;

        // convert variables to netowrk byte order
        size_t sz = htonl(buff.getLen());
        bool isEnv = htonl(isEnv_);

        // create the iovector representing our micro-protocol. Refer to receiver & sender section of the report.
        struct iovec iov[2];
        iov[0].iov_base = &isEnv;
        iov[0].iov_len = sizeof(isEnv);
        iov[1].iov_base = &sz;
        iov[1].iov_len = sizeof(sz);

        // write the iovector 
        if (writevn(sck, iov, 2) < 0){
            error("Error writing on socket");
            return -1;
        }

        // write the buffer (i.e. data)
        if (writen(sck, buff.getPtr(), buff.getLen()) < 0){
            error("Error writing on socket");
            return -1;
        }

        return 0;
    }

    
public:
    /*
        Constructor for a single destination (used by worker)
    */
    sender(const int dGroup_id, std::string destination, Env* env_ptr = nullptr, int coreid=-1)
		: distibutedGroupId(dGroup_id),coreid(coreid), env(env_ptr) {
        this->destinations.push_back(std::move(destination));
    }

    /*
        Constructor for multiple destination (used by master)
    */
    sender(const int dGroup_id, std::vector<std::string> destinations_v, Env* env_ptr = nullptr, int coreid=-1)
		: distibutedGroupId(dGroup_id), destinations(std::move(destinations_v)),coreid(coreid), env(env_ptr) {
        }

    int svc_init() {
		if (coreid!=-1)
			ff_mapThreadToCpu(coreid);
		
        // intialize a persisten connection to all specified destinations
        for(size_t i=0; i < this->destinations.size(); i++)
            sockets[i] = tryConnect(this->destinations[i]);

        // send to all the connected worker the environment if present - This information is known at compile time
        if constexpr (!std::is_void<Env>::value){
            if (env != nullptr)
                for (const auto& [_, sck] : sockets){
                    std::ignore = _;
                    if (sendToSck(sck, env, true) < 0)
                        return -1;
                }
                    
        }
        
        return 0;
    }

    void svc_end() {
        // close the socket not matter if local or remote
        for(size_t i=0; i < this->destinations.size(); i++)
            close(sockets[i]);    
    }

    Dtask<Tin> *svc(Dtask<Tin>* task) {
        int sck;
        // workers have just one destination, the master node, so everything must be sent to it
        if (this->destinations.size() == 1) 
            sck = sockets[0];
        else // otherwise send to the right worker (used only by master)
            sck = sockets[task->id_worker];

        sendToSck(sck, task);

        delete task;
        return this->GO_ON;
    }
    
    /*
        Redefinition of eosnotify method. When receiving an EOS propagate it to all the destinations.
    */
     void eosnotify(ssize_t) {
	    if (++_neos >= 1){
            size_t sz = htonl(0);
            bool isEnv = htonl(false);

            // create the iovector for sending <false, 0> 
            struct iovec iov[2];
            iov[0].iov_base = &isEnv;
            iov[0].iov_len = sizeof(isEnv);
            iov[1].iov_base = &sz;
            iov[1].iov_len = sizeof(sz);
            
            // send it to all the destinations
            for(const auto &[_, sck] : sockets){
                std::ignore = _;
                if (writevn(sck, iov, 2) <= 0)
                    ff::error("Error sending EOS");
            }
            #ifdef VERBOSE
                std::cout << "EOS sent on network" << std::endl;
            #endif
        }
    }

};

#endif
