## Introduction

Zest is a simple network library using C++ language, based on the reactor pattern.

https://github.com/huanggomery/zest

## Environment

+ Linux kernel version 5.15.0-83-generic

+ g++ 9.4.0

+ xmake 

If you haven't installed xmake yet, please see https://xmake.io for more information.

## Build

First, clone the code repository and enter the directory.

```bash
git clone https://github.com/huanggomery/zest.git
cd zest
```

Then, run

```bash
./build.sh
```

Alternatively, you can specify the installation path (default /usr/local/)

```bash
./build.sh /your/specify/path/
```

## Example

In the `example` directory, you can find several C++ files that demonstrate how to use the zest network library to create an echo  server and echo client. Additionally, there's a stress testing tool available.

After successfully running `./build.sh`, you can find the following three executable files in the `bin` directory:

+ `echo_server`
+ `echo_client`
+ `echo_bench`

Please run the `echo_server` first, and then you can run `echo_client`, it will send data to the `echo_server` every second and then print the received data.

`echo_bench` is a stress testing tool.  To get its usage, you can run:

```bash
./echo_bench -h
```

Here is a result of the stress test:![](/home/huangchen/Project/zest/pics/stress_test.png)

You might notice inconsistency between two pieces of data, which is because the client might terminate the test before receiving a response after sending data.

## Tutorial

First of all, if you want to use the logging system, you should call `zest::Logger::InitGlobalLogger()` function to complete the initialization. For example:

```c++
#include "zest/base/logging.h"
zest::Logger::InitGlobalLogger("DEBUG", "echo_server", "../logs/");
```

You need to bind a listening address for your server, such as creating an IPv4 address:

```c++
zest::net::InetAddress local_addr("127.0.0.1", 12345);
```

zest::net::InetAddress supports multiple constructors:

```c++
InetAddress(const std::string &addr_str);
InetAddress(const std::string &ip_str, uint16_t port);
InetAddress(const char *addr_str);
InetAddress(const char *ip_str, uint16_t port);
InetAddress(sockaddr_in addr);
InetAddress(in_addr_t ip, uint16_t port);
```

Then, you can create a TcpServer, requiring the address and the number of threads as parameters:

```c++
zest::net::TcpServer server(local_addr, 4);
```

Use the following interface functions to bind callback functions for different events. The callback function should have the form

`void yourCallback(zest::net::TcpConnection &conn)`

```c++
// interface functions
void setOnConnectionCallback(const ConnectionCallbackFunc &cb);
void setMessageCallback(const ConnectionCallbackFunc &cb);
void setWriteCompleteCallback(const ConnectionCallbackFunc &cb);
void setCloseCallback(const ConnectionCallbackFunc &cb);
```

In the callback functions, you can use the interfaces provided by `zest::net::TcpConnection`

|         Interface          |                      Parameter                      |           Ouput           |               Comment               |
| :------------------------: | :-------------------------------------------------: | :-----------------------: | :---------------------------------: |
|      `waitForMessage`      |                          -                          |             -             |                                     |
|           `data`           |                          -                          |        std::string        |    string data in receive buffer    |
|           `send`           |                std::string / char *                 |             -             |      send data to peer address      |
|        `clearData`         |                          -                          |             -             |      clear the receive buffer       |
|      `clearBytesData`      |                         int                         |             -             | clear n bytes in the receive buffer |
|         `shutdown`         |                          -                          |             -             |      half close the connection      |
|         `socketfd`         |                          -                          |            int            |     get socket file descriptor      |
|       `peerAddress`        |                          -                          | zest::net::NetBaseAddress |          get peer address           |
|         `setState`         |                 zest::net::TcpState                 |             -             |                                     |
|         `getState`         |                          -                          |    zest::net::TcpState    |                                     |
|    `setMessageCallback`    |   std::function\<void(zest::net::TcpConnection&)>   |             -             |                                     |
| `setWriteCompleteCallback` |   std::function\<void(zest::net::TcpConnection&)>   |             -             |                                     |
|     `setCloseCallback`     |   std::function\<void(zest::net::TcpConnection&)>   |             -             |                                     |
|         `addTimer`         | std::string, uint64_t, std::function\<void()>, bool |             -             |     add timer to the connection     |
|        `resetTimer`        |                     std::string                     |             -             |                                     |
|        `resetTimer`        |                std::string, uint64_t                |             -             |   reset timer using new interval    |
|       `cancelTimer`        |                     std::string                     |             -             |                                     |
|   `deleteFromEventLoop`    |                          -                          |             -             |                                     |

**Furthermore, `zest::net::TcpConnection` provides two interfaces for users to store and read arbitrary data in this TCP connection:**

```c++
template <typename ValueType, typename... Args>
bool Put(const std::string &key, Args&&... args);

template <typename ValueType>
ValueType* Get(const std::string &key) const;
```

You can use them as follow:

```c++
// conn is a zest::net::TcpConnection object.
// Foo is a user-defined class that has a constructor Foo(int, double)

conn.Put<Foo>("my_foo", 42, 3.14);   // create a Foo(42, 3.14) named "my_foo" and then put it into conn
Foo* foo = conn.Get<Foo>("my_foo");  // get it by its name "my_foo"
```

***This allows you to customize data to record the state of this TCP connection, enabling richer functionality.***



That's all, have a good time!
