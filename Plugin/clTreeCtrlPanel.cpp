#include "clTreeCtrlPanel.h"
#include "clFileOrFolderDropTarget.h"
#include "codelite_events.h"
#include "macros.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include "clWorkspaceView.h"
#include <imanager.h>
#include "globals.h"
#include <wx/menu.h>
#include <wx/xrc/xmlres.h>
#include <wx/richmsgdlg.h>
#include "event_notifier.h"
#include "fileutils.h"
#include "ieditor.h"
#include "imanager.h"
#include <wx/wupdlock.h>
#include <wx/log.h>

clTreeCtrlPanel::clTreeCtrlPanel(wxWindow* parent)
    : clTreeCtrlPanelBase(parent)
{
    ::MSWSetNativeTheme(GetTreeCtrl());
    // Allow DnD
    SetDropTarget(new clFileOrFolderDropTarget(this));
    GetTreeCtrl()->SetDropTarget(new clFileOrFolderDropTarget(this));
    Bind(wxEVT_DND_FOLDER_DROPPED, &clTreeCtrlPanel::OnFolderDropped, this);
    GetTreeCtrl()->AddRoot(_("Folders"), wxNOT_FOUND, wxNOT_FOUND, new clTreeCtrlData(clTreeCtrlData::kRoot));
    GetTreeCtrl()->AssignImageList(m_bmpLoader.MakeStandardMimeImageList());

    EventNotifier::Get()->Bind(wxEVT_ACTIVE_EDITOR_CHANGED, &clTreeCtrlPanel::OnActiveEditorChanged, this);
}

clTreeCtrlPanel::~clTreeCtrlPanel()
{
    Unbind(wxEVT_DND_FOLDER_DROPPED, &clTreeCtrlPanel::OnFolderDropped, this);
    EventNotifier::Get()->Unbind(wxEVT_ACTIVE_EDITOR_CHANGED, &clTreeCtrlPanel::OnActiveEditorChanged, this);
}

void clTreeCtrlPanel::OnContextMenu(wxTreeEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);

    if(cd && cd->IsFolder()) {

        // Prepare a folder context menu
        wxMenu menu;
        if(IsTopLevelFolder(item)) {
            menu.Append(XRCID("tree_ctrl_close_folder"), _("Close Folder..."));
            menu.AppendSeparator();
        }

        menu.Append(XRCID("tree_ctrl_new_folder"), _("New Folder"));
        menu.Append(XRCID("tree_ctrl_new_file"), _("New File"));
        menu.AppendSeparator();
        menu.Append(XRCID("tree_ctrl_delete_folder"), _("Delete"));
        menu.AppendSeparator();
        menu.Append(XRCID("tree_ctrl_find_in_files_folder"), _("Find in Files"));
        menu.AppendSeparator();
        menu.Append(XRCID("tree_ctrl_open_containig_folder"), _("Open Containing Folder"));
        menu.Append(XRCID("tree_ctrl_open_shell_folder"), _("Open Shell"));

        // Now that we added the basic menu, let the plugin
        // adjust it
        wxArrayString files, folders;
        GetSelections(folders, files);

        clContextMenuEvent dirMenuEvent(wxEVT_CONTEXT_MENU_FOLDER);
        dirMenuEvent.SetMenu(&menu);
        dirMenuEvent.SetPath(cd->GetPath());
        EventNotifier::Get()->ProcessEvent(dirMenuEvent);

        // Connect events
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnCloseFolder, this, XRCID("tree_ctrl_close_folder"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnNewFolder, this, XRCID("tree_ctrl_new_folder"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnNewFile, this, XRCID("tree_ctrl_new_file"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnDeleteFolder, this, XRCID("tree_ctrl_delete_folder"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnFindInFilesFolder, this, XRCID("tree_ctrl_find_in_files_folder"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnOpenContainingFolder, this, XRCID("tree_ctrl_open_containig_folder"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnOpenShellFolder, this, XRCID("tree_ctrl_open_shell_folder"));
        PopupMenu(&menu);

    } else if(cd && cd->IsFile()) {
        // File context menu
        // Prepare a folder context menu
        wxMenu menu;

        menu.Append(XRCID("tree_ctrl_open_file"), _("Open"));
        menu.Append(XRCID("tree_ctrl_rename_file"), _("Rename"));
        menu.AppendSeparator();
        menu.Append(XRCID("tree_ctrl_delete_file"), _("Delete"));

        // Now that we added the basic menu, let the plugin
        // adjust it
        wxArrayString files, folders;
        GetSelections(folders, files);

        clContextMenuEvent fileMenuEvent(wxEVT_CONTEXT_MENU_FILE);
        fileMenuEvent.SetMenu(&menu);
        fileMenuEvent.SetStrings(files);
        EventNotifier::Get()->ProcessEvent(fileMenuEvent);

        // Connect events
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnOpenFile, this, XRCID("tree_ctrl_open_file"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnRenameFile, this, XRCID("tree_ctrl_rename_file"));
        menu.Bind(wxEVT_MENU, &clTreeCtrlPanel::OnDeleteFile, this, XRCID("tree_ctrl_delete_file"));
        PopupMenu(&menu);
    }
}

void clTreeCtrlPanel::OnItemActivated(wxTreeEvent& event)
{
    event.Skip();
    wxCommandEvent dummy;
    OnOpenFile(dummy);
}

void clTreeCtrlPanel::OnItemExpanding(wxTreeEvent& event)
{
    event.Skip();
    wxTreeItemId item = event.GetItem();
    CHECK_ITEM_RET(item);
    DoExpandItem(item, true);
}

void clTreeCtrlPanel::OnFolderDropped(clCommandEvent& event)
{
    const wxArrayString& folders = event.GetStrings();
    for(size_t i = 0; i < folders.size(); ++i) {
        AddFolder(folders.Item(i));
    }
    ::clGetManager()->GetWorkspaceView()->SelectPage(_("File Viewer"));
}

void clTreeCtrlPanel::DoExpandItem(const wxTreeItemId& parent, bool expand)
{
    clTreeCtrlData* cd = GetItemData(parent);
    CHECK_PTR_RET(cd);

    // we only know how to expand folders...
    if(!cd->IsFolder()) return;
    wxString folderPath = cd->GetPath();

    if(!m_treeCtrl->ItemHasChildren(parent)) return;
    // Test the first item for dummy

    wxTreeItemIdValue cookie;
    wxTreeItemId child = m_treeCtrl->GetFirstChild(parent, cookie);
    CHECK_ITEM_RET(child);

    cd = GetItemData(child);
    CHECK_PTR_RET(cd);

    // If not dummy - already expanded do nothing here
    if(!cd->IsDummy()) return;

    m_treeCtrl->Delete(child);
    cd = NULL;

    // Get the top level folders
    wxDir dir(folderPath);
    if(!dir.IsOpened()) return;
    wxBusyCursor bc;
    wxString filename;
    bool cont = dir.GetFirst(&filename, wxEmptyString);
    while(cont) {
        wxFileName fullpath(folderPath, filename);
        if(wxFileName::DirExists(fullpath.GetFullPath())) {
            // a folder
            DoAddFolder(parent, fullpath.GetFullPath());
        } else {
            DoAddFile(parent, fullpath.GetFullPath());
        }
        cont = dir.GetNext(&filename);
    }

    // Sort the parent
    if(GetTreeCtrl()->ItemHasChildren(parent)) {
        GetTreeCtrl()->SortChildren(parent);
        if(expand) {
            GetTreeCtrl()->Expand(parent);
        }
        SelectItem(parent);
    }
}

clTreeCtrlData* clTreeCtrlPanel::GetItemData(const wxTreeItemId& item)
{
    CHECK_ITEM_RET_NULL(item);
    clTreeCtrlData* cd = dynamic_cast<clTreeCtrlData*>(m_treeCtrl->GetItemData(item));
    return cd;
}

void clTreeCtrlPanel::AddFolder(const wxString& path)
{
    wxTreeItemId itemFolder = DoAddFolder(GetTreeCtrl()->GetRootItem(), path);
    DoExpandItem(itemFolder, false);
}

wxTreeItemId clTreeCtrlPanel::DoAddFile(const wxTreeItemId& parent, const wxString& path)
{
    wxFileName filename(path);
    clTreeCtrlData* cd = new clTreeCtrlData(clTreeCtrlData::kFile);
    cd->SetPath(filename.GetFullPath());

    int imgIdx = m_bmpLoader.GetMimeImageId(filename.GetFullName());
    if(imgIdx == wxNOT_FOUND) {
        imgIdx = m_bmpLoader.GetMimeImageId(FileExtManager::TypeText);
    }
    wxTreeItemId fileItem = GetTreeCtrl()->AppendItem(parent, filename.GetFullName(), imgIdx, imgIdx, cd);
    // Add this entry to the index
    clTreeCtrlData* parentData = GetItemData(parent);
    if(parentData->GetIndex()) {
        parentData->GetIndex()->Add(filename.GetFullName(), fileItem);
    }
    return fileItem;
}

wxTreeItemId clTreeCtrlPanel::DoAddFolder(const wxTreeItemId& parent, const wxString& path)
{
    // if we already have this folder opened, dont re-add it
    wxArrayString topFolders;
    wxArrayTreeItemIds topFoldersItems;
    GetTopLevelFolders(topFolders, topFoldersItems);
    int where = topFolders.Index(path);
    if(where != wxNOT_FOUND) {
        return topFoldersItems.Item(where);
    }

    // Add the folder
    clTreeCtrlData* cd = new clTreeCtrlData(clTreeCtrlData::kFolder);
    cd->SetPath(path);

    wxFileName filename(path, "");

    wxString displayName;
    if(filename.GetDirCount() && GetTreeCtrl()->GetRootItem() != parent) {
        displayName << filename.GetDirs().Last();
    } else {
        displayName << path;
    }
    int imgIdx = m_bmpLoader.GetMimeImageId(FileExtManager::TypeFolder);
    wxTreeItemId itemFolder = GetTreeCtrl()->AppendItem(parent, displayName, imgIdx, imgIdx, cd);

    // Add this entry to the index
    clTreeCtrlData* parentData = GetItemData(parent);
    if(parentData->GetIndex()) {
        parentData->GetIndex()->Add(displayName, itemFolder);
    }

    // Append the dummy item
    GetTreeCtrl()->AppendItem(itemFolder, "Dummy", -1, -1, new clTreeCtrlData(clTreeCtrlData::kDummy));
    return itemFolder;
}

void clTreeCtrlPanel::GetSelections(wxArrayString& folders, wxArrayString& files)
{
    wxArrayTreeItemIds d1, d2;
    GetSelections(folders, d1, files, d2);
}

TreeItemInfo clTreeCtrlPanel::GetSelectedItemInfo()
{
    TreeItemInfo info;
    wxArrayString folders, files;
    GetSelections(folders, files);

    folders.insert(folders.end(), files.begin(), files.end());
    if(folders.empty()) return info;

    info.m_paths = folders;
    info.m_item = wxTreeItemId();
    return info;
}

void clTreeCtrlPanel::OnCloseFolder(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    CHECK_ITEM_RET(item);
    if(!IsTopLevelFolder(item)) return;
    GetTreeCtrl()->Delete(item);
}

bool clTreeCtrlPanel::IsTopLevelFolder(const wxTreeItemId& item)
{
    clTreeCtrlData* cd = GetItemData(item);
    return (cd && cd->IsFolder() && GetTreeCtrl()->GetItemParent(item) == GetTreeCtrl()->GetRootItem());
}

void clTreeCtrlPanel::OnDeleteFolder(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);
    CHECK_PTR_RET(cd);
    CHECK_COND_RET(cd->IsFolder());

    wxString message;
    message << _("Are you sure you want to delete folder:\n'") << cd->GetPath() << _("'");

    wxRichMessageDialog dialog(EventNotifier::Get()->TopFrame(),
                               message,
                               _("Confirm"),
                               wxYES_NO | wxCANCEL | wxNO_DEFAULT | wxCENTER | wxICON_WARNING);
    dialog.SetYesNoLabels(_("Delete Folder"), _("Don't Delete"));
    if(dialog.ShowModal() == wxID_YES) {
        if(wxFileName::Rmdir(cd->GetPath(), wxPATH_RMDIR_RECURSIVE)) {
            // Remove this item from its parent cache
            UpdateItemDeleted(item);
            // Remove the item from the UI
            GetTreeCtrl()->Delete(item);
        }
    }
}

void clTreeCtrlPanel::OnNewFile(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);
    CHECK_PTR_RET(cd);
    CHECK_COND_RET(cd->IsFolder());

    wxString filename =
        ::clGetTextFromUser(_("New File"), _("Set the file name:"), "Untitled.txt", wxStrlen("Untitled"));
    if(filename.IsEmpty()) return; // user cancelled

    wxFileName file(cd->GetPath(), filename);

    // Write the file content
    if(!FileUtils::WriteFileContent(file, "")) return;
    wxTreeItemId newFile = DoAddFile(item, file.GetFullPath());
    GetTreeCtrl()->SortChildren(item);
    CallAfter(&clTreeCtrlPanel::SelectItem, newFile);
}

void clTreeCtrlPanel::OnNewFolder(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);
    CHECK_PTR_RET(cd);
    CHECK_COND_RET(cd->IsFolder());

    wxString foldername = ::clGetTextFromUser(_("New Folder"), _("Set the folder name:"), "New Folder");
    if(foldername.IsEmpty()) return; // user cancelled

    wxFileName file(cd->GetPath(), "");
    file.AppendDir(foldername);

    // Create the folder
    wxFileName::Mkdir(file.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    // Add it to the tree view
    wxTreeItemId newFile = DoAddFolder(item, file.GetPath());
    GetTreeCtrl()->SortChildren(item);
    CallAfter(&clTreeCtrlPanel::SelectItem, newFile);
}

void clTreeCtrlPanel::GetSelections(wxArrayString& folders,
                                    wxArrayTreeItemIds& folderItems,
                                    wxArrayString& files,
                                    wxArrayTreeItemIds& fileItems)
{
    folders.clear();
    files.clear();
    folderItems.clear();
    fileItems.clear();

    wxArrayTreeItemIds items;
    if(GetTreeCtrl()->GetSelections(items)) {
        for(size_t i = 0; i < items.size(); ++i) {
            clTreeCtrlData* cd = GetItemData(items.Item(i));
            if(cd) {
                if(cd->IsFile()) {
                    files.Add(cd->GetPath());
                    fileItems.Add(items.Item(i));
                } else if(cd->IsFolder()) {
                    folders.Add(cd->GetPath());
                    folderItems.Add(items.Item(i));
                }
            }
        }
    }
}

void clTreeCtrlPanel::SelectItem(const wxTreeItemId& item)
{
    CHECK_ITEM_RET(item);
    wxArrayTreeItemIds items;
    if(GetTreeCtrl()->GetSelections(items)) {
        for(size_t i = 0; i < items.size(); ++i) {
            GetTreeCtrl()->SelectItem(items.Item(i), false);
        }
    }

    GetTreeCtrl()->SelectItem(item);
    GetTreeCtrl()->EnsureVisible(item);
}

void clTreeCtrlPanel::OnDeleteFile(wxCommandEvent& event)
{
    wxArrayString folders, files;
    wxArrayTreeItemIds folderItems, fileItems;
    GetSelections(folders, folderItems, files, fileItems);
    if(files.empty()) return;

    wxString message;
    message << _("Are you sure you want to delete the selected files?");

    wxRichMessageDialog dialog(EventNotifier::Get()->TopFrame(),
                               message,
                               _("Confirm"),
                               wxYES_NO | wxCANCEL | wxNO_DEFAULT | wxCENTER | wxICON_WARNING);
    dialog.SetYesNoLabels(_("Delete Files"), _("No"));

    wxWindowUpdateLocker locker(GetTreeCtrl());
    wxArrayTreeItemIds deletedItems;
    if(dialog.ShowModal() == wxID_YES) {
        wxLogNull nl;
        for(size_t i = 0; i < files.size(); ++i) {
            if(::wxRemoveFile(files.Item(i))) {
                deletedItems.Add(fileItems.Item(i));
            }
        }
    }

    // Update the UI
    for(size_t i = 0; i < deletedItems.size(); ++i) {
        // Before we delete the item from the tree, update the parent cache
        UpdateItemDeleted(deletedItems.Item(i));
        // And now remove the item from the tree
        GetTreeCtrl()->Delete(deletedItems.Item(i));
    }
}

void clTreeCtrlPanel::OnOpenFile(wxCommandEvent& event)
{
    wxArrayString folders, files;
    GetSelections(folders, files);

    for(size_t i = 0; i < files.size(); ++i) {
        clGetManager()->OpenFile(files.Item(i));
    }
}

void clTreeCtrlPanel::OnRenameFile(wxCommandEvent& event)
{
    wxArrayString files, folders;
    wxArrayTreeItemIds fileItems, folderItems;
    GetSelections(folders, folderItems, files, fileItems);

    if(files.empty()) return;

    // Prompt and rename each file
    for(size_t i = 0; i < files.size(); ++i) {
        wxFileName oldname(files.Item(i));

        wxString newname =
            ::clGetTextFromUser(_("Rename File"), _("New Name:"), oldname.GetFullName(), wxStrlen(oldname.GetName()));
        if(!newname.IsEmpty() && (newname != oldname.GetFullName())) {
            clTreeCtrlData* d = GetItemData(fileItems.Item(i));
            if(d) {
                wxFileName oldpath = d->GetPath();
                wxFileName newpath = oldpath;
                newpath.SetFullName(newname);
                if(::wxRenameFile(oldpath.GetFullPath(), newpath.GetFullPath(), false)) {
                    DoRenameItem(fileItems.Item(i), oldname.GetFullName(), newname);
                }
            }
        }
    }
}

bool clTreeCtrlPanel::ExpandToFile(const wxFileName& filename)
{
    wxArrayString topFolders;
    wxArrayTreeItemIds topFoldersItems;
    GetTopLevelFolders(topFolders, topFoldersItems);

    int where = wxNOT_FOUND;
    wxString fullpath = filename.GetFullPath();
    for(size_t i = 0; i < topFolders.size(); ++i) {
        if(fullpath.StartsWith(topFolders.Item(i))) {
            where = i;
            break;
        }
    }

    // Could not find a folder that matches the filename
    if(where == wxNOT_FOUND) return false;
    wxString topFolder = topFolders.Item(where);
    wxTreeItemId closestItem = topFoldersItems.Item(where);
    fullpath.Remove(0, topFolder.length());
    wxFileName left(fullpath);

    wxArrayString parts = left.GetDirs();
    parts.Add(filename.GetFullName());
    clTreeCtrlData* d = GetItemData(closestItem);
    while(!parts.IsEmpty()) {
        if(!d->GetIndex()) return false; // ??
        wxTreeItemId child = d->GetIndex()->Find(parts.Item(0));
        if(!child.IsOk()) break;
        closestItem = child;
        d = GetItemData(closestItem);
        parts.RemoveAt(0);
    }

    if(parts.IsEmpty()) {
        // we found our file!
        SelectItem(closestItem);
        return true;
    }
    return false;
}

void clTreeCtrlPanel::GetTopLevelFolders(wxArrayString& paths, wxArrayTreeItemIds& items)
{
    wxTreeItemIdValue cookie;
    wxTreeItemId child = GetTreeCtrl()->GetFirstChild(GetTreeCtrl()->GetRootItem(), cookie);
    while(child.IsOk()) {
        clTreeCtrlData* clientData = GetItemData(child);
        paths.Add(clientData->GetPath());
        items.Add(child);
        child = GetTreeCtrl()->GetNextChild(GetTreeCtrl()->GetRootItem(), cookie);
    }
}

void clTreeCtrlPanel::OnActiveEditorChanged(wxCommandEvent& event)
{
    event.Skip();
    if(clGetManager()->GetActiveEditor()) {
        ExpandToFile(clGetManager()->GetActiveEditor()->GetFileName());
    }
}

void clTreeCtrlPanel::UpdateItemDeleted(const wxTreeItemId& item)
{
    wxTreeItemId parent = GetTreeCtrl()->GetItemParent(item);
    CHECK_ITEM_RET(parent);

    clTreeCtrlData* parentData = GetItemData(parent);
    wxString text = GetTreeCtrl()->GetItemText(item);

    // Update the parent cache
    if(parentData->GetIndex()) {
        parentData->GetIndex()->Delete(text);
    }
}

void clTreeCtrlPanel::DoRenameItem(const wxTreeItemId& item, const wxString& oldname, const wxString& newname)
{
    // Update the UI + client data
    clTreeCtrlData* d = GetItemData(item);
    if(d->IsFile()) {
        wxFileName fn(d->GetPath());
        fn.SetFullName(newname);
        d->SetPath(fn.GetFullPath());
    } else if(d->IsFolder()) {
        // FIXME:
    }

    GetTreeCtrl()->SetItemText(item, newname);

    // Update the parent's cache
    wxTreeItemId parent = GetTreeCtrl()->GetItemParent(item);
    CHECK_ITEM_RET(parent);
    clTreeCtrlData* parentData = GetItemData(parent);

    // Update the parent cache
    if(parentData->GetIndex()) {
        parentData->GetIndex()->Delete(oldname);
        parentData->GetIndex()->Add(newname, item);
    }
}

void clTreeCtrlPanel::OnFindInFilesFolder(wxCommandEvent& event)
{
    wxArrayString folders, files;
    GetSelections(folders, files);
    
    if(folders.IsEmpty()) return;
    clGetManager()->OpenFindInFileForPaths(folders);
}

void clTreeCtrlPanel::OnOpenContainingFolder(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);

    if(cd && cd->IsFolder()) {
        FileUtils::OpenFileExplorer(cd->GetPath());
    }
}

void clTreeCtrlPanel::OnOpenShellFolder(wxCommandEvent& event)
{
    wxTreeItemId item = GetTreeCtrl()->GetFocusedItem();
    clTreeCtrlData* cd = GetItemData(item);

    if(cd && cd->IsFolder()) {
        FileUtils::OpenTerminal(cd->GetPath());
    }
}
