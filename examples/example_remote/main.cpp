// dear imgui - standalone example application for Remote
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
#include "imgui.h"

#include <chrono>
#include <thread>

void ImImpl_RenderDrawLists(ImDrawData* draw_data)
{
    // @RemoteImgui begin
    ImGui::RemoteDraw(draw_data->CmdLists, draw_data->CmdListsCount);
}

int main(int, char**)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DeltaTime = 1.0f / 60.0f;
    io.RenderDrawListsFn = ImImpl_RenderDrawLists;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    io.Fonts->TexID = nullptr;

    // @RemoteImgui begin
    ImGui::RemoteInit("0.0.0.0", 7002); // local host, local port
    //ImGui::GetStyle().WindowRounding = 0.f; // no rounding uses less bandwidth
    io.DisplaySize = ImVec2((float)1920, (float)1080);
    // @RemoteImgui end

    io.SetClipboardTextFn = ImGui::RemoteSetClipboardTextFn;

    auto last_time = std::chrono::high_resolution_clock::now();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int counter = 0;

    // Main loop
    while (!io.KeysDown[io.KeyMap[ImGuiKey_Escape]])
    {
        // Setup time step
        auto now = std::chrono::high_resolution_clock::now();
        io.DeltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_time).count();
        last_time = now;

        // @RemoteImgui begin
        ImGui::RemoteUpdate();
        if (ImGui::RemoteActive())
        {
            ImGui::RemoteInput input;
            if (ImGui::RemoteGetInput(input))
            {
                for (int i = 0; i < 256; i++)
                    io.KeysDown[i] = input.KeysDown[i];

                io.KeyCtrl = input.KeyCtrl;
                io.KeyShift = input.KeyShift;
                io.MousePos = input.MousePos;
                io.MouseDown[0] = (input.MouseButtons & 1);
                io.MouseDown[1] = (input.MouseButtons & 2) != 0;
                io.MouseDown[2] = (input.MouseButtons & 4) != 0;
                io.MouseWheel += input.MouseWheelDelta;// / highDpiScale;

                // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
                io.KeyMap[ImGuiKey_Tab] = ImGuiKey_Tab;
                io.KeyMap[ImGuiKey_LeftArrow] = ImGuiKey_LeftArrow;
                io.KeyMap[ImGuiKey_RightArrow] = ImGuiKey_RightArrow;
                io.KeyMap[ImGuiKey_UpArrow] = ImGuiKey_UpArrow;
                io.KeyMap[ImGuiKey_DownArrow] = ImGuiKey_DownArrow;
                io.KeyMap[ImGuiKey_Home] = ImGuiKey_Home;
                io.KeyMap[ImGuiKey_End] = ImGuiKey_End;
                io.KeyMap[ImGuiKey_Delete] = ImGuiKey_Delete;
                io.KeyMap[ImGuiKey_Backspace] = ImGuiKey_Backspace;
                io.KeyMap[ImGuiKey_Enter] = 13;
                io.KeyMap[ImGuiKey_Escape] = 27;
                io.KeyMap[ImGuiKey_A] = 'a';
                io.KeyMap[ImGuiKey_C] = 'c';
                io.KeyMap[ImGuiKey_V] = 'v';
                io.KeyMap[ImGuiKey_X] = 'x';
                io.KeyMap[ImGuiKey_Y] = 'y';
                io.KeyMap[ImGuiKey_Z] = 'z';
            }

            // Start the frame
            ImGui::NewFrame();

            // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

                ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
                ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
                ImGui::Checkbox("Another Window", &show_another_window);

                ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

                if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    counter++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
            }

            // 3. Show another simple window.
            if (show_another_window)
            {
                ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
                ImGui::Text("Hello from another window!");
                ImGui::Image(ImTextureID(1), ImVec2(100, 100));
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }

            // Rendering
            ImGui::Render();
        }

        if (++counter > 10)
        {
            counter = 0;
            ImU32* color = new ImU32[100 * 100];
            float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            for (int i = 0; i < 100 * 100; i++)
            {
                color[i] = ImColor::HSV(r, 1.0f, 1.0f);
            }
            ImGui::RemoteSetTexture(color, 100, 100, ImTextureID(1));
            delete[] color;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    ImGui::DestroyContext();

    return 0;
}
