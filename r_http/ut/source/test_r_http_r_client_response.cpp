
#include "test_r_http_r_client_response.h"
#include "r_utils/r_socket.h"
#include "r_http/r_http_exception.h"
#include "r_http/r_client_request.h"
#include "r_http/r_server_request.h"
#include "r_http/r_client_response.h"
#include "r_http/r_server_response.h"
#include <thread>
#include <unistd.h>

using namespace std;
using namespace r_utils;
using namespace r_http;

REGISTER_TEST_FIXTURE(test_r_http_r_client_response);

void test_r_http_r_client_response::setup()
{
}

void test_r_http_r_client_response::teardown()
{
}

void test_r_http_r_client_response::test_default_constructor()
{
    r_client_response response;
}

void test_r_http_r_client_response::test_copy_constructor()
{
}

void test_r_http_r_client_response::test_assignment_operator()
{
}

void test_r_http_r_client_response::test_receive_response()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        auto clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        auto body = ssRequest.get_body_as_string();

        r_server_response ssResponse;
        ssResponse.set_body(body);
        RTF_ASSERT_NO_THROW(ssResponse.write_response(clientSocket));

        clientSocket.close();

        socket.close();
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    string message = "Hello, Webby!";

    r_client_request request("127.0.0.1", port);

    request.set_method(METHOD_POST);
    request.set_body((uint8_t*)message.c_str(), message.length());

    RTF_ASSERT_NO_THROW(request.write_request(socket));

    r_client_response response;
    response.read_response(socket);

    auto responseBody = response.get_body_as_string();

    RTF_ASSERT(responseBody == "Hello, Webby!");
}

void test_r_http_r_client_response::test_streaming()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        auto clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        r_server_response ssResponse;
        ssResponse.set_content_type("application/x-rtsp-tunnelled");

        for(uint8_t i = 0; i < 10; ++i)
            ssResponse.write_chunk(clientSocket, 1, &i);
        
        ssResponse.write_chunk_finalizer(clientSocket);
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    string message = "Hello, Webby!";

    r_client_request request("127.0.0.1", port);
    request.set_method(METHOD_POST);
    request.set_body(message);

    request.write_request(socket);

    uint8_t sum = 0;

    r_client_response response;
    response.register_chunk_callback([&](const vector<uint8_t>& chunk, const r_client_response& response){
        sum += chunk[0];
    },
    true);
    response.read_response(socket);

    // For this test the server sends back 10 chunks, each 1 byte long... Containing the values
    // 9 down to 0. The sum of 0 .. 9 == 45. Our callback just sums them, so at this point our
    // _total should == 45.
    RTF_ASSERT(sum == 45);
}

void test_r_http_r_client_response::test_100_continue()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        auto clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        auto body = ssRequest.get_body_as_string();

        string cont = "HTTP/1.1 100 Continue\r\n\r\n";
        clientSocket.send(cont.c_str(), cont.length());

        r_server_response ssResponse;
        ssResponse.set_body(body);
        ssResponse.write_response(clientSocket);
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    string message = "Hello, Webby!";

    r_client_request request("127.0.0.1", port);
    request.set_method(METHOD_POST);
    request.set_body(message);

    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    auto responseBody = response.get_body_as_string();

    RTF_ASSERT(responseBody == "Hello, Webby!");
}

void test_r_http_r_client_response::test_interrupted_streaming()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        auto clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        r_server_response ssResponse;
        ssResponse.set_content_type("application/x-rtsp-tunnelled");

        try
        {
            for(uint8_t i = 0; i < 10; ++i)
            {
                ssResponse.write_chunk(clientSocket, 1, &i);
                usleep(1000000);
            }

            ssResponse.write_chunk_finalizer(clientSocket);
        }
        catch(...)
        {
            // supress the io interrupted exception...
        }
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    string message = "Hello, Webby!";

    r_client_request request("127.0.0.1", port);
    request.set_method(METHOD_POST);
    request.set_body(message);

    request.write_request(socket);

    auto thInt = std::thread([&](){
        usleep(5000000);
        socket.close();
    });
    thInt.detach();

    uint8_t sum = 0;

    r_client_response response;
    response.register_chunk_callback([&](const vector<uint8_t>& chunk, const r_client_response& response){
        sum += chunk[0];
    },
    true);

    RTF_ASSERT_THROWS(response.read_response(socket), r_socket_exception);

    // For this test the server sends back 10 chunks, each 1 byte long... Containing the values
    // 9 down to 0. But, we interrupt it 5 seconds in... So, the total here is probably 10 (0+1+2+3+4), but
    // the safer test (with less race conditions) is to just test that the _total is less than the 45 it would
    // be if it had run to completion.
    RTF_ASSERT(sum < 45);
}

void test_r_http_r_client_response::test_multi_part()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        auto clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        r_server_response ssResponse;
        ssResponse.set_content_type("multipart/x-mixed-replace; boundary=\"foo\"");

        ssResponse.write_response(clientSocket);

        for(uint8_t i = 0; i < 10; ++i)
        {
            map<string,string> headers;
            headers.insert(make_pair("Content-Type", "image/jpeg"));
            ssResponse.write_part(clientSocket, "foo", headers, &i, 1);
            usleep(1000000);
        }

        ssResponse.write_part_finalizer( clientSocket, "foo");
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    string message = "Hello, Webby!";

    r_client_request request("127.0.0.1", port);
    request.set_method(METHOD_POST);
    request.set_body(message);

    request.write_request(socket);

    uint8_t sum = 0;

    r_client_response response;
    response.register_part_callback([&](const std::vector<uint8_t>& chunk, const std::map<std::string, string>& headers, const r_client_response& response){
        sum += chunk[0];
    });

    response.read_response(socket);

    RTF_ASSERT(sum == 45);
}

void test_r_http_r_client_response::TestColonsInHeaders()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        r_server_response ssResponse;
        ssResponse.add_additional_header("HeaderWithColon", "This header has a : in it.");

        ssResponse.write_response(clientSocket);
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    string headerValue = response.get_header("HeaderWithColon");

    RTF_ASSERT(r_string_utils::contains(headerValue, "This header has a : in it."));
}

void test_r_http_r_client_response::TestMultipleHeadersWithSameKey()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        //Note: We don't use r_server_response here because r_server_response does not support
        //multiple header values associated with the same key. If this is needed, we could add it, but
        //for now we just return a manually constructed response.

        string response = "HTTP/1.1 200 OK\r\nDate: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nkey: val 2\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    vector<string> matchingHeaders = response.get_all_matching_headers("key");

    int numHeaders = matchingHeaders.size();

    RTF_ASSERT(numHeaders == 2);

    RTF_ASSERT(matchingHeaders[0] == "val 1");
    RTF_ASSERT(matchingHeaders[1] == "val 2");

}

void test_r_http_r_client_response::TestSpaceAtHeaderStart()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        string response = "HTTP/1.1 200 OK\r\n Date: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nhole: val 2\r\nvalue:key\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    RTF_ASSERT_THROWS(response.read_response(socket), r_http_exception_generic);
}

void test_r_http_r_client_response::TestTabAtHeaderStart()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        string response = "HTTP/1.1 200 OK\r\n\tDate: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nhole: val 2\r\nvalue:key\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    RTF_ASSERT_THROWS(response.read_response(socket), r_http_exception_generic);
}

void test_r_http_r_client_response::TestSpaceAtHeaderLine()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        string response = "HTTP/1.1 200 OK\r\nDate: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nhole: val 2\r\n value:key\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    vector<string> matchingHeaders = response.get_all_matching_headers("key");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 1");

    matchingHeaders = response.get_all_matching_headers("hole");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 2\r\n value:key");

    RTF_ASSERT(response.get_all_matching_headers("value").empty());
}

void test_r_http_r_client_response::TestTabAtHeaderLine()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        string response = "HTTP/1.1 200 OK\r\nDate: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nhole: val 2\r\n\tvalue:key\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    vector<string> matchingHeaders = response.get_all_matching_headers("key");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 1");

    matchingHeaders = response.get_all_matching_headers("hole");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 2\r\n\tvalue:key");

    RTF_ASSERT(response.get_all_matching_headers("value").empty());
}

void test_r_http_r_client_response::TestMultipleKeys()
{
    int port = RTF_NEXT_PORT();

    auto th = thread([&](){
        r_socket socket;
        socket.bind(port);
        socket.listen();

        r_socket clientSocket = socket.accept();

        r_server_request ssRequest;
        ssRequest.read_request(clientSocket);

        string response = "HTTP/1.1 200 OK\r\nDate: Mon Nov 14 09:58:23 2011\r\nContent-Type: text/plain\r\nkey: val 1\r\nhole: val 2\r\nvalue:key\r\n\r\n";
        clientSocket.send(response.c_str(), response.length());
    });
    th.detach();

    usleep(250000);

    r_socket socket;
    socket.connect("127.0.0.1", port);

    r_client_request request("127.0.0.1", port);
    request.write_request(socket);

    r_client_response response;
    response.read_response(socket);

    vector<string> matchingHeaders = response.get_all_matching_headers("key");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 1");

    matchingHeaders = response.get_all_matching_headers("hole");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "val 2");

    matchingHeaders = response.get_all_matching_headers("value");
    RTF_ASSERT(matchingHeaders.size() == 1);
    RTF_ASSERT(matchingHeaders[0] == "key");
}
