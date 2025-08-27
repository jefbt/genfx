#include "MainFrame.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <thread>
#include <future>
#include <sstream>

#include "FFmpegPipe.h"
// PNG encoder for frames
#include "PngEncoder.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

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
    if (m_codecChoice) { m_codecChoice->SetBackgroundColour(bg); m_codecChoice->SetForegroundColour(fg); }
    if (m_saveLogs) { m_saveLogs->SetBackgroundColour(bg); m_saveLogs->SetForegroundColour(fg); }
    if (m_sizeMinSlider) { m_sizeMinSlider->SetBackgroundColour(bg); m_sizeMinSlider->SetForegroundColour(fg); }
    if (m_sizeMaxSlider) { m_sizeMaxSlider->SetBackgroundColour(bg); m_sizeMaxSlider->SetForegroundColour(fg); }
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
    right->Add(new wxStaticText(this, wxID_ANY, "Effect"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
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

    // Particle size range sliders (in pixels)
    right->Add(new wxStaticText(this, wxID_ANY, "Particle size min (px)"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_sizeMinSlider = new wxSlider(this, wxID_ANY, 1, 1, 50, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_sizeMinSlider->Bind(wxEVT_SLIDER, &MainFrame::OnSizeRangeChanged, this);
    right->Add(m_sizeMinSlider, 0, wxEXPAND|wxALL, 8);

    right->Add(new wxStaticText(this, wxID_ANY, "Particle size max (px)"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_sizeMaxSlider = new wxSlider(this, wxID_ANY, 8, 1, 100, wxDefaultPosition, wxDefaultSize, wxSL_LABELS);
    m_sizeMaxSlider->Bind(wxEVT_SLIDER, &MainFrame::OnSizeRangeChanged, this);
    right->Add(m_sizeMaxSlider, 0, wxEXPAND|wxALL, 8);

    // Background radio: Black / Gray / White
    const wxString bgChoices[] = {"Black", "Gray", "White"};
    m_bgRadio = new wxRadioBox(this, wxID_ANY, "Preview background", wxDefaultPosition, wxDefaultSize,
                               WXSIZEOF(bgChoices), bgChoices, 1, wxRA_SPECIFY_COLS);
    m_bgRadio->SetSelection(1); // Gray default
    m_bgRadio->Bind(wxEVT_RADIOBOX, &MainFrame::OnBgChanged, this);
    right->Add(m_bgRadio, 0, wxEXPAND|wxALL, 8);

    // Codec selection
    right->Add(new wxStaticText(this, wxID_ANY, "Codec"), 0, wxLEFT|wxRIGHT|wxTOP, 8);
    m_codecChoice = new wxChoice(this, wxID_ANY);
    m_codecChoice->Append("VP8 (dual-stream alpha)");
    m_codecChoice->Append("VP9 (single-stream alpha)");
    m_codecChoice->Append("UT Video RGBA (lossless)");
    m_codecChoice->SetSelection(0);
    m_codecChoice->Enable(true);
    right->Add(m_codecChoice, 0, wxEXPAND|wxALL, 8);

    // Save logs checkbox
    m_saveLogs = new wxCheckBox(this, wxID_ANY, "Save FFmpeg logs");
    m_saveLogs->SetValue(true);
    right->Add(m_saveLogs, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 8);

    // Export button
    m_exportBtn = new wxButton(this, wxID_ANY, "Export (4 sizes)");
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
    float sizeMin = m_sizeMinSlider ? (float)m_sizeMinSlider->GetValue() : 1.0f;
    float sizeMax = m_sizeMaxSlider ? (float)m_sizeMaxSlider->GetValue() : 8.0f;

    std::string effect = "golden-lights";
    if (m_effectChoice) effect = m_effectChoice->GetStringSelection().ToStdString();

    m_renderer = std::make_unique<Renderer>(m_previewW, m_previewH);
    m_renderer->SetEffect(effect);
    m_renderer->SetDuration(durationSec);
    m_renderer->SetFPS(fps);
    m_renderer->SetDensity(density);
    m_renderer->SetSpeed(speed);
    m_renderer->SetSizeMin(sizeMin);
    m_renderer->SetSizeMax(sizeMax);
    m_renderer->Setup();
}

void MainFrame::OnSizeRangeChanged(wxCommandEvent&) {
    if (!m_sizeMinSlider || !m_sizeMaxSlider) return;
    // Keep min <= max by clamping the other slider if needed
    int minv = m_sizeMinSlider->GetValue();
    int maxv = m_sizeMaxSlider->GetValue();
    if (minv > maxv) {
        // Prefer expanding max to match min if possible within range
        m_sizeMaxSlider->SetValue(minv);
        maxv = minv;
    }
    RecreateRenderer();
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

    // Snapshot preferences before starting background work
    bool saveLogsPref = m_saveLogs ? m_saveLogs->GetValue() : true;
    int codecSel = m_codecChoice ? m_codecChoice->GetSelection() : 0; // 0=VP8 dual, 1=VP9 single, 2=UT RGBA

    // Run export off the UI thread
    auto fut = std::async(std::launch::async, [=]() {
        for (auto s : sizes) {
            // Determine output path/extension by codec
            std::string baseName = BuildFileName(effect, s.w, s.h); // default ends with .webm
            if (codecSel == 2) { // UT Video RGBA -> .mov
                auto pos = baseName.rfind(".webm");
                if (pos != std::string::npos) baseName.replace(pos, 5, ".mov");
                else baseName += ".mov";
            }
            std::string outPath = (wxFileName(outDir, baseName)).GetFullPath().ToStdString();
            Renderer r(s.w, s.h);
            r.SetEffect(effect);
            r.SetDuration(durationSec);
            r.SetFPS(fps);
            r.SetDensity(m_densitySlider->GetValue());
            r.SetSpeed(speed);
            // Forward particle size settings
            float sizeMin = 1.0f, sizeMax = 8.0f;
            if (m_sizeMinSlider) sizeMin = (float)m_sizeMinSlider->GetValue();
            if (m_sizeMaxSlider) sizeMax = (float)m_sizeMaxSlider->GetValue();
            r.SetSizeMin(sizeMin);
            r.SetSizeMax(sizeMax);
            r.Setup();

            // Prepare frames directory
            std::filesystem::path framesDir = std::filesystem::path(outDir.ToStdString()) /
                                              (ToKebabCase(effect) + "-" + std::to_string(s.w) + "x" + std::to_string(s.h) + "_frames");
            std::error_code ec;
            std::filesystem::create_directories(framesDir, ec);
            if (ec) {
                return std::string("Failed to create frames directory: ") + framesDir.string();
            }

            auto write_png_file = [&](int index, const std::vector<uint8_t>& pngBytes) -> bool {
                char name[64];
                std::snprintf(name, sizeof(name), "frame-%06d.png", index);
                std::filesystem::path fpath = framesDir / name;
                std::ofstream ofs(fpath, std::ios::binary);
                if (!ofs) return false;
                ofs.write(reinterpret_cast<const char*>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
                return ofs.good();
            };

            int totalFrames = durationSec * fps;
            int cross = std::clamp(fps, 1, totalFrames/2); // up to 1s of crossfade
            std::vector<std::vector<uint8_t>> firstFrames;
            firstFrames.reserve(cross);

            for (int i = 0; i < totalFrames; ++i) {
                r.RenderNextFrame();
                const auto& buf = r.GetFrameBuffer();
                if (i < cross) {
                    firstFrames.emplace_back(buf.begin(), buf.end());
                }

                std::vector<uint8_t> png;
                if (i >= totalFrames - cross) {
                    int k = i - (totalFrames - cross);
                    float t = float(k + 1) / float(cross);
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
                    if (!EncodePNGFromBGRA(blended.data(), s.w, s.h, png)) {
                        return std::string("Failed to encode PNG (blend) for ") + outPath;
                    }
                } else {
                    if (!EncodePNGFromBGRA(buf.data(), s.w, s.h, png)) {
                        return std::string("Failed to encode PNG for ") + outPath;
                    }
                }

                if (!write_png_file(i+1, png)) {
                    return std::string("Failed to write PNG file for frame ") + std::to_string(i+1) + " to " + framesDir.string();
                }
            }

            // Build ffmpeg command to encode sequence with selected codec
            auto quote = [](const std::string& s){ return std::string("\"") + s + "\""; };
            std::string cmd;
            cmd.reserve(1024);
            // Build ffmpeg base with selected log level
            cmd += "ffmpeg -hide_banner ";
            if (saveLogsPref) cmd += "-loglevel debug "; else cmd += "-loglevel warning ";
            cmd += "-y ";
            cmd += "-framerate "; cmd += std::to_string(fps); cmd += ' ';
            std::string pattern = (framesDir / "frame-%06d.png").string();
            cmd += "-i "; cmd += quote(pattern); cmd += ' ';
            if (codecSel == 2) {
                // UT Video RGBA (lossless), single stream with native alpha in MOV
                cmd += "-an -vf format=rgba ";
                cmd += "-c:v utvideo -pix_fmt rgba ";
            } else if (codecSel == 1) {
                // VP9 single-stream alpha (yuva420p). Note: some builds may drop alpha.
                cmd += "-an -vf format=rgba,format=yuva420p ";
                cmd += "-c:v libvpx-vp9 -pix_fmt yuva420p ";
                cmd += "-b:v 0 -crf 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 1 -lag-in-frames 25 ";
                cmd += "-metadata:s:v:0 alpha_mode=1 ";
            } else {
                // VP8 dual-stream alpha: color in v:0 (yuv420p), alpha in v:1 (gray/yuv420p)
                // This path is widely supported and guarantees transparency.
                cmd += "-an -filter_complex \"[0:v]format=rgba,split=2[c][a];[c]format=yuv420p[color];[a]alphaextract,format=gray[alpha]\" ";
                cmd += "-map [color] -map [alpha] ";
                cmd += "-c:v:0 libvpx -pix_fmt yuv420p -b:v:0 0 -crf:v:0 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 0 ";
                cmd += "-c:v:1 libvpx -pix_fmt yuv420p -b:v:1 0 -crf:v:1 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 0 ";
                cmd += "-metadata:s:v:0 alpha_mode=1 -metadata:s:v:1 alpha_mode=1 ";
            }
            cmd += quote(outPath);

            if (saveLogsPref) {
                std::string ffstdoutPath = outPath;
                {
                    // Place output log next to media file
                    auto pos = ffstdoutPath.find_last_of('.') ;
                    if (pos != std::string::npos) ffstdoutPath.insert(pos, "-ffmpeg-output");
                    else ffstdoutPath += ".ffmpeg-output";
                    ffstdoutPath += ".txt";
                }
                // Redirect stdout/stderr to a text file
                cmd += " 1>"; cmd += quote(ffstdoutPath); cmd += " 2>&1";
            }

            std::cout << "FFmpeg sequence command: " << cmd << std::endl;
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                return std::string("ffmpeg failed with code ") + std::to_string(rc) + ". Check ffmpeg-*.log.";
            }

            // Cleanup frames on success
            std::filesystem::remove_all(framesDir, ec);
        }
        return std::string();
    });

    // Poll completion on UI thread
    std::thread([this, f = std::move(fut)]() mutable {
        std::string err = f.get();
        wxTheApp->CallAfter([this, err]() {
            m_exportBtn->Enable(true);
            if (!err.empty()) {
                wxMessageBox(err, "Export error", wxOK | wxICON_ERROR, this);
            } else {
                wxMessageBox("Export finished!", "Success", wxOK | wxICON_INFORMATION, this);
            }
        });
    }).detach();
}
