//-----------------------------------------------------------------------------
// Remote ImGui https://github.com/JordiRos/remoteimgui
// Uses
// ImGui https://github.com/ocornut/imgui 1.3
// Webby https://github.com/deplinenoise/webby
// LZ4   https://code.google.com/p/lz4/
//-----------------------------------------------------------------------------
#ifndef IMGUIREMOTE_H_
#define IMGUIREMOTE_H_

#include "lz4/lz4.h"
#include <stdio.h>

#define IMGUI_REMOTE_KEY_FRAME    60 // send keyframe every 30 frames
#define IMGUI_REMOTE_INPUT_FRAMES 60 // input valid during 120 frames

#include "imgui.h"
#include "imgui_remote_webby.h"

namespace ImGui
{
//------------------
// ImGuiRemoteInput
// - a structure to store input received from remote imgui, so you can use it on your whole app (keys, mouse) or just in imgui engine
// - use GetImGuiRemoteInput to read input data safely (valid for IMGUI_REMOTE_INPUT_FRAMES)
//------------------
struct RemoteInput
{
    ImVec2	MousePos;
    int		MouseButtons;
    float	MouseWheelDelta;
    bool	KeyCtrl;
    bool	KeyShift;
    bool	KeysDown[256];
};

//------------------
// IWebSocketServer
// - WebSocketServer interface
//------------------
/*
struct IWebSocketServer
{
	enum OpCode
	{
		Text,
		Binary,
		Disconnect,
		Ping,
		Pong
	};
	void Init(const char *bind_address, int port);
	void Shutdown();
	void SendText(const void *data, int size);
	void SendBinary(const void *data, int size);
  	virtual void OnMessage(OpCode opcode, const void *data, int size) { }
	virtual void OnError() { }
};
*/


//------------------
// WebSocketServer
// - ImGui web socket server connection
//------------------
struct WebSocketServer : public IWebSocketServer
{
	bool ClientActive;
	int Frame;
	int FrameReceived;
	int PrevPacketSize;
	bool IsKeyFrame;
	bool ForceKeyFrame;
	ImVector<unsigned char> Packet;
	ImVector<unsigned char> PrevPacket;
	RemoteInput Input;

	WebSocketServer()
	{
		ClientActive = false;
		Frame = 0;
		FrameReceived = 0;
		IsKeyFrame = false;
		PrevPacketSize = 0;

		Packet.reserve(4096);
		PrevPacket.reserve(4096);
	}

    virtual void OnMessage(OpCode opcode, const void *data, int size);

#pragma pack(1)
	struct Cmd
	{
		int   elemCount;
		float clip_rect[4];
		void Set(const ImDrawCmd &draw_cmd)
		{
			elemCount = draw_cmd.ElemCount;
			clip_rect[0] = draw_cmd.ClipRect.x;
			clip_rect[1] = draw_cmd.ClipRect.y;
			clip_rect[2] = draw_cmd.ClipRect.z;
			clip_rect[3] = draw_cmd.ClipRect.w;
			//printf("DrawCmd: %d ( %.2f, %.2f, %.2f, %.2f )\n", vtx_count, clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]);
		}
	};
	struct Vtx
	{
		short x,y; // 16 short
		short u,v; // 16 fixed point
		unsigned char r,g,b,a; // 8*4
		void Set(const ImDrawVert &vtx)
		{
			x = (short)(vtx.pos.x);
			y = (short)(vtx.pos.y);
			u = (short)(vtx.uv.x * 32767.f);
			v = (short)(vtx.uv.y * 32767.f);
			r = (vtx.col>>0 ) & 0xff;
			g = (vtx.col>>8 ) & 0xff;
			b = (vtx.col>>16) & 0xff;
			a = (vtx.col>>24) & 0xff;
		}
	};
	struct Idx
	{
		unsigned short idx;

		void Set(ImDrawIdx _idx)
		{
			idx = _idx;
		}
	};
#pragma pack()

	void Write(unsigned char c) { Packet.push_back(c); }
	void Write(unsigned int i)
	{
		if (IsKeyFrame)
			Write(&i, sizeof(unsigned int));
		else
			WriteDiff(&i, sizeof(unsigned int));
	}
	void Write(Cmd const &cmd)
	{
		if (IsKeyFrame)
			Write((void *)&cmd, sizeof(Cmd));
		else
			WriteDiff((void *)&cmd, sizeof(Cmd));
	}
	void Write(Vtx const &vtx)
	{
		if (IsKeyFrame)
			Write((void *)&vtx, sizeof(Vtx));
		else
			WriteDiff((void *)&vtx, sizeof(Vtx));
	}

	void Write(Idx const &idx)
	{
		if (IsKeyFrame)
			Write((void *)&idx, sizeof(Idx));
		else
			WriteDiff((void *)&idx, sizeof(Idx));
	}

	void Write(const void *data, int size)
	{
		unsigned char *src = (unsigned char *)data;
		for (int i = 0; i < size; i++)
		{
			int pos = (int)Packet.size();
			Write(src[i]);
			PrevPacket[pos] = src[i];
		}
	}
	void WriteDiff(const void *data, int size)
	{
		unsigned char *src = (unsigned char *)data;
		for (int i = 0; i < size; i++)
		{
			int pos = (int)Packet.size();
			Write((unsigned char)(src[i] - (pos < PrevPacketSize ? PrevPacket[pos] : 0)));
			PrevPacket[pos] = src[i];
		}
	}

	void SendPacket()
	{
		static int buffer[65536];
		int size = (int)Packet.size();
		int csize = LZ4_compress_limitedOutput((char *)&Packet[0], (char *)(buffer+3), size, 65536*sizeof(int)-12);
		buffer[0] = 0xBAADFEED; // Our LZ4 header magic number (used in custom lz4.js to decompress)
		buffer[1] = size;
		buffer[2] = csize;
		//printf("ImWebSocket SendPacket: %s %d / %d (%.2f%%)\n", IsKeyFrame ? "(KEY)" : "", size, csize, (float)csize * 100.f / size);
		SendBinary(buffer, csize+12);
		PrevPacketSize = size;
	}

	void PreparePacket(unsigned char data_type, unsigned int data_size)
	{
		unsigned int size = sizeof(unsigned char) + data_size;
		Packet.clear();
		Packet.reserve(size);
		PrevPacket.reserve(size);
		while (size > (unsigned int)PrevPacket.size())
			PrevPacket.push_back(0);
		Write(data_type);
	}

	// packet types
	enum { TEX_FONT = 255, FRAME_KEY = 254, FRAME_DIFF = 253 };

	void PreparePacketTexFont(const void *data, unsigned int w, unsigned int h)
	{
		IsKeyFrame = true;
		PreparePacket(TEX_FONT, sizeof(unsigned int)*2 + w*h);
		Write(w);
		Write(h);
		Write(data, w*h);
		ForceKeyFrame = true;
	}

	void PreparePacketFrame(unsigned int size)//unsigned int cmd_count, unsigned int vtx_count, unsigned int idx_count)
	{
		IsKeyFrame = (Frame%IMGUI_REMOTE_KEY_FRAME) == 0 || ForceKeyFrame;
		PreparePacket((unsigned char)(IsKeyFrame ? FRAME_KEY : FRAME_DIFF), size);	
		//printf("ImWebSocket PreparePacket: cmd_count = %i, vtx_count = %i ( %lu bytes )\n", cmd_count, vtx_count, sizeof(unsigned int) + sizeof(unsigned int) + cmd_count * sizeof(Cmd) + vtx_count * sizeof(Vtx));
		ForceKeyFrame = false;
	}
};

void RemoteSetClipboardTextFn(void*, const char* text);

//------------------
// RemoteGetInput
// - get input received from remote safely (valid for 30 frames)
//------------------
bool RemoteGetInput(RemoteInput & input);


//------------------
// RemoteInit
// - initialize RemoteImGui on top of your ImGui
//------------------
void RemoteInit(const char *local_address, int local_port);


bool RemoteActive();

//------------------
// RemoteUpdate
// - update RemoteImGui stuff
//------------------
void RemoteUpdate();


//------------------
// RemoteDraw
// - send draw list commands to connected client
//------------------
void RemoteDraw(ImDrawList** const cmd_lists, int cmd_lists_count);


} // namespace

#endif
