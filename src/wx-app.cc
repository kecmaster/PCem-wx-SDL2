#include "wx-app.h"

#include <wx/xrc/xmlres.h>
#include <wx/event.h>
#include "wx-utils.h"

extern "C"
{
        int wx_start(void*);
        int wx_stop(void*);
        void wx_show(void*);
        void wx_handle_command(void*, int, int);
        int window_remember;
        void stop_emulation_confirm();
}

int wx_window_x = 0;
int wx_window_y = 0;

int (*wx_keydown_func)(void* window, void* event, int keycode, int modifiers);
int (*wx_keyup_func)(void* window, void* event, int keycode, int modifiers);
void (*wx_idle_func)(void* window, void* event);

extern void InitXmlResource();

wxDEFINE_EVENT(WX_CALLBACK_EVENT, CallbackEvent);
wxDEFINE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_SHOW_WINDOW_EVENT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(App);

App::App()
{
        this->frame = NULL;
}

bool App::OnInit()
{
        wxImage::AddHandler( new wxPNGHandler );
        wxXmlResource::Get()->InitAllHandlers();
//        if (!wxXmlResource::Get()->Load("src/pc.xrc"))
//        {
//                std::cout << "Could not load resource file" << std::endl;
//                return false;
//        }
        InitXmlResource();

        frame = new Frame(this, "PCem Machine", wxPoint(50, 50),
                        wxSize(DEFAULT_WINDOW_WIDTH, 200));
        frame->Start();
        return true;
}

int App::OnRun()
{
        return wxApp::OnRun();
}

int App::FilterEvent(wxEvent& event)
{
        int type = event.GetEventType();
        if (type == wxEVT_KEY_DOWN && wx_keydown_func)
        {
                wxKeyEvent e = (wxKeyEvent&)event;
                if (wx_keydown_func(this, &e, e.GetKeyCode(), e.GetModifiers()))
                        return Event_Processed;
        }
        else if (type == wxEVT_KEY_UP && wx_keyup_func)
        {
                wxKeyEvent e = (wxKeyEvent&)event;
                if (wx_keyup_func(this, &e, e.GetKeyCode(), e.GetModifiers()))
                        return Event_Processed;
        }

        return Event_Skip;
}



Frame::Frame(App* app, const wxString& title, const wxPoint& pos,
                const wxSize& size) :
                wxFrame(NULL, wxID_ANY, title, pos, size, wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER))
{
        this->closing = false;
        this->statusPane = new StatusPane(this);
        SetMenuBar(wxXmlResource::Get()->LoadMenuBar(wxT("main_menu")));
        SetToolBar(wxXmlResource::Get()->LoadToolBar(this, wxT("tool_bar")));

        Bind(wxEVT_CLOSE_WINDOW, &Frame::OnClose, this);
        Bind(wxEVT_MENU, &Frame::OnCommand, this);
        Bind(wxEVT_TOOL, &Frame::OnCommand, this);
        Bind(wxEVT_MOVE, &Frame::OnMoveWindow, this);
        Bind(wxEVT_IDLE, &Frame::OnIdle, this);
        Bind(WX_SHOW_WINDOW_EVENT, &Frame::OnShowWindowEvent, this);
        Bind(WX_EXIT_EVENT, &Frame::OnExitEvent, this);
        Bind(WX_EXIT_COMPLETE_EVENT, &Frame::OnExitCompleteEvent, this);
        Bind(WX_STOP_EMULATION_EVENT, &Frame::OnStopEmulationEvent, this);
        Bind(WX_CALLBACK_EVENT, &Frame::OnCallbackEvent, this);

        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(statusPane, 1, wxEXPAND);
        SetSizer(sizer);

        statusTimer = new StatusTimer(statusPane);
}

void Frame::Start()
{
        if (wx_start(this))
        {
                Show();
                statusTimer->Start();
                if (window_remember)
                        SetPosition(wxPoint(wx_window_x, wx_window_y));
                else
                        CenterOnScreen();
        }
        else
                Quit(0);

}

void Frame::OnCallbackEvent(CallbackEvent& event)
{
        WX_CALLBACK callback = event.GetCallback();
        callback();
}

void Frame::OnIdle(wxIdleEvent& event)
{
	if (wx_idle_func)
		wx_idle_func(this, &event);
}

void Frame::OnStopEmulationEvent(wxCommandEvent& event)
{
        wx_handle_command(this, XRCID("TOOLBAR_STOP"), 0); /* Simulate toolbar stop */
}

void Frame::OnShowWindowEvent(wxCommandEvent& event)
{
        wxWindow* window = (wxWindow*)event.GetEventObject();
        int shown = window->IsShown();
        int value = event.GetInt();
        if (value < 0)
                window->Show(!shown);
        else
                window->Show(value > 0);
        if (!shown)
                window->Refresh();
}

void Frame::OnCommand(wxCommandEvent& event)
{
        wx_handle_command(this, event.GetId(), event.IsChecked());
}

void Frame::OnClose(wxCloseEvent& event)
{
        wx_exit(this, 0);
}

void Frame::OnMoveWindow(wxMoveEvent& event)
{
        if (window_remember) {
                wx_window_x = event.GetPosition().x;
                wx_window_y = event.GetPosition().y;
        }
}

/* Exit */

void Frame::Quit(bool stop_emulator)
{
        if (closing)
                return;
        closing = true;
        if (stop_emulator)
        {
                if (!wx_stop(this))
                {
                        // cancel quit
                        closing = false;
                        return;
                }
                statusTimer->Stop();
                delete statusTimer;
        }
        Destroy();
}

void Frame::OnExitEvent(wxCommandEvent& event)
{
        if (closing)
                return;
        closing = true;
        CExitThread* exitThread = new CExitThread(this);
        exitThread->Run();
}

void Frame::OnExitCompleteEvent(wxCommandEvent& event)
{
        if (event.GetInt())
                Destroy();
        else
                closing = false;
}

CExitThread::CExitThread(Frame* frame)
{
	this->frame = frame;
}

wxThread::ExitCode CExitThread::Entry()
{
        wxCommandEvent* event = new wxCommandEvent(WX_EXIT_COMPLETE_EVENT, wxID_ANY);
        event->SetInt(wx_stop(frame));
        wxQueueEvent(frame, event);
	return 0;
}
