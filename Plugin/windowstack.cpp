//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : windowstack.cpp
//
// -------------------------------------------------------------------------
// A
//              _____           _      _     _ _
//             /  __ \         | |    | |   (_) |
//             | /  \/ ___   __| | ___| |    _| |_ ___
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/
//              \____/\___/ \__,_|\___\_____/_|\__\___|
//
//                                                  F i l e
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#include "windowstack.h"

WindowStack::WindowStack(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
    , m_selection(NULL)
{
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_mainSizer);
    m_windows.clear();
}

WindowStack::~WindowStack() { Clear(); }

void WindowStack::DoSelect(wxWindow* win)
{
    Freeze();
    // remove the old selection
    if(m_selection) {
        m_mainSizer->Detach(m_selection);
        m_selection->Hide();
    }
    if(win) {
        m_mainSizer->Add(win, 1, wxEXPAND);
        win->Show();
        m_selection = win;
    } else {
        m_selection = NULL;
    }
    m_mainSizer->Layout();
    Thaw();
}

void WindowStack::Select(wxWindow* win)
{
    if(!Contains(win)) return;
    DoSelect(win);
}

void WindowStack::SelectNone() { DoSelect(NULL); }

void WindowStack::Clear()
{
    SelectNone();
    m_selection = NULL;

    std::set<wxWindow*>::iterator iter = m_windows.begin();
    for(; iter != m_windows.end(); iter++) {
        (*iter)->Destroy();
    }
    m_windows.clear();
}

bool WindowStack::Remove(wxWindow* win)
{
    if(!Contains(win)) return false;
    m_windows.erase(win);

    if(win == m_selection) {
        SelectNone();
    }
    return true;
}

bool WindowStack::Delete(wxWindow* win)
{
    if(!Remove(win)) return false;
    win->Destroy();
    return true;
}

bool WindowStack::Add(wxWindow* win, bool select)
{
    if(!win || Contains(win)) {
        return false;
    }
    win->Reparent(this);
    m_windows.insert(win);
    if(select) {
        DoSelect(win);
    } else {
        win->Hide();
    }
    return true;
}

bool WindowStack::Contains(wxWindow* win)
{
    return m_windows.count(win);
}
