//-----------------------------------------------------------------------------
// Remote ImGui https://github.com/JordiRos/remoteimgui
// Uses
// ImGui https://github.com/ocornut/imgui 1.3
// Webby https://github.com/deplinenoise/webby
// LZ4   https://code.google.com/p/lz4/
//-----------------------------------------------------------------------------
#include "imgui_remote.h"

namespace ImGui
{

static WebSocketServer GServer;

void RemoteSetClipboardTextFn(void*, const char* text)
{
	std::string message("ImClipboard=");
	message.append(text);
	GServer.SendText(message.c_str(), message.size());
}

//------------------
// RemoteGetInput
// - get input received from remote safely (valid for 30 frames)
//------------------
bool RemoteGetInput(RemoteInput & input)
{
    bool res = false;
	if (GServer.ClientActive)
	{
        
		if (GServer.Frame - GServer.FrameReceived < IMGUI_REMOTE_INPUT_FRAMES)
		{
			input = GServer.Input;
			GServer.Input.MouseWheelDelta *= 0.9f;
            res = true;
		}
	}
    memset(GServer.Input.KeysDown, 0, 256*sizeof(bool));
    GServer.Input.KeyCtrl = false;
    GServer.Input.KeyShift = false;
  //  GServer.Input.MouseWheelDelta = 0;
	return res;
}


//------------------
// RemoteInit
// - initialize RemoteImGui on top of your ImGui
//------------------
void RemoteInit(const char *local_address, int local_port)
{
	GServer.Init(local_address, local_port);
}


bool RemoteActive()
{
    return GServer.ClientActive;
}

//------------------
// RemoteUpdate
// - update RemoteImGui stuff
//------------------
void RemoteUpdate()
{
	GServer.Frame++;
	GServer.Update();
}


//------------------
// RemoteDraw
// - send draw list commands to connected client
//------------------
void RemoteDraw(ImDrawList** const cmd_lists, int cmd_lists_count)
{
	if (GServer.ClientActive)
	{
		static int sendframe = 0;
		if (sendframe++ < 2) // every 2 frames, @TWEAK
		{
			return;
		}
		sendframe = 0;

		unsigned int totalSize = sizeof(unsigned int); // cmd_lists_count
		for(int n = 0; n < cmd_lists_count; n++)
		{ 
			const ImDrawList* cmd_list = cmd_lists[n];
			int cmd_count = cmd_list->CmdBuffer.size();
			int vtx_count = cmd_list->VtxBuffer.size();
			int idx_count = cmd_list->IdxBuffer.size();
			totalSize += 3 * sizeof(unsigned int); //cmd_count, vtx_count and idx_count
			totalSize += cmd_count*sizeof(WebSocketServer::Cmd) + vtx_count*sizeof(WebSocketServer::Vtx) + idx_count*sizeof(WebSocketServer::Idx);
		}

		GServer.PreparePacketFrame(totalSize);
		GServer.Write((unsigned int)cmd_lists_count);

		for (int n = 0; n < cmd_lists_count; n++)
		{
			const ImDrawList* cmd_list = cmd_lists[n];
			const ImDrawVert * vtx_src = cmd_list->VtxBuffer.begin();
			const ImDrawIdx * idx_src = cmd_list->IdxBuffer.begin();
			unsigned int cmd_count = cmd_list->CmdBuffer.size();
			unsigned int vtx_count = cmd_list->VtxBuffer.size();
			unsigned int idx_count = cmd_list->IdxBuffer.size();
			GServer.Write(cmd_count);
			GServer.Write(vtx_count);
			GServer.Write(idx_count);
			// Send 
			// Add all drawcmds
			WebSocketServer::Cmd cmd;
			const ImDrawCmd* pcmd_end = cmd_list->CmdBuffer.end();
			for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != pcmd_end; pcmd++)
			{
				cmd.Set(*pcmd);
				GServer.Write(cmd);
			}
			// Add all vtx
			WebSocketServer::Vtx vtx;
			int vtx_remaining = vtx_count;
			while (vtx_remaining-- > 0)
			{
				vtx.Set(*vtx_src++);
				GServer.Write(vtx);
			}

			// Add all idx
			WebSocketServer::Idx idx;

			int idx_remaining = idx_count;
			while (idx_remaining-- > 0)
			{
				idx.Set(*idx_src++);
				GServer.Write(idx);
			}

		}
		// Send
		GServer.SendPacket();		
	}
}


} // namespace
