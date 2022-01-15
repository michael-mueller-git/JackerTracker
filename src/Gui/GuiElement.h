#pragma once

#include <opencv2/core.hpp>
#include <OISKeyboard.h>

#define GUI_LAYER_STATE 1
#define GUI_LAYER_TIMEBAR 2
#define GUI_LAYER_BUTTONS 3

class GuiElement
{
public:
    virtual void DoDraw(cv::Mat& frame)
    {
        Draw(frame);
        drawRequested = false;
    }
    virtual bool HandleInput(OIS::KeyCode c) = 0;
    virtual bool HandleMouse(int e, int x, int y, int f) = 0;
    virtual int GetLayerNum() = 0;
    virtual void AskDraw() { drawRequested = true; }
    virtual bool DrawRequested() { 
        return drawRequested;
    }

protected:
    virtual void Draw(cv::Mat& frame) = 0;
    bool drawRequested = false;
};