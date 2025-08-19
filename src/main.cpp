#include <wx/wx.h>
#include "MainFrame.h"

class GenfxApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;
        MainFrame* frame = new MainFrame("GenFX - Looping Effects Exporter");
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(GenfxApp);
