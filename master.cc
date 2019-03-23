#include <iostream>
#include <string>
#include <unistd.h>
#include <conservator/ConservatorFrameworkFactory.h>
#include <zookeeper/zookeeper.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <grpc++/grpc++.h>
#include "hello.grpc.pb.h"
#define HOSTNAME_MAX_LEN 128
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;
using namespace std;

string default_master_prefix = "master1";
string hostname = "";
int stop = 0;
mutex terminated_mtx;
thread *greeting_thread = NULL;
mutex greeter_mtx;



class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assambles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

GreeterClient *greeter = NULL;

string state2String(ZOOAPI int state){
  if (state == 0)
    return "CLOSED_STATE";
  if (state == ZOO_CONNECTING_STATE)
    return "CONNECTING_STATE";
  if (state == ZOO_ASSOCIATING_STATE)
    return "ASSOCIATING_STATE";
  if (state == ZOO_CONNECTED_STATE)
    return "CONNECTED_STATE";
 if (state == ZOO_EXPIRED_SESSION_STATE)
    return "EXPIRED_SESSION_STATE";
  if (state == ZOO_AUTH_FAILED_STATE)
    return "AUTH_FAILED_STATE";

  return "INVALID_STATE";
}

void greeting_client()
{
    this_thread::sleep_for(chrono::seconds(1));
    while (1)
    {
        string value = "No workering is available.";
        greeter_mtx.lock();
        if (greeter != NULL)
        {
            value = greeter->SayHello(hostname);
        }
        greeter_mtx.unlock();
        cout << value << endl;
        //sleep
        this_thread::sleep_for(chrono::seconds(2));
        //check to terminated
        terminated_mtx.lock();
        if (stop == 1)
        {
            terminated_mtx.unlock();
            return;
        }
        terminated_mtx.unlock();
    }
}

void start_working_as_a_leader()
{
    terminated_mtx.lock();
    stop = 0;
    greeting_thread = new thread(greeting_client);
    terminated_mtx.unlock();
}

void stop_working_as_a_leader()
{
    terminated_mtx.lock();
    stop = 1;
    terminated_mtx.unlock();
}

void worker_update_fn(zhandle_t *zh, int type,
                      int state, const char *path, void *watcherCtx)
{
    cout << "worker updated" << endl;
    cout << type << state << endl;


    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    unique_ptr<ConservatorFramework> framework = factory.newClient("localhost:2181");
    framework->start();
    vector<string> worker_ids = framework->getChildren()->withWatcher(worker_update_fn, &framework)->forPath("/worker");
    //update_worker(worker_ids, &framework);
    int min = 0x7fffffff;
    for (string &id : worker_ids)
    {
        int temp = stoi(id);
        if (min > temp)
            min = temp;
    }
    if (min == 0x7fffffff)
    {
        cout << "No existed children" << endl;
        greeter_mtx.lock();
        greeter = NULL;
        greeter_mtx.unlock();
        return;
    }
    string worker_hostname = framework->getData()->forPath("/worker/" + to_string(min));
    cout << "Change to worker: " << worker_hostname << endl;
    greeter_mtx.lock();
    greeter = new GreeterClient(grpc::CreateChannel(worker_hostname + ":50051", grpc::InsecureChannelCredentials()));
    greeter_mtx.unlock();
}


void watch_leader(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    /*
    * type = 1 created
    *      = 2 deleted
    *      = 3 updated
    */
    if (type == 1)
    {
        //return the leadership to default leader
        cout << "Default leader is alive. Return leadership to the default leader." << endl;
        stop_working_as_a_leader();
    }
    else
    {
        //return leadership to leader
        cout << "The default leader is dead. Follower becomes leader." << endl;
        start_working_as_a_leader();
    }

    ConservatorFramework *framework = (ConservatorFramework *)watcherCtx;
    framework->checkExists()->withWatcher(watch_leader, framework)->forPath("/master/" + default_master_prefix);
}




int main(int argc, char** argv) {

  char current_hostname[1024];
  int is_default_leader = 0;

  if(gethostname(current_hostname, 1024) != 0) {

    cout << "Cannot get the hostname" << endl;
    return 0;
  }
  hostname = string(current_hostname);
  string nodename;
  //1.   nodename is master1 , master2 etc...
  nodename = hostname.substr(0, default_master_prefix.length());
  //check whether is the defalut master1 node
  if(hostname == default_master_prefix)
    is_default_leader = 1;

  // connect to local zookeeper server
  ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
  unique_ptr<ConservatorFramework> framework = factory.newClient("localhost:2181");
  framework->start();

  cout << "This master is the default leader? " << is_default_leader << endl;

  //to create the default master nodes
  framework->create()->forPath("/master", (char *)"master-nodes");
  //nodename = /master/master1 , /master/master2 etc...
  nodename = "/master/" + nodename;
  //check and create the ephemeral and sequence node
  if(framework->create()->withFlags(ZOO_EPHEMERAL)->forPath(nodename) != ZOK) {

      cout << "Error: Failed to create node " << nodename << endl;
      framework->close();
      return -1;
  }

  // //this is for greeting 
  // string used_worker_hostname = framework->getData()->forPath("/worker/worker1");
  // greeter_mtx.lock();
  // greeter = new GreeterClient(grpc::CreateChannel(used_worker_hostname + ":50051", grpc::InsecureChannelCredentials()));
  // greeter_mtx.unlock();

  // masters watch workers
  vector<string> worker_ids = framework->getChildren()->withWatcher(worker_update_fn, &framework)->forPath("/worker");

  int min = 0x7fffffff;
  for (string &id : worker_ids)
  {

      cout << "current work ids: " << id << endl;
      int temp = stoi(id);
      if (min > temp)
          min = temp;
  }
  if (min == 0x7fffffff)
  {
      cout << "No existed children" << endl;
      greeter_mtx.lock();
      greeter = NULL;
      greeter_mtx.unlock();
  }
  else
  {
      string used_worker_hostname = framework->getData()->forPath("/worker/" + to_string(min));
      cout << "Using worker: " << used_worker_hostname << endl;
      greeter_mtx.lock();
      greeter = new GreeterClient(grpc::CreateChannel(used_worker_hostname + ":50051", grpc::InsecureChannelCredentials()));
      greeter_mtx.unlock();
  }


  //2.    masters watch workers
  if (is_default_leader == 0)
    {
        int leader_existed = framework->checkExists()->withWatcher(watch_leader, ((void *)framework.get()))->forPath("/master/" + default_master_prefix);
        if (leader_existed != 0)
        {
            //default leader is not running
            cout << "The leader is not running. The follower becomes the leader." << endl;
            start_working_as_a_leader();
        }
        else
            cout << "A leader is running, enter follower mode" << endl;
    }
    else
        start_working_as_a_leader();

   

    int temp;
    cin >> temp;
    framework->close();
    return 0;

}
