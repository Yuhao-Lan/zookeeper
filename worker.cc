#include <iostream>
#include <string>
#include <unistd.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <zookeeper/zookeeper.h>
#include <thread>
#include <iostream>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include "hello.grpc.pb.h"
#define HOSTNAME_MAX_LEN 128
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using namespace std;

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  GreeterServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}


int main(int argc, char** argv) {
	//get the hostname in C; check whether it is successfully
	char hostname[HOSTNAME_MAX_LEN];
	if(gethostname(hostname, HOSTNAME_MAX_LEN) != 0) {

		cout << "Cannot get the hostname" << endl;
		return 0;
	}
	string myhostname = string(hostname);
	string helpname = "worker";
	string myhostid = myhostname.substr(helpname.length(), 1);
	//start host
	cout << "hostname: " << myhostname << "    " << "hostid: " << myhostid << " is running... "<< endl;

	//Create factory for framework
    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    //Create a new connection object to Zookeeper
    unique_ptr<ConservatorFramework> framework = factory.newClient("localhost:2181",10000);
    //Start the connection
    framework->start();
    cout << "Starting ZooKeeper...." << endl;

    //create the worker path
    framework->create()->forPath("/worker", (char *) "worker-nodes");
    //check the worknode create
  if (framework->create()->withFlags(ZOO_EPHEMERAL)->forPath("/worker/" + myhostid, myhostname.c_str()) != ZOK)
	{
	    cout << "Error: Failed to create node "
	         << "/worker/" + myhostname << endl;
	    framework->close();
	    return -1;
	}


	   // start RPC server
	cout << "Start RPC server ... " << endl;
	RunServer();
	int temp;
	cin >> temp;
	framework->close();
	return 0;


  
}
