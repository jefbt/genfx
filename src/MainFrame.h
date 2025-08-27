#pragma once
#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/radiobox.h>
#include <wx/slider.h>
#include <wx/timer.h>
#include <wx/dcbuffer.h>
#include <wx/checkbox.h>
#include "Renderer.h"
#include "Utils.h"

class PreviewPanel : public wxPanel {
public:
    enum class BackgroundMode { Black=0, Gray=1, White=2 };
    PreviewPanel(wxWindow* parent, Renderer* renderer)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(640, 360)), m_renderer(renderer) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &PreviewPanel::OnPaint, this);
        Bind(wxEVT_SIZE, &PreviewPanel::OnSize, this);
    }

    void SetFrameBuffer(const std::vector<uint8_t>* buf, int w, int h) {
        m_buf = buf; m_w = w; m_h = h; Refresh(false);
    }

    void SetBackgroundMode(BackgroundMode m) { m_bgMode = m; Refresh(false); }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        // Fill with selected background color
        wxBrush brush;
        switch (m_bgMode) {
            case BackgroundMode::Black: brush.SetColour(wxColour(0,0,0)); break;
            case BackgroundMode::White: brush.SetColour(wxColour(255,255,255)); break;
            case BackgroundMode::Gray:
            default: brush.SetColour(wxColour(32,32,32)); break;
        }
        dc.SetBackground(brush);
        dc.Clear();
        if (!m_buf || m_w <= 0 || m_h <= 0) return;
        // Buffer is BGRA; wxImage expects RGB; we'll convert to RGBA then to wxBitmap
        wxImage img(m_w, m_h);
        unsigned char* data = img.GetData();
        if (!img.HasAlpha()) img.InitAlpha();
        unsigned char* alpha = img.GetAlpha();
        const uint8_t* src = m_buf->data();
        for (int i = 0; i < m_w * m_h; ++i) {
            uint8_t b = src[i*4 + 0];
            uint8_t g = src[i*4 + 1];
            uint8_t r = src[i*4 + 2];
            uint8_t a = src[i*4 + 3];
            data[i*3 + 0] = r;
            data[i*3 + 1] = g;
            data[i*3 + 2] = b;
            alpha[i] = a;
        }
        wxBitmap bmp(img);
        wxSize sz = GetClientSize();
        double sx = (double)sz.GetWidth() / m_w;
        double sy = (double)sz.GetHeight() / m_h;
        double s = std::min(sx, sy);
        int dw = (int)(m_w * s);
        int dh = (int)(m_h * s);
        int ox = (sz.GetWidth() - dw) / 2;
        int oy = (sz.GetHeight() - dh) / 2;
        if (dw > 0 && dh > 0) {
            wxBitmap scaled = wxBitmap(bmp.ConvertToImage().Scale(dw, dh, wxIMAGE_QUALITY_HIGH));
            // UseMask=false to ensure true alpha blending is used (mask is for 1-bit transparency)
            dc.DrawBitmap(scaled, ox, oy, false);
        }
    }
    void OnSize(wxSizeEvent&) { Refresh(false); }

    const std::vector<uint8_t>* m_buf{nullptr};
    int m_w{0}, m_h{0};
    Renderer* m_renderer{nullptr};
    BackgroundMode m_bgMode{BackgroundMode::Gray};
};

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);

private:
    void BuildUI();
    void OnTimer(wxTimerEvent&);
    void OnExport(wxCommandEvent&);
    void OnEffectChanged(wxCommandEvent&);
    void OnDurationChanged(wxCommandEvent&);
    void OnSpeedChanged(wxCommandEvent&);
    void OnDensityChanged(wxCommandEvent&);
    void OnSizeRangeChanged(wxCommandEvent&);
    void OnBgChanged(wxCommandEvent&);
    void RecreateRenderer();

    PreviewPanel* m_preview{nullptr};
    wxChoice* m_effectChoice{nullptr};
    wxSlider* m_durationSlider{nullptr};
    wxSlider* m_speedSlider{nullptr};
    wxSlider* m_densitySlider{nullptr};
    wxSlider* m_sizeMinSlider{nullptr};
    wxSlider* m_sizeMaxSlider{nullptr};
    wxRadioBox* m_bgRadio{nullptr};
    wxChoice* m_codecChoice{nullptr};
    wxButton* m_exportBtn{nullptr};
    wxCheckBox* m_saveLogs{nullptr};

    wxTimer m_timer;
    std::unique_ptr<Renderer> m_renderer;
    int m_previewW{640}, m_previewH{360};

    wxDECLARE_EVENT_TABLE();
};
