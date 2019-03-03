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

bool RemoteSetTexture(ImU32* texture, unsigned int width, unsigned int height, ImTextureID id)
{
    if (GServer.ClientActive)
    {
        GServer.PreparePacketImage(texture, width, height, (unsigned int)id);
        GServer.SendPacket();
        return false;
    }
    else
    {
        return false;
    }
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

inline bool mapRemoteKey(int* remoteKey, bool isCtrlPressed)
{
    if (*remoteKey == 37)
        *remoteKey = ImGuiKey_LeftArrow;
    else if (*remoteKey == 40)
        *remoteKey = ImGuiKey_DownArrow;
    else if (*remoteKey == 38)
        *remoteKey = ImGuiKey_UpArrow;
    else if (*remoteKey == 39)
        *remoteKey = ImGuiKey_RightArrow;
    else if (*remoteKey == 46)
        *remoteKey = ImGuiKey_Delete;
    else if (*remoteKey == 9)
        *remoteKey = ImGuiKey_Tab;
    else if (*remoteKey == 8)
        *remoteKey = ImGuiKey_Backspace;
    else if (*remoteKey == 65 && isCtrlPressed)
        *remoteKey = 'a';
    else if (*remoteKey == 67 && isCtrlPressed)
        *remoteKey = 'c';
    else if (*remoteKey == 86 && isCtrlPressed)
        *remoteKey = 'v';
    else if (*remoteKey == 88 && isCtrlPressed)
        *remoteKey = 'x';
    else
        return true;

    return false;
}

void WebSocketServer::OnMessage(OpCode opcode, const void *data, int /*size*/)
{
    switch (opcode)
    {
        // Text message
    case WebSocketServer::Text:
        if (!ClientActive)
        {
            if (!memcmp(data, "ImInit", 6))
            {
                ClientActive = true;
                ForceKeyFrame = true;
                // Send confirmation
                SendText("ImInit", 6);

                int x, y;
                if (sscanf((char *)data, "ImInit=%d,%d", &x, &y) == 2)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    io.DisplaySize = ImVec2((float)x, (float)y);
                }

                // Send font texture
                unsigned char* pixels;
                int width, height;
                ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
                PreparePacketTexFont(pixels, width, height);
                SendPacket();
            }
        }
        else if (strstr((char *)data, "ImMouseMove"))
        {
            int x, y, mouse_left, mouse_right, mouse_middle;
            if (sscanf((char *)data, "ImMouseMove=%d,%d,%d,%d,%d", &x, &y, &mouse_left, &mouse_right, &mouse_middle) == 5)
            {
                FrameReceived = Frame;
                Input.MousePos.x = (float)x;
                Input.MousePos.y = (float)y;
                Input.MouseButtons = mouse_left | (mouse_right << 1) | (mouse_middle << 2);
            }
        }
        else if (strstr((char *)data, "ImMousePress"))
        {
            int l, r, m;
            if (sscanf((char *)data, "ImMousePress=%d,%d,%d", &l, &r, &m) == 3)
            {
                FrameReceived = Frame;
                Input.MouseButtons = l | (r << 1) | (m << 2);
            }
        }
        else if (strstr((char *)data, "ImMouseWheelDelta"))
        {
            float mouseWheelDelta;
            if (sscanf((char *)data, "ImMouseWheelDelta=%f", &mouseWheelDelta) == 1)
            {
                FrameReceived = Frame;
                Input.MouseWheelDelta = mouseWheelDelta * 0.01f;
            }
        }
        else if (strstr((char *)data, "ImKeyDown"))
        {
            int key, shift, ctrl;
            if (sscanf((char *)data, "ImKeyDown=%d,%d,%d", &key, &shift, &ctrl) == 3)
            {
                //update key states
                FrameReceived = Frame;
                Input.KeyShift = shift > 0;
                Input.KeyCtrl = ctrl > 0;
                mapRemoteKey(&key, Input.KeyCtrl);
                Input.KeysDown[key] = true;
            }
        }
        else if (strstr((char *)data, "ImKeyUp"))
        {
            int key;
            if (sscanf((char *)data, "ImKeyUp=%d", &key) == 1)
            {
                //update key states
                FrameReceived = Frame;
                Input.KeysDown[key] = false;
                Input.KeyShift = false;
                Input.KeyCtrl = false;
            }
        }
        else if (strstr((char *)data, "ImKeyPress"))
        {
            unsigned int key;
            if (sscanf((char *)data, "ImKeyPress=%d", &key) == 1)
                ImGui::GetIO().AddInputCharacter(ImWchar(key));
        }
        else if (strstr((char *)data, "ImClipboard="))
        {
            char *clipboard = &((char *)data)[strlen("ImClipboard=")];
            ImGui::GetIO().SetClipboardTextFn(nullptr, clipboard);
        }
        else if (strstr((char *)data, "ImResize="))
        {
            int x, y;
            if (sscanf((char *)data, "ImResize=%d,%d", &x, &y) == 2)
            {
                ImGuiIO& io = ImGui::GetIO();
                io.DisplaySize = ImVec2((float)x, (float)y);
            }
        }
        break;
        // Binary message
    case WebSocketServer::Binary:
        //printf("ImGui client: Binary message received (%d bytes)\n", size);
        break;
        // Disconnect
    case WebSocketServer::Disconnect:
        printf("ImGui client: DISCONNECT\n");
        ClientActive = false;
        break;
        // Ping
    case WebSocketServer::Ping:
        printf("ImGui client: PING\n");
        break;
        // Pong
    case WebSocketServer::Pong:
        printf("ImGui client: PONG\n");
        break;
    default:
        assert(0);
        break;
    }
}

} // namespace
