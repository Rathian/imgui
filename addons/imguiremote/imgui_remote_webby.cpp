//-----------------------------------------------------------------------------
// Remote ImGui https://github.com/JordiRos/remoteimgui
// Uses
// ImGui https://github.com/ocornut/imgui 1.3
// Webby https://github.com/deplinenoise/webby
// LZ4   https://code.google.com/p/lz4/
//-----------------------------------------------------------------------------
#include "imgui_remote_webby.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */
#endif

namespace ImGui
{
    IWebSocketServer *s_WebSocketServer;

    int IWebSocketServer::Init(const char *local_address, int local_port)
    {
        s_WebSocketServer = this;

#if defined(_WIN32)
        {
            WORD wsa_version = MAKEWORD(2, 2);
            WSADATA wsa_data;
            if (0 != WSAStartup(wsa_version, &wsa_data))
                return -1;
        }
#endif

        memset(&ServerConfig, 0, sizeof ServerConfig);
        ServerConfig.bind_address = local_address;
        ServerConfig.listening_port = (unsigned short)(local_port);
        ServerConfig.flags = WEBBY_SERVER_WEBSOCKETS;
        ServerConfig.connection_max = 64;
        ServerConfig.request_buffer_size = 2048;
        ServerConfig.io_buffer_size = 8192;
        ServerConfig.dispatch = &onDispatch;
        ServerConfig.log = &onLog;
        ServerConfig.ws_connect = &onConnect;
        ServerConfig.ws_connected = &onConnected;
        ServerConfig.ws_closed = &onDisconnected;
        ServerConfig.ws_frame = &onFrame;

        MemorySize = WebbyServerMemoryNeeded(&ServerConfig);
        Memory = malloc(MemorySize);
        Server = WebbyServerInit(&ServerConfig, Memory, MemorySize);
        Client = NULL;
        if (!Server)
            return -2;

        return 0;
    }

    void IWebSocketServer::Update()
    {
        if (Server)
            WebbyServerUpdate(Server);
    }

    void IWebSocketServer::Shutdown()
    {
        if (Server)
        {
            WebbyServerShutdown(Server);
            free(Memory);
        }

#if defined(_WIN32)
        WSACleanup();
#endif
    }

    void IWebSocketServer::WsOnConnected(struct WebbyConnection *connection)
    {
        Client = connection;
    }

    void IWebSocketServer::WsOnDisconnected(struct WebbyConnection * /*connection*/)
    {
        Client = NULL;
        OnMessage(Disconnect, NULL, 0);
    }

    int IWebSocketServer::WsOnFrame(struct WebbyConnection *connection, const struct WebbyWsFrame *frame)
    {
        //		printf("WebSocket frame incoming\n");
        //		printf("  Frame OpCode: %d\n", frame->opcode);
        //		printf("  Final frame?: %s\n", (frame->flags & WEBBY_WSF_FIN) ? "yes" : "no");
        //		printf("  Masked?     : %s\n", (frame->flags & WEBBY_WSF_MASKED) ? "yes" : "no");
        //		printf("  Data Length : %d\n", (int) frame->payload_length);

        std::vector<unsigned char> buffer(frame->payload_length + 1);
        WebbyRead(connection, &buffer[0], frame->payload_length);
        buffer[frame->payload_length] = 0;
        //        if(!strstr((char*)&buffer[0],"ImMouseMove"))
        //            printf("  Data : %s\n", &buffer[0]);

        OnMessage((OpCode)frame->opcode, &buffer[0], frame->payload_length);

        return 0;
    }

    void IWebSocketServer::OnMessage(OpCode /*opcode*/, const void * /*data*/, int /*size*/) { }
    void IWebSocketServer::OnError() { }

    void IWebSocketServer::SendText(const void *data, int size)
    {
        if (Client)
        {
            WebbySendFrame(Client, WEBBY_WS_OP_TEXT_FRAME, data, size);
        }
    }

    void IWebSocketServer::SendBinary(const void *data, int size)
    {
        if (Client)
        {
            WebbySendFrame(Client, WEBBY_WS_OP_BINARY_FRAME, data, size);
        }
    }


    static void onLog(const char* /*text*/)
    {
        //printf("[WsOnLog] %s\n", text);
    }

    static int onDispatch(struct WebbyConnection *connection)
    {
        //printf("[WsOnDispatch] %s\n", connection->request.uri);
        if (0 == strcmp("/", connection->request.uri))
        {
            std::ifstream is("html/imgui.html");
            if (is.is_open())
            {
                std::string file((std::istreambuf_iterator<char>(is)),
                    std::istreambuf_iterator<char>());

                WebbyBeginResponse(connection, 200, -1, NULL, 0);
                WebbyWrite(connection, file.c_str(), file.size());
                WebbyEndResponse(connection);

                return 0;
            }
        }
        else if (0 == strncmp("/html", connection->request.uri, 5))
        {
            std::ifstream is(connection->request.uri + 1);
            if (is.is_open())
            {
                std::string file((std::istreambuf_iterator<char>(is)),
                    std::istreambuf_iterator<char>());

                WebbyBeginResponse(connection, 200, -1, NULL, 0);
                WebbyWrite(connection, file.c_str(), file.size());
                WebbyEndResponse(connection);

                return 0;
            }
        }
        return 1;
    }

    static int onConnect(struct WebbyConnection * /*connection*/)
    {
        //printf("[WsOnConnect] %s\n", connection->request.uri);
        return 0;
    }

    static void onConnected(struct WebbyConnection *connection)
    {
        //printf("[WsOnConnected]\n");
        s_WebSocketServer->WsOnConnected(connection);
    }

    static void onDisconnected(struct WebbyConnection *connection)
    {
        //printf("[WsOnDisconnected]\n");
        s_WebSocketServer->WsOnDisconnected(connection);
    }

    static int onFrame(struct WebbyConnection *connection, const struct WebbyWsFrame *frame)
    {
        //printf("[WsOnFrame]\n");
        return s_WebSocketServer->WsOnFrame(connection, frame);
    }
}
