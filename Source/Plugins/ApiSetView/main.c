/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2019 - 2021
*
*  TITLE:       MAIN.H
*
*  VERSION:     1.13
*
*  DATE:        01 Oct 2021
*
*  WinObjEx64 ApiSetView plugin.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

//
// Plugin entry.
//
WINOBJEX_PLUGIN* g_Plugin = NULL;

HINSTANCE g_ThisDLL = NULL;
GUI_CONTEXT g_ctx;

volatile DWORD m_PluginState = PLUGIN_RUNNING;

/*
* ClipboardCopy
*
* Purpose:
*
* Copy text to the clipboard.
*
*/
VOID ClipboardCopy(
    _In_ LPWSTR lpText,
    _In_ SIZE_T cbText
)
{
    LPWSTR  lptstrCopy;
    HGLOBAL hglbCopy;
    SIZE_T  dwSize;

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        dwSize = cbText + sizeof(UNICODE_NULL);
        hglbCopy = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dwSize);
        if (hglbCopy != NULL) {
            lptstrCopy = (LPWSTR)GlobalLock(hglbCopy);
            if (lptstrCopy) {
                RtlCopyMemory(lptstrCopy, lpText, cbText);
            }
            GlobalUnlock(hglbCopy);
            if (!SetClipboardData(CF_UNICODETEXT, hglbCopy))
                GlobalFree(hglbCopy);
        }
        CloseClipboard();
    }
}

/*
* TreeListCopyItemValueToClipboard
*
* Purpose:
*
* Copy selected treelist item text to the clipboard.
*
*/
BOOL TreeListCopyItemValueToClipboard(
    _In_ HWND hwndTreeList,
    _In_ INT tlSubItemHit
)
{
    INT         nIndex;
    LPWSTR      lpCopyData = NULL;
    SIZE_T      cbCopyData = 0;
    TVITEMEX    itemex;
    WCHAR       szText[MAX_PATH + 1];

    TL_SUBITEMS_FIXED* pSubItems = NULL;

    szText[0] = 0;
    RtlSecureZeroMemory(&itemex, sizeof(itemex));
    itemex.mask = TVIF_TEXT;
    itemex.hItem = TreeList_GetSelection(hwndTreeList);
    itemex.pszText = szText;
    itemex.cchTextMax = MAX_PATH;

    if (TreeList_GetTreeItem(hwndTreeList, &itemex, &pSubItems)) {

        if ((tlSubItemHit > 0) && (pSubItems != NULL)) {

            nIndex = (tlSubItemHit - 1);
            if (nIndex < (INT)pSubItems->Count) {

                lpCopyData = pSubItems->Text[nIndex];
                cbCopyData = _strlen(lpCopyData) * sizeof(WCHAR);

            }

        }
        else {
            if (tlSubItemHit == 0) {
                lpCopyData = szText;
                cbCopyData = sizeof(szText);
            }
        }

        if (lpCopyData && cbCopyData) {
            ClipboardCopy(lpCopyData, cbCopyData);
            return TRUE;
        }
        else {
            if (OpenClipboard(NULL)) {
                EmptyClipboard();
                CloseClipboard();
            }
        }
    }

    return FALSE;
}


/*
* TreeListAddCopyValueItem
*
* Purpose:
*
* Add copy to clipboard menu item depending on hit treelist header item.
*
*/
BOOL TreeListAddCopyValueItem(
    _In_ HMENU hMenu,
    _In_ HWND hwndTreeList,
    _In_ UINT uId,
    _In_opt_ UINT uPos,
    _In_ LPARAM lParam,
    _In_ INT* pSubItemHit
)
{
    HDHITTESTINFO hti;
    HD_ITEM hdItem;
    WCHAR szHeaderText[MAX_PATH + 1];
    WCHAR szItem[MAX_PATH * 2];

    *pSubItemHit = -1;

    hti.iItem = -1;
    hti.pt.x = LOWORD(lParam);
    hti.pt.y = HIWORD(lParam);
    ScreenToClient(hwndTreeList, &hti.pt);

    hti.pt.y = 1;
    if (TreeList_HeaderHittest(hwndTreeList, &hti) < 0)
        return FALSE;

    RtlSecureZeroMemory(&hdItem, sizeof(hdItem));

    szHeaderText[0] = 0;
    hdItem.mask = HDI_TEXT;

    hdItem.cchTextMax = sizeof(szHeaderText) - 1;

    hdItem.pszText = szHeaderText;
    if (TreeList_GetHeaderItem(hwndTreeList, hti.iItem, &hdItem)) {
        *pSubItemHit = hti.iItem;

        _strcpy(szItem, TEXT("Copy \""));
        _strcat(szItem, szHeaderText);
        _strcat(szItem, TEXT("\""));
        if (InsertMenu(hMenu, uPos, MF_BYCOMMAND, uId, szItem)) {
            return TRUE;
        }
    }

    return FALSE;
}

/*
* ContextMenuHandler
*
* Purpose:
*
* Main list context menu handler.
*
*/
VOID ContextMenuHandler(
    _In_ HWND hwndDlg,
    _In_ HWND hwndTreeList,
    _In_ LPARAM lParam,
    _Inout_ INT* pSubItemHit
)
{
    POINT pt1;
    HMENU hMenu;
    INT uPos = 0;

    if (GetCursorPos(&pt1) == FALSE)
        return;

    hMenu = CreatePopupMenu();
    if (hMenu) {

        if (TreeListAddCopyValueItem(hMenu,
            hwndTreeList,
            ID_OBJECT_COPY,
            uPos++,
            lParam,
            pSubItemHit))
        {
            InsertMenu(hMenu, uPos++, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        }
        InsertMenu(hMenu, uPos++, MF_BYCOMMAND, IDC_BROWSE_BUTTON, TEXT("Select Schema File"));
        InsertMenu(hMenu, uPos++, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, uPos++, MF_BYCOMMAND, ID_USE_SYSTEM_SCHEMA_FILE, TEXT("Use System Schema"));
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt1.x, pt1.y, 0, hwndDlg, NULL);
        DestroyMenu(hMenu);
    }

}

/*
* OpenDialogExecute
*
* Purpose:
*
* Display OpenDialog
*
*/
BOOL OpenDialogExecute(
    _In_ HWND OwnerWindow,
    _Inout_ LPWSTR OpenFileName,
    _In_ LPWSTR lpDialogFilter
)
{
    OPENFILENAME tag1;

    RtlSecureZeroMemory(&tag1, sizeof(OPENFILENAME));

    tag1.lStructSize = sizeof(OPENFILENAME);
    tag1.hwndOwner = OwnerWindow;
    tag1.lpstrFilter = lpDialogFilter;
    tag1.lpstrFile = OpenFileName;
    tag1.nMaxFile = MAX_PATH;
    tag1.lpstrInitialDir = NULL;
    tag1.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&tag1);
}

/*
* PluginHandleWMNotify
*
* Purpose:
*
* Main window WM_NOTIFY handler.
*
*/
VOID PluginHandleWMNotify(
    _In_ HWND   hwndDlg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    TL_SUBITEMS_FIXED* subitems;
    LPNMHDR hdr = (LPNMHDR)lParam;

    UNREFERENCED_PARAMETER(wParam);

    HWND TreeList = (HWND)TreeList_GetTreeControlWindow(g_ctx.TreeList);

    TVITEMEX tvi;
    WCHAR szBuffer[MAX_PATH + 1];

    if (hdr->hwndFrom == TreeList) {
        switch (hdr->code) {
        case TVN_ITEMEXPANDED:
        case TVN_SELCHANGED:
            RtlSecureZeroMemory(&tvi, sizeof(tvi));
            RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));

            tvi.mask = TVIF_TEXT;
            tvi.pszText = szBuffer;
            tvi.hItem = TreeList_GetSelection(g_ctx.TreeList);
            tvi.cchTextMax = MAX_PATH;
            if (TreeList_GetTreeItem(g_ctx.TreeList, &tvi, &subitems)) {
                SendDlgItemMessage(
                    hwndDlg,
                    IDC_ENTRY_EDIT,
                    WM_SETTEXT,
                    (WPARAM)0,
                    (LPARAM)&szBuffer);
            }

            break;
        default:
            break;
        }
    }
}

/*
* CenterWindow
*
* Purpose:
*
* Centers given window relative to desktop window.
*
*/
VOID CenterWindow(
    _In_ HWND hwnd
)
{
    RECT rc, rcDlg, rcOwner;
    HWND hwndParent = GetDesktopWindow();

    if (hwndParent) {
        GetWindowRect(hwndParent, &rcOwner);
        GetWindowRect(hwnd, &rcDlg);
        CopyRect(&rc, &rcOwner);
        OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
        OffsetRect(&rc, -rc.left, -rc.top);
        OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

        //
        // Center window
        //
        SetWindowPos(hwnd,
            HWND_TOP,
            rcOwner.left + (rc.right / 2),
            rcOwner.top + (rc.bottom / 2),
            0, 0,
            SWP_NOSIZE);
    }
}

/*
* InitTreeList
*
* Purpose:
*
* Intialize TreeList control.
*
*/
BOOL InitTreeList(
    _In_ HWND hwndParent,
    _Out_ HWND* pTreeListHwnd
)
{
    HWND TreeList;
    HDITEM hdritem;
    RECT rc;

    UINT uDpi;
    INT dpiScaledX, dpiScaledY, iScaledWidth, iScaledHeight, iScaleSubX, iScaleSubY;

    if (pTreeListHwnd == NULL) {
        return FALSE;
    }

    uDpi = g_ctx.ParamBlock.CurrentDPI;
    dpiScaledX = MulDiv(10, uDpi, DefaultSystemDpi);
    dpiScaledY = dpiScaledX;

    GetWindowRect(hwndParent, &rc);

    iScaleSubX = MulDiv(24, uDpi, DefaultSystemDpi);
    iScaleSubY = MulDiv(140, uDpi, DefaultSystemDpi);
    iScaledWidth = (rc.right - rc.left) - dpiScaledX - iScaleSubX;
    iScaledHeight = (rc.bottom - rc.top) - dpiScaledY - iScaleSubY;

    TreeList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREELIST, NULL,
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | TLSTYLE_COLAUTOEXPAND | TLSTYLE_LINKLINES,
        dpiScaledX, dpiScaledY,
        iScaledWidth, iScaledHeight, hwndParent, NULL, NULL, NULL);

    if (TreeList == NULL) {
        *pTreeListHwnd = NULL;
        return FALSE;
    }

    *pTreeListHwnd = TreeList;

    RtlSecureZeroMemory(&hdritem, sizeof(hdritem));
    hdritem.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
    hdritem.fmt = HDF_LEFT | HDF_BITMAP_ON_RIGHT | HDF_STRING;

    hdritem.cxy = 340;
    hdritem.pszText = TEXT("Namespace");
    TreeList_InsertHeaderItem(TreeList, 0, &hdritem);

    hdritem.cxy = 130;
    hdritem.pszText = TEXT("Flags");
    TreeList_InsertHeaderItem(TreeList, 1, &hdritem);

    hdritem.cxy = 200;
    hdritem.pszText = TEXT("Alias");
    TreeList_InsertHeaderItem(TreeList, 2, &hdritem);

    return TRUE;
}

/*
* PluginDialogProc
*
* Purpose:
*
* Main plugin window procedure.
*
*/
INT_PTR CALLBACK PluginDialogProc(
    _In_ HWND   hwndDlg,
    _In_ UINT   uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    HANDLE hIcon;
    HTREEITEM hRoot;
    LPWSTR lpFilter = NULL;
    WCHAR szFileName[MAX_PATH + 1];
    WCHAR szFilterOption[MAX_PATH + 1];

    switch (uMsg) {

    case WM_INITDIALOG:

        g_ctx.MainWindow = hwndDlg;
        g_ctx.SearchEdit = GetDlgItem(hwndDlg, IDC_SEARCH_EDIT);
        g_ctx.tlSubItemHit = -1;

        hIcon = LoadImage(
            g_ctx.ParamBlock.Instance,
            MAKEINTRESOURCE(WINOBJEX64_ICON_MAIN),
            IMAGE_ICON,
            32, 32,
            0);

        if (hIcon) {
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon);
            g_ctx.WindowIcon = hIcon;

        }

        if (InitTreeList(hwndDlg, &g_ctx.TreeList)) {
            ListApiSetFromFile(NULL, NULL);
        }
        else {
            MessageBox(g_ctx.MainWindow,
                TEXT("ApiSetView: Could not initialize treelist window"),
                NULL,
                MB_ICONERROR);
        }

        break;

    case WM_CLOSE:
        InterlockedExchange((PLONG)&m_PluginState, PLUGIN_STOP);
        PostQuitMessage(0);
        break;

    case WM_SHOWWINDOW:
        if (wParam) {
            CenterWindow(hwndDlg);
            SendDlgItemMessage(hwndDlg, IDC_SEARCH_EDIT, EM_LIMITTEXT, MAX_PATH, 0);
            hRoot = TreeList_GetRoot(g_ctx.TreeList);
            TreeList_EnsureVisible(g_ctx.TreeList, hRoot);
            SetFocus(g_ctx.TreeList);
        }
        break;

    case WM_CONTEXTMENU:

        ContextMenuHandler(hwndDlg,
            g_ctx.TreeList,
            lParam,
            &g_ctx.tlSubItemHit);

        break;

    case WM_NOTIFY:
        PluginHandleWMNotify(
            hwndDlg,
            wParam,
            lParam);
        break;

    case WM_COMMAND:

        switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case IDC_SEARCH_EDIT:
            if (GET_WM_COMMAND_CMD(wParam, lParam) == EN_CHANGE) {

                RtlSecureZeroMemory(szFilterOption, sizeof(szFilterOption));
                if (GetWindowText(
                    g_ctx.SearchEdit,
                    szFilterOption,
                    MAX_PATH))
                {
                    if (szFilterOption[0] != 0) {
                        lpFilter = szFilterOption;
                    }
                }

                ListApiSetFromFile(g_ctx.SchemaFileName, lpFilter);

            }
            break;

        case ID_USE_SYSTEM_SCHEMA_FILE:
            ListApiSetFromFile(NULL, NULL);
            break;

        case IDC_BROWSE_BUTTON:

            RtlSecureZeroMemory(szFileName, sizeof(szFileName));
            if (OpenDialogExecute(hwndDlg,
                szFileName,
                TEXT("All files\0*.*\0\0")))
            {
                SetWindowText(g_ctx.SearchEdit, TEXT(""));
                _strcpy(g_ctx.SchemaFileName, szFileName);

                /*
                szFilterOption[0] = 0;
                if (GetWindowText(
                    g_ctx.SearchEdit,
                    szFilterOption,
                    MAX_PATH))
                {
                    if (szFilterOption[0] != 0) {
                        lpFilter = szFilterOption;
                    }
                }
                */
                ListApiSetFromFile(g_ctx.SchemaFileName, NULL);
            }
            break;

        case IDOK:
        case IDCANCEL:
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            return TRUE;

        case ID_OBJECT_COPY:

            TreeListCopyItemValueToClipboard(g_ctx.TreeList,
                g_ctx.tlSubItemHit);

            break;


        default:
            break;
        }

        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/*
* PluginFreeGlobalResources
*
* Purpose:
*
* Plugin resources deallocation routine.
*
*/
VOID PluginFreeGlobalResources()
{
    if (g_ctx.WindowIcon)
        DestroyIcon(g_ctx.WindowIcon);

    if (g_ctx.PluginHeap) {
        HeapDestroy(g_ctx.PluginHeap);
        g_ctx.PluginHeap = NULL;
    }

    if (g_Plugin->StateChangeCallback)
        g_Plugin->StateChangeCallback(g_Plugin, PluginStopped, NULL);
}

/*
* PluginThread
*
* Purpose:
*
* Plugin payload thread.
*
*/
DWORD WINAPI PluginThread(
    _In_ PVOID Parameter
)
{
    UNREFERENCED_PARAMETER(Parameter);

    INITCOMMONCONTROLSEX icex;

    BOOL rv;
    MSG msg1;

    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    CreateDialogParam(g_ThisDLL,
        MAKEINTRESOURCE(IDD_ASDIALOG),
        NULL,
        PluginDialogProc,
        0);

    do {
        rv = GetMessage(&msg1, NULL, 0, 0);

        if (rv == -1)
            break;

        TranslateMessage(&msg1);
        DispatchMessage(&msg1);

    } while (rv != 0 && InterlockedAdd((PLONG)&m_PluginState, PLUGIN_RUNNING) == PLUGIN_RUNNING);

    PluginFreeGlobalResources();

    ExitThread(0);
}

/*
* StartPlugin
*
* Purpose:
*
* Run actual plugin code in dedicated thread.
*
*/
NTSTATUS CALLBACK StartPlugin(
    _In_ PWINOBJEX_PARAM_BLOCK ParamBlock
)
{
    DWORD ThreadId;
    NTSTATUS Status;
    WINOBJEX_PLUGIN_STATE State = PluginInitialization;

    InterlockedExchange((PLONG)&m_PluginState, PLUGIN_RUNNING);

    RtlSecureZeroMemory(&g_ctx, sizeof(g_ctx));

    g_ctx.PluginHeap = HeapCreate(0, 0, 0);
    if (g_ctx.PluginHeap) {

        HeapSetInformation(g_ctx.PluginHeap, HeapEnableTerminationOnCorruption, NULL, 0);

        RtlCopyMemory(&g_ctx.ParamBlock, ParamBlock, sizeof(WINOBJEX_PARAM_BLOCK));

        g_ctx.WorkerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PluginThread, (PVOID)NULL, 0, &ThreadId);
        if (g_ctx.WorkerThread) {
            Status = STATUS_SUCCESS;
        }
        else {
            Status = STATUS_UNSUCCESSFUL;
            HeapDestroy(g_ctx.PluginHeap);
            g_ctx.PluginHeap = NULL;
        }

        if (NT_SUCCESS(Status))
            State = PluginRunning;
        else
            State = PluginError;

        if (g_Plugin->StateChangeCallback)
            g_Plugin->StateChangeCallback(g_Plugin, State, NULL);

    }
    else {
        Status = STATUS_MEMORY_NOT_ALLOCATED;
    }

    return Status;
}

/*
* StopPlugin
*
* Purpose:
*
* Stop plugin execution.
*
*/
void CALLBACK StopPlugin(
    VOID
)
{
    if (g_ctx.WorkerThread) {
        InterlockedExchange((PLONG)&m_PluginState, PLUGIN_STOP);//force stop
        if (WaitForSingleObject(g_ctx.WorkerThread, 1000) == WAIT_TIMEOUT) {
            TerminateThread(g_ctx.WorkerThread, 0);
        }
        CloseHandle(g_ctx.WorkerThread);
        g_ctx.WorkerThread = NULL;

        if (g_Plugin->StateChangeCallback)
            g_Plugin->StateChangeCallback(g_Plugin, PluginStopped, NULL);
    }
}

/*
* PluginInit
*
* Purpose:
*
* Initialize plugin information for WinObjEx64.
*
*/
BOOLEAN CALLBACK PluginInit(
    _Inout_ PWINOBJEX_PLUGIN PluginData
)
{
    if (g_Plugin)
        return FALSE;

    __try {
        //
        // Set plugin name to be displayed in WinObjEx64 UI.
        //
        StringCbCopy(PluginData->Name, sizeof(PluginData->Name), TEXT("ApiSetSchema Viewer"));

        //
        // Set authors.
        //
        StringCbCopy(PluginData->Authors, sizeof(PluginData->Authors), TEXT("UG North"));

        //
        // Set plugin description.
        //
        StringCbCopy(PluginData->Description, sizeof(PluginData->Description),
            TEXT("A simple viewer for ApiSet schema."));

        //
        // Set required plugin system version.
        //
        PluginData->RequiredPluginSystemVersion = WOBJ_PLUGIN_SYSTEM_VERSION;

        //
        // Setup start/stop plugin callbacks.
        //
        PluginData->StartPlugin = (pfnStartPlugin)&StartPlugin;
        PluginData->StopPlugin = (pfnStopPlugin)&StopPlugin;

        //
        // Setup permissions.
        //
        PluginData->NeedAdmin = FALSE;
        PluginData->SupportWine = FALSE;
        PluginData->NeedDriver = FALSE;

        PluginData->MajorVersion = 1;
        PluginData->MinorVersion = 1;

        //
        // Set plugin type.
        //
        PluginData->Type = DefaultPlugin;

        g_Plugin = PluginData;

        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DbgPrint("PluginInit exception thrown %lx\r\n", GetExceptionCode());
        return FALSE;
    }
}

/*
* DllMain
*
* Purpose:
*
* Dummy dll entrypoint.
*
*/
BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_ThisDLL = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
