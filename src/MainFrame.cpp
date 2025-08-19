#include "MainFrame.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <thread>
#include <future>
#include <sstream>

#include "FFmpegPipe.h"

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TIMER(wxID_ANY, MainFrame::OnTimer)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(980, 640)), m_timer(this) {
    BuildUI();
    RecreateRenderer();
    m_timer.Start(1000/30); // fixed 30 FPS preview

    // Apply dark theme defaults
    wxColour bg(24,24,24); wxColour fg(230,230,230);
    SetBackgroundColour(bg); SetForegroundColour(fg);
    if (m_preview) m_preview->SetBackgroundColour(bg);
    if (m_effectChoice) { m_effectChoice->SetBackgroundColour(bg); m_effectChoice->SetForegroundColour(fg); }
    if (m_durationSlider) { m_durationSlider->SetBackgroundColour(bg); m_durationSlider->SetForegroundColour(fg); }
    if (m_speedSlider) { m_speedSlider->SetBackgroundColour(bg); m_speedSlider->SetForegroundColour(fg); }
    if (m_densitySlider) { m_densitySlider->SetBackgroundColour(bg); m_densitySlider->SetForegroundColour(fg); }
    if (m_bgRadio) { m_bgRadio->SetBackgroundColour(bg); m_bgRadio->SetForegroundColour(fg); }
    if (m_exportBtn) { m_exportBtn->SetBackgroundColour(bg); m_exportBtn->SetForegroundColour(fg); }
}

void MainFrame::OnDurationChanged(wxCommandEvent&) {
    // Duration changes total frames and effect setup; recreate for consistency
    RecreateRenderer();
}

void MainFrame::OnSpeedChanged(wxCommandEvent&) {
    if (!m_renderer) return;
    float speed = m_speedSlider ? (m_speedSlider->GetValue() / 100.0f) : 1.0f;
    m_renderer->SetSpeed(speed);
}

void MainFrame::OnDensityChanged(wxCommandEvent&) {
    // Density affects number of particles/drops; recreate effect
    RecreateRenderer();
}

void MainFrame::OnBgChanged(wxCommandEvent&) {
    if (!m_preview || !m_bgRadio) return;
    int sel = m_bgRadio->GetSelection();
    PreviewPanel::BackgroundMode mode = PreviewPanel::BackgroundMode::Gray;
    if (sel == 0) mode = PreviewPanel::BackgroundMode::Black;
    else if (sel == 1) mode = PreviewPanel::BackgroundMode::Gray;
    else if (sel == 2) mode = PreviewPanel::BackgroundMode::White;
    m_preview->SetBackgroundMode(mode);
}

void MainFrame::BuildUI() {
    auto* root = new wxBoxSizer(wxHORIZONTAL);

    // Left: preview
    auto* left = new wxBoxSizer(wxVERTICAL);
    m_preview = new PreviewPanel(this, nullptr);
    left->Add(m_preview, 1, wxEXPAND|wxALL, 8);

    // Right: controls
    auto* right = new wxBoxSizer(wxVERTICAL);

    // Effect choice
    right->Add(new wxStaticText(this, wxID_ANY, "Efeito"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_effectChoice = new wxChoice(this, wxID_ANY);
    m_effectChoice->Append("black-noise");
    m_effectChoice->Append("golden-lights");
    m_effectChoice->Append("rain");
    m_effectChoice->Append("snow");
    m_effectChoice->Append("fireflies");
    m_effectChoice->SetSelection(1);
    m_effectChoice->Bind(wxEVT_CHOICE, &MainFrame::OnEffectChanged, this);
    right->Add(m_effectChoice, 0, wxEXPAND|wxALL, 8);

    // Duration slider (10..20 s)
    right->Add(new wxStaticText(this, wxID_ANY, "Duration (s)"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_durationSlider = new wxSlider(this, wxID_ANY, 12, 10, 20, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_durationSlider->Bind(wxEVT_SLIDER, &MainFrame::OnDurationChanged, this);
    right->Add(m_durationSlider, 0, wxEXPAND|wxALL, 8);

    // Speed slider (0.1x .. 3.0x)
    right->Add(new wxStaticText(this, wxID_ANY, "Speed"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_speedSlider = new wxSlider(this, wxID_ANY, 100, 10, 300, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_speedSlider->Bind(wxEVT_SLIDER, &MainFrame::OnSpeedChanged, this);
    right->Add(m_speedSlider, 0, wxEXPAND|wxALL, 8);

    // Density slider (1..100)
    right->Add(new wxStaticText(this, wxID_ANY, "Density"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_densitySlider = new wxSlider(this, wxID_ANY, 50, 1, 100, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_densitySlider->Bind(wxEVT_SLIDER, &MainFrame::OnDensityChanged, this);
    right->Add(m_densitySlider, 0, wxEXPAND|wxALL, 8);

    // Background radio: Black / Gray / White
    const wxString bgChoices[] = {"Black", "Gray", "White"};
    m_bgRadio = new wxRadioBox(this, wxID_ANY, "Preview background", wxDefaultPosition, wxDefaultSize,
                               WXSIZEOF(bgChoices), bgChoices, 1, wxRA_SPECIFY_COLS);
    m_bgRadio->SetSelection(1); // Gray default
    m_bgRadio->Bind(wxEVT_RADIOBOX, &MainFrame::OnBgChanged, this);
    right->Add(m_bgRadio, 0, wxEXPAND|wxALL, 8);

    // Export button
    m_exportBtn = new wxButton(this, wxID_ANY, "Export (4 sizes, VP9 + alpha)");
    m_exportBtn->Bind(wxEVT_BUTTON, &MainFrame::OnExport, this);
    right->AddStretchSpacer(1);
    right->Add(m_exportBtn, 0, wxEXPAND|wxALL, 8);

    root->Add(left, 2, wxEXPAND);
    root->Add(right, 1, wxEXPAND);
    SetSizer(root);
}

void MainFrame::RecreateRenderer() {
    int durationSec = m_durationSlider ? m_durationSlider->GetValue() : 12;
    int fps = 30; // fixed preview/export fps
    int density = m_densitySlider ? m_densitySlider->GetValue() : 50;
    float speed = m_speedSlider ? (m_speedSlider->GetValue() / 100.0f) : 1.0f;

    std::string effect = "golden-lights";
    if (m_effectChoice) effect = m_effectChoice->GetStringSelection().ToStdString();

    m_renderer = std::make_unique<Renderer>(m_previewW, m_previewH);
    m_renderer->SetEffect(effect);
    m_renderer->SetDuration(durationSec);
    m_renderer->SetFPS(fps);
    m_renderer->SetDensity(density);
    m_renderer->SetSpeed(speed);
    m_renderer->Setup();
}

void MainFrame::OnEffectChanged(wxCommandEvent&) {
    RecreateRenderer();
}

void MainFrame::OnTimer(wxTimerEvent&) {
    if (!m_renderer) return;

    m_renderer->RenderNextFrame();
    m_preview->SetFrameBuffer(&m_renderer->GetFrameBuffer(), m_renderer->GetWidth(), m_renderer->GetHeight());
}

static std::string BuildFileName(const std::string& effectName, int w, int h) {
    std::string kebab = ToKebabCase(effectName);
    std::ostringstream oss;
    oss << kebab << "-" << w << "x" << h << ".webm";
    return oss.str();
}

void MainFrame::OnExport(wxCommandEvent&) {
    if (!m_renderer) return;
    m_exportBtn->Enable(false);

    int durationSec = m_renderer->GetDuration();
    int fps = 30; // fixed export fps
    std::string effect = m_renderer->GetEffectName();
    float speed = m_renderer->GetSpeed();

    std::vector<SizeI> sizes = { {1280,720}, {720,1280}, {1920,1080}, {1080,1920} };

    // Ask folder
    wxDirDialog dlg(this, "Select output folder", wxEmptyString, wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) { m_exportBtn->Enable(true); return; }
    wxString outDir = dlg.GetPath();

    // Run export off the UI thread
    auto fut = std::async(std::launch::async, [=]() {
        for (auto s : sizes) {
            std::string outPath = (wxFileName(outDir, BuildFileName(effect, s.w, s.h))).GetFullPath().ToStdString();
            Renderer r(s.w, s.h);
            r.SetEffect(effect);
            r.SetDuration(durationSec);
            r.SetFPS(fps);
            r.SetDensity(m_densitySlider->GetValue());
            r.SetSpeed(speed);
            r.Setup();

            FFmpegPipe pipe;
            if (!pipe.Open(outPath, s.w, s.h, fps)) {
                return std::string("Failed to open FFmpeg for ") + outPath;
            }

            int totalFrames = durationSec * fps;
            // Crossfade the final 1 second into the first frames to make a seamless loop
            int cross = std::clamp(fps, 1, totalFrames/2); // up to 1s of crossfade

            // Store the first 'cross' frames
            std::vector<std::vector<uint8_t>> firstFrames;
            firstFrames.reserve(cross);

            for (int i = 0; i < totalFrames; ++i) {
                r.RenderNextFrame();
                const auto& buf = r.GetFrameBuffer();
                if (i < cross) {
                    firstFrames.emplace_back(buf.begin(), buf.end());
                }

                // For the last 'cross' frames, blend toward the corresponding first frame
                if (i >= totalFrames - cross) {
                    int k = i - (totalFrames - cross); // 0..cross-1
                    float t = float(k + 1) / float(cross); // 0->1
                    // Blend: out = (1-t)*current + t*first
                    std::vector<uint8_t> blended(buf.size());
                    const auto& f0 = firstFrames[k];
                    const uint8_t* a = buf.data();
                    const uint8_t* b = f0.data();
                    for (size_t p = 0; p < buf.size(); p += 4) {
                        for (int c = 0; c < 4; ++c) {
                            float va = a[p + c] / 255.0f;
                            float vb = b[p + c] / 255.0f;
                            float vo = (1.0f - t) * va + t * vb;
                            blended[p + c] = (uint8_t)std::clamp(int(vo * 255.0f + 0.5f), 0, 255);
                        }
                    }
                    if (!pipe.WriteFrame(blended.data(), blended.size())) {
                        return std::string("Falha ao escrever frame (blend) para ") + outPath;
                    }
                } else {
                    if (!pipe.WriteFrame(buf.data(), buf.size())) {
                        return std::string("Falha ao escrever frame para ") + outPath;
                    }
                }
            }
            pipe.Close();
        }
        return std::string();
    });

    // Poll completion on UI thread
    std::thread([this, f = std::move(fut)]() mutable {
        std::string err = f.get();
        wxTheApp->CallAfter([this, err]() {
            m_exportBtn->Enable(true);
            if (!err.empty()) {
                wxMessageBox(err, "Erro na exportação", wxOK | wxICON_ERROR, this);
            } else {
                wxMessageBox("Exportação concluída!", "Sucesso", wxOK | wxICON_INFORMATION, this);
            }
        });
    }).detach();
}
