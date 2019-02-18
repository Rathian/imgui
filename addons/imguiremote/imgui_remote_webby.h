//-----------------------------------------------------------------------------
// Remote ImGui https://github.com/JordiRos/remoteimgui
// Uses
// ImGui https://github.com/ocornut/imgui 1.3
// Webby https://github.com/deplinenoise/webby
// LZ4   https://code.google.com/p/lz4/
//-----------------------------------------------------------------------------
#ifndef IMGUIREMOTE_WEBBY_H_
#define IMGUIREMOTE_WEBBY_H_

#include "webby/webby.h"

namespace ImGui 
{
	struct IWebSocketServer
	{
		enum OpCode // based on websocket connection opcodes
		{
			Continuation = 0,
			Text = 1,
			Binary = 2,
			Disconnect = 8,
			Ping = 9,
			Pong = 10,
		};

		void *Memory;
		int MemorySize;
		struct WebbyServer *Server;
		struct WebbyServerConfig ServerConfig;
		struct WebbyConnection *Client;

		int Init(const char *local_address, int local_port);

		void Update();

		void Shutdown();

		void WsOnConnected(struct WebbyConnection *connection);

		void WsOnDisconnected(struct WebbyConnection *connection);

		int WsOnFrame(struct WebbyConnection *connection, const struct WebbyWsFrame *frame);
		
		virtual void OnMessage(OpCode opcode, const void *data, int size);
		virtual void OnError();
		virtual void SendText(const void *data, int size);
		virtual void SendBinary(const void *data, int size);
	};

	static void onLog(const char* text);
	static int  onDispatch(struct WebbyConnection *connection);
	static int  onConnect(struct WebbyConnection *connection);
	static void onConnected(struct WebbyConnection *connection);
	static void onDisconnected(struct WebbyConnection *connection);
	static int  onFrame(struct WebbyConnection *connection, const struct WebbyWsFrame *frame);
}

#endif
