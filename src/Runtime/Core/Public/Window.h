#pragma once



class Window
{
public:
    Window();
    ~Window();
    
    int Open(const char* title, int w, int h);
    void Render();
    void Shutdown();
    
    void SetTitle(const char* title);

private:
    struct SDL_Window* window;
    struct SDL_GPUDevice* gpuDevice;
    
    bool bInitialized;
};
