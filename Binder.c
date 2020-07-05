#pragma comment(lib, "COMCTL32")
#pragma comment(lib, "SHLWAPI")
#pragma comment(lib, "aplib")

#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <shlwapi.h>
#include <stddef.h>
#include <limits.h>
#include "aplib.h"
#include "camellia.h"
#include "resource.h"

// g (Global optimization), s (Favor small code), y (No frame pointers).
#pragma optimize("gsy", on)
#pragma comment(linker, "/FILEALIGN:0x200")
#pragma comment(linker, "/MERGE:.rdata=.data")
#pragma comment(linker, "/MERGE:.text=.data")
#pragma comment(linker, "/MERGE:.reloc=.data")
#pragma comment(linker, "/SECTION:.text, EWR /IGNORE:4078")
#pragma comment(linker, "/OPT:NOWIN98")		// Make section alignment really small.
#define WIN32_LEAN_AND_MEAN

#define BUF_SIZE 256
#define ENC_LEN  128
/*
 * Unsigned char type.
 */
typedef unsigned char byte;

BOOL		CALLBACK Main(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
void		InitList(void);
BOOL		CALLBACK AddDialogProc(HWND, UINT, WPARAM, LPARAM);
BOOL		ExtractLoader(char *);
int			WriteFiles(int);
int			callback(unsigned int insize, unsigned int inpos, unsigned int outpos, void *cbparam);
unsigned int ratio(unsigned int x, unsigned int y);

HINSTANCE	hInst;
HWND		hwndList;
HANDLE		hLoader;
LONG		run, windir, sysdir, tmpdir;
BOOL		cancel;
int			iIndex, iSelect;

struct file_data{
	char			szFileName[40];
	unsigned long	size;
	unsigned long	actual;
	short			path;
	short			run;
} *pfile_data;

int WINAPI WinMain(HINSTANCE hinstCurrent,HINSTANCE hinstPrevious,LPSTR lpszCmdLine,int nCmdShow){
	INITCOMMONCONTROLSEX icc;
	icc.dwICC= ICC_LISTVIEW_CLASSES;
	icc.dwSize= sizeof(INITCOMMONCONTROLSEX);

	InitCommonControlsEx(&icc);

	hInst= hinstCurrent;
	DialogBox(hinstCurrent, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)Main);
	return 0;
}

BOOL CALLBACK Main(HWND hDlg,UINT uMsg,WPARAM wParam,LPARAM lParam){
	HDC					hdc;
	PAINTSTRUCT			ps;
	LPNMHDR				lpnmhdr;
	LPNMITEMACTIVATE	lpnmitem;
	LVITEM				lvItem;
	LVHITTESTINFO		lvHti;
	HMENU				hMenu;
	POINT				pt;
	OPENFILENAME		ofn;
	char				szFile[MAX_PATH], szSize[15], *szDir = "", szBound[_MAX_FNAME]= "bound";
	HANDLE				hFile;
	DWORD				dwSize;

	switch( uMsg ){
		case WM_INITDIALOG:
			SendMessageA(hDlg,WM_SETICON,ICON_SMALL, (LPARAM) LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON)));
			SendMessageA(hDlg,WM_SETICON, ICON_BIG,(LPARAM) LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON)));
			hwndList= GetDlgItem(hDlg, IDC_LIST);

			ListView_SetExtendedListViewStyle(hwndList, LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP | LVS_EX_GRIDLINES);
			InitList();

			return TRUE;
		case WM_NOTIFY:
			lpnmhdr= (LPNMHDR)lParam;
			if( lpnmhdr->hwndFrom==hwndList ){
				if(lpnmhdr->code == NM_RCLICK) {
					lpnmitem= (LPNMITEMACTIVATE)lParam;
					hMenu= CreatePopupMenu();

					ZeroMemory(&lvHti, sizeof(LVHITTESTINFO));

					lvHti.pt= lpnmitem->ptAction;
					iSelect= ListView_HitTest(hwndList, &lvHti);


					if( lvHti.flags&LVHT_ONITEM ){
						AppendMenu(hMenu, MF_GRAYED | MF_STRING, IDM_ADD, "Add");
						AppendMenu(hMenu, MF_STRING, IDM_REMOVE, "Remove");
					}else{
						AppendMenu(hMenu, MF_STRING, IDM_ADD, "Add");
						AppendMenu(hMenu, MF_GRAYED | MF_STRING, IDM_REMOVE, "Remove");
					}

					AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

					if( iIndex<2 ){
						AppendMenu(hMenu, MF_GRAYED | MF_STRING, IDM_BIND, "Bind");
					}else{
						AppendMenu(hMenu, MF_STRING, IDM_BIND, "Bind");
					}
					GetCursorPos(&pt);
					TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hDlg, 0);
				}
			}
			DestroyMenu(hMenu);
			return TRUE;
		case WM_COMMAND:
			switch( LOWORD(wParam) ){
				case IDM_ADD:
					ZeroMemory(&ofn, sizeof(OPENFILENAME));
					ZeroMemory(szFile, sizeof szFile);
					ZeroMemory(szDir, sizeof szDir);

					ofn.lStructSize= sizeof(OPENFILENAME);
					ofn.hwndOwner= hDlg;
					ofn.lpstrFilter= "All Files (*.*)\0*.*\0";
					ofn.lpstrFile= szFile;
					ofn.nMaxFile= MAX_PATH;
					ofn.Flags= OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

					if( GetOpenFileName(&ofn) ){
						cancel= FALSE;
						DialogBox(hInst, MAKEINTRESOURCE(IDD_ADD), hDlg, AddDialogProc);

						if( !cancel ){
							lvItem.mask= LVIF_TEXT;
							lvItem.cchTextMax= MAX_PATH;

							lvItem.iItem= iIndex;
							lvItem.iSubItem= 0;
							lvItem.pszText= szFile;
							ListView_InsertItem(hwndList, &lvItem);

							hFile= CreateFile(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

							if( hFile==INVALID_HANDLE_VALUE ){
								return FALSE;
							}
							dwSize= GetFileSize(hFile, NULL);
							CloseHandle(hFile);
							wsprintf(szSize, "%d KB", dwSize / 1024);

							lvItem.iItem= iIndex;
							lvItem.iSubItem= 1;
							lvItem.pszText= szSize;

							ListView_SetItem(hwndList, &lvItem);

							lvItem.iItem= iIndex;
							lvItem.iSubItem= 2;

							if( windir==BST_CHECKED ){
								szDir= "Windows";
							}else if( sysdir==BST_CHECKED ){
								szDir= "System";
							}else{
								szDir= "Temporary";
							}

							lvItem.pszText= szDir;
							ListView_SetItem(hwndList, &lvItem);

							lvItem.iItem= iIndex;
							lvItem.iSubItem= 3;
							
							if( run==BST_CHECKED ){
								lvItem.pszText= "Yes";
							}else{
								lvItem.pszText= "No";
							}
							ListView_SetItem(hwndList, &lvItem);
							iIndex++;
						}
					}

					return TRUE;
				case IDM_REMOVE:
					ListView_DeleteItem(hwndList, iSelect);
					iIndex--;
					return TRUE;
				case IDM_BIND:
					ZeroMemory(&ofn, sizeof(OPENFILENAME));

					ofn.lStructSize= sizeof(OPENFILENAME);
					ofn.hwndOwner= hDlg;
					ofn.lpstrFilter= "Application (*.exe)\0*.exe\0";
					ofn.lpstrFile= szBound;
					ofn.lpstrDefExt= "exe";
					ofn.nMaxFile= MAX_PATH;
					ofn.Flags= OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if( GetSaveFileName(&ofn) ){
						if(!ExtractLoader(szBound)){
							return FALSE;
						}
						if( !WriteFiles(iIndex) ){
							MessageBox(hDlg, "Error writing files.", NULL, MB_OK);
							CloseHandle(hLoader);
							return FALSE;
						}
					}
					return TRUE;
			}
			case WM_PAINT:
				hdc= BeginPaint(hDlg, &ps);
				InvalidateRect(hDlg, NULL, TRUE);
				EndPaint (hDlg, &ps);
			break;
			case WM_CLOSE:
				EndDialog(hDlg,wParam);
				DestroyWindow(hDlg);
			break;
			case WM_DESTROY:
				PostQuitMessage(0);
			break;
	}
	return FALSE;
}

void InitList( void ){
	LVCOLUMN lvCol;
	char *szColumn[]= {"File", "Size", "Installation Directory", "Run"};
	int i, width[]= {220, 55, 160, 35};

	ZeroMemory(&lvCol, sizeof(LVCOLUMN));

	lvCol.mask= LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH | LVCF_FMT;
	lvCol.fmt= LVCFMT_LEFT;

	for( i=0; i<4; i++ ){
		lvCol.iSubItem= i;
		lvCol.cx= width[i];
		lvCol.pszText= szColumn[i];

		ListView_InsertColumn(hwndList, i, &lvCol);
	}
}

BOOL CALLBACK AddDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ){
	switch(uMsg) {
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					run= SendDlgItemMessage(hwndDlg, IDC_CHECK_RUN, BM_GETCHECK, wParam, lParam);
				
					windir= SendDlgItemMessage(hwndDlg, IDC_RADIO_WINDIR, BM_GETCHECK, wParam, lParam);
					sysdir= SendDlgItemMessage(hwndDlg, IDC_RADIO_SYSDIR, BM_GETCHECK, wParam, lParam);
					tmpdir= SendDlgItemMessage(hwndDlg, IDC_RADIO_TMPDIR, BM_GETCHECK, wParam, lParam);

					if( windir!=BST_CHECKED && sysdir!=BST_CHECKED && tmpdir!=BST_CHECKED ){
						MessageBox(hwndDlg, "You have not selected an installation directory.", NULL, MB_ICONERROR | MB_OK);
					}else{
						EndDialog(hwndDlg, 0);
					}
					break;
				case IDCANCEL:
					cancel= TRUE;
					EndDialog(hwndDlg, 0);
					break;
			}
			break;
		case WM_CLOSE:
			cancel= TRUE;
			EndDialog(hwndDlg, 0);
			break;
	}
	return FALSE;
}

BOOL ExtractLoader( char *szLoc ){
	HRSRC			rc;
	HGLOBAL			hGlobal;
	HMODULE			hThisProc;
	DWORD			dwSize, dwBytesWritten;
	unsigned char	*lpszData;

	hThisProc= GetModuleHandle(NULL);
	rc= FindResource(hThisProc, MAKEINTRESOURCE(IDR_RT_EXE), "RT_EXE");

	if(hGlobal = LoadResource(hThisProc, rc)) {
		lpszData= (unsigned char *)LockResource(hGlobal);
		dwSize= SizeofResource(hThisProc, rc);
		hLoader= CreateFile(szLoc, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if( hLoader==INVALID_HANDLE_VALUE ){
			return FALSE;
		}else{
			WriteFile(hLoader, lpszData, dwSize, &dwBytesWritten, NULL);
		}
	}
	if( dwBytesWritten!=dwSize ){
		MessageBox(NULL, "Error writing stub file.", NULL, MB_ICONERROR | MB_OK);
		return FALSE;
	}else{
		return TRUE;
	}
}

int WriteFiles( int nFileNum ){
	int					i, temp;
	HANDLE				hFile;
	DWORD				dwStart, dwBytesWritten, dwBytesRead, dwSize;
	char				szPath[MAX_PATH], szDir[10], szExec[4], done[40];
	struct file_data	fd;
	KEY_TABLE_TYPE		keyTable;
	unsigned char		rawKey[]= {0x52, 0x45, 0xA9, 0x7F, 0x3B, 0xA4, 0xC2, 0xE5,
									0x00, 0x42, 0x8C, 0xEF, 0x70, 0xC1, 0x5C, 0xC2,
									0xBA, 0xB7, 0x9D, 0x95, 0x69, 0xE8, 0xFF, 0xB3,
									0x21, 0xA2, 0x36, 0x8D, 0xFF, 0xDD, 0xE2, 0x79};
	byte				*data, *packed, *workmem;
	size_t				outsize;

	pfile_data= &fd;
	dwStart= GetTickCount();

	srand(dwStart);

	for( i=0; i<nFileNum; i++ ){
		ZeroMemory(&fd, sizeof fd);
		ListView_GetItemText(hwndList, i, 0, szPath, MAX_PATH);

		hFile= CreateFile(szPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if( hFile==INVALID_HANDLE_VALUE ){
			return 0;
		}
		dwSize= GetFileSize(hFile, NULL);
		// Start of aPLib compression.
		/* allocate memory */
		if( (data= (byte *)malloc(dwSize))==NULL || (packed= (byte *)malloc(aP_max_packed_size(dwSize)))==NULL || (workmem= (byte *)malloc(aP_workmem_size(dwSize)))==NULL ){
			MessageBox(NULL, "ERROR: Not enough memory.", "Memory not Enough Error.", MB_OK);
			return 1;
		}
		memset(data, 0, dwSize);
		ReadFile( hFile, data, dwSize , &dwBytesRead, NULL );
		if( dwBytesRead!=dwSize ){
			MessageBox(NULL, "ERROR: Error reading from input file.", "InputFile Error.", MB_OK);
			return 1;
		}
		// Compress data block.
		outsize= aPsafe_pack(data, packed, dwSize, workmem, callback, NULL);

		if( outsize==APLIB_ERROR ){
			MessageBox(NULL, "ERROR: An error occured while compressing.", "Compression Error.", MB_OK);
			return 1;
		}
		dwSize= outsize;
		// End of aPLib compression.
		pfile_data->actual= dwSize;
		
		temp= dwSize%(ENC_LEN/8);
		dwSize += (ENC_LEN/8 - temp);
		
		pfile_data->size= dwSize;

		strcpy(pfile_data->szFileName, PathFindFileName(szPath));

		ListView_GetItemText(hwndList, i, 2, szDir, sizeof szDir);

		if( !strcmp(szDir, "System") ){
			pfile_data->path= 1;
		}else if( !strcmp(szDir, "Temporary") ){
			pfile_data->path= 2;
		}else{
			pfile_data->path= 3;
		}
		ListView_GetItemText(hwndList, i, 3, szExec, sizeof szExec);
		pfile_data->run= strcmp(szExec, "Yes") == 0 ? 1 : 0;

		Camellia_Ekeygen(128, rawKey, keyTable);

		SetFilePointer(hLoader, 0, NULL, FILE_END);
		WriteFile(hLoader, pfile_data, sizeof fd, &dwBytesWritten, NULL);

		temp= 0;

		while( temp<dwSize  ){
			// Debugging Camellia.
			char *plaintext= packed;
			unsigned char *enc_data= malloc(ENC_LEN/8);
			enc_data[0]= 0;
			plaintext += temp;
			
			Camellia_EncryptBlock(ENC_LEN, plaintext, keyTable, enc_data);
			WriteFile(hLoader, enc_data, ENC_LEN/8, &dwBytesWritten, NULL);
			// End Debugging for Camellia.
			if( dwBytesWritten!=dwBytesRead ){
				//return 0;
			}
			free(enc_data);
			enc_data= NULL;
			temp += ENC_LEN /8;
		}
		free(workmem);
		free(packed);
	    free(data);
		CloseHandle(hFile);
	}
	if( i==nFileNum ){
		wsprintf(done, "%d Files bound in %d second(s).", nFileNum, (GetTickCount() - dwStart) / 1000);
		MessageBox(NULL, done, "Finished.", MB_OK);
	}else{
		return 0;
	}
	CloseHandle(hLoader);
	return i;
}

// Compute ratio between two numbers.
unsigned int ratio(unsigned int x, unsigned int y){
    if( x<= UINT_MAX/100 )
		x *= 100;
	else
		y /= 100;

    if( y==0 )
		y= 1;

    return x / y;
}

// Compression callback.
int callback(unsigned int insize, unsigned int inpos, unsigned int outpos, void *cbparam){
   return 1;
}