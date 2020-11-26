// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "test.h"

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "coyote.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using coyote::HelloRequest;
using coyote::HelloReply;
using coyote::Scheduler;
using namespace coyote;

class SchedulerClient {
public:
    SchedulerClient(std::shared_ptr<Channel> channel)
        : stub_(Scheduler::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.
    std::string SayHello(const std::string& user)
    {
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
        if (status.ok())
        {
            return reply.message();
        }
        else
        {
            std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
            return "RPC failed";
        }
    }

private:
    std::unique_ptr<Greeter::Stub> stub_;
};

int main(int argc, char** argv)
{
    std::string target_str;
    std::string arg_str("--target");
    if (argc > 1)
    {
        std::string arg_val = argv[1];
        size_t start_pos = arg_val.find(arg_str);
        if (start_pos != std::string::npos)
        {
            start_pos += arg_str.size();
            if (arg_val[start_pos] == '=')
            {
                target_str = arg_val.substr(start_pos + 1);
            }
            else
            {
                std::cout << "The only correct argument syntax is --target=" << std::endl;
                return 0;
            }
        }
        else
        {
            std::cout << "The only acceptable argument is --target=" << std::endl;
            return 0;
        }
    }
    else
    {
        target_str = "localhost:50051";
    }

    SchedulerClient scheduler(grpc::CreateChannel(
        target_str, grpc::InsecureChannelCredentials()));
    std::string user("world");
    std::string reply = scheduler.SayHello(user);
    std::cout << "Scheduler received: " << reply << std::endl;

    return 0;
}
